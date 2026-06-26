# STAX Complete System Flow

This document is a step-by-step walkthrough of every stage of STAX — from typing `make qemu` to the interactive shell running in QEMU. Nothing is skipped.

---

## Stage 0 — The Build System (`make qemu`)

Before anything runs, the `Makefile` assembles all the pieces.

### Step 0.1 — Compile the Bootloader

```
arm-none-eabi-gcc → boot/boot_startup.s   → build/boot_startup.o
arm-none-eabi-gcc → boot/bootloader.c     → build/bootloader.o
```

- `bootloader.ld.in` is preprocessed by GCC (`-E -P -x c`) into `build/bootloader.ld`.  
  The preprocessor resolves `#include "memory_map.h"` so linker constants like `BOOTLOADER_EXEC_ADDR = 0x10000` and `BOOTLOADER_STACK = 0x80000` are available as raw numbers.
- `arm-none-eabi-ld` links `boot_startup.o` + `bootloader.o` → `build/bootloader.elf` using the linker script that places everything at `0x10000`.
- `arm-none-eabi-objcopy -O binary` strips ELF headers → `build/bootloader.bin` (pure machine code, ~2 KB).

### Step 0.2 — Compile the Kernel

All kernel/driver/fs/mm source files are compiled into `.o` objects:

```
kernel/startup.s    → build/startup.o      (ARM entry point, BSS zero, vector copy)
kernel/vectors.s    → build/vectors.o      (vector table + IRQ stub + context switch)
kernel/kernel.c     → build/kernel.o       (kernel_main, timer ISR)
drivers/vic.c       → build/vic.o          (VIC interrupt controller)
drivers/timer.c     → build/timer.o        (SP804 timer)
kernel/scheduler.c  → build/scheduler.o    (round-robin TCB scheduler)
mm/heap.c           → build/heap.o         (bump allocator + free list)
fs/fat.c            → build/fat.o          (FAT12/16 driver for kernel)
fs/disk.c           → build/disk.o         (disk abstraction stub)
drivers/irq.c       → build/irq.o          (IRQ register/dispatch)
drivers/console.c   → build/console.o      (UART serial I/O)
kernel/command.c    → build/command.o      (interactive shell commands)
kernel/tasks.c      → build/tasks.o        (demo tasks)
```

- `linker.ld.in` is preprocessed → `build/linker.ld`, resolving `KERNEL_EXEC_ADDR = 0x100000` and `4M` RAM length.
- The linker links all `.o` files in the **exact order** listed (startup.o must be first so `_start` lands at `0x100000`).
- `objcopy` produces `build/kernel.bin` — the raw binary to be placed on the SD card.

### Step 0.3 — Assemble the FAT16 SD Card Image (`os.bin`)

```bash
dd if=/dev/zero of=os.bin bs=1M count=16   # Create 16 MB blank file
mkfs.vfat -F 16 os.bin                      # Format it as FAT16
mcopy -i os.bin build/kernel.bin ::/KERNEL.BIN  # Copy kernel into the root dir
```

**What this creates on disk:**

```
os.bin (16 MB FAT16 image):
┌──────────────────────────────────────────────────────────┐
│ Sector 0   : FAT16 Boot Sector (BPB — filesystem params) │
│ Sectors 1-N: FAT Table(s) — cluster chain map            │
│ Sectors N+ : Root Directory — contains KERNEL.BIN entry  │
│ Data Region: KERNEL.BIN actual binary clusters           │
└──────────────────────────────────────────────────────────┘
```

The bootloader is NOT in this image. It is a completely separate binary.

### Step 0.4 — Launch QEMU

```bash
qemu-system-arm -M versatilepb \
  -kernel build/bootloader.bin \
  -drive file=os.bin,if=sd,format=raw \
  -nographic -serial mon:stdio
```

| Flag | What it does |
|---|---|
| `-M versatilepb` | Emulates the ARM Versatile Platform Baseboard (ARM926EJ-S CPU, 256 MB RAM) |
| `-kernel bootloader.bin` | QEMU reads `bootloader.bin` and **copies it directly into guest RAM at `0x10000`**, then sets the CPU Program Counter to `0x10000` |
| `-drive file=os.bin,if=sd` | Exposes `os.bin` as an SD card accessible via the PL181 MMC/SD controller at `0x10005000` |
| `-nographic -serial mon:stdio` | Redirects the PL011 UART serial output to your terminal |

> **Key Point:** QEMU does not read the bootloader from the SD card. It injects it directly into RAM. The CPU never "boots from disk" in the traditional sense here.

---

## Stage 1 — Bootloader Execution (`boot_startup.s` → `bootloader.c`)

The CPU Program Counter is now at `0x10000`. The first instruction executed is in `boot_startup.s`.

### Step 1.1 — `_start` in `boot_startup.s`

```asm
_start:
    ldr sp, =BOOTLOADER_STACK     @ Set stack pointer to 0x80000 (512 KB mark, grows down)

    ldr r0, =__bss_start          @ Zero out bootloader's .bss segment
    ldr r1, =__bss_end
    mov r2, #0
1:  cmp r0, r1
    bge 2f
    str r2, [r0], #4
    b 1b
2:
    bl bootloader_main            @ Jump to C
hang: b hang                      @ Safety hang if C ever returns
```

Three things happen:
1. Stack pointer is placed at `0x80000` (grows downward toward `0x10000`).
2. BSS is zeroed — any global variables in the bootloader start at 0.
3. `bootloader_main()` is called.

### Step 1.2 — `bootloader_main()` in `bootloader.c`

#### Phase A: UART Initialisation
The PL011 UART at `0x101F1000` is configured first so all subsequent messages can be printed to the terminal:
- Baud rate set via `IBRD=13`, `FBRD=1` (~115200 baud).
- 8-bit, no parity, 1 stop bit (`LCRH`).
- TX and RX enabled via `CR`.

#### Phase B: PL181 SD Card Controller Initialisation (`sd_init`)

The PL181 MMC/SD controller at `0x10005000` must be initialised following the SD card protocol before any sectors can be read:

| Command | Purpose |
|---|---|
| `MCI_POWER = 0x02/0x03` | Ramp up power to the SD card |
| `CMD0` | `GO_IDLE_STATE` — reset the card, no response expected |
| `CMD8 (0x1AA)` | `SEND_IF_COND` — probe for SD v2.0 and check voltage range (2.7–3.6V) |
| `ACMD41` (CMD55 + CMD41) | Poll OCR register in a loop until bit 31 is set (card finished power-up). `CCS` bit (bit 30) tells us if it's SDHC (sector-addressed) or SDSC (byte-addressed) |
| `CMD2` | `ALL_SEND_CID` — 128-bit long response with card identification |
| `CMD3` | `SEND_RELATIVE_ADDR` — card assigns itself an RCA (Relative Card Address) |
| `CMD7` | `SELECT_CARD` — puts the card into Transfer state using its RCA |
| `CMD16` | `SET_BLOCKLEN = 512` — fix block size |

After this sequence, the SD card is ready to accept `CMD17` (read single block) commands.

#### Phase C: FAT16 Filesystem Parsing

The bootloader reads the SD card (i.e., `os.bin`) sector by sector through the PL181 controller using `sd_read_sector(lba, buffer)`.

Each `sd_read_sector` call:
1. Clears the MCI status flags.
2. Programs the data transfer timer and block length.
3. Arms the DCTRL register for a 512-byte card→host read.
4. Issues `CMD17` with the sector address (LBA directly for SDHC, LBA×512 for SDSC).
5. Drains the RX FIFO 32 bits at a time into the output buffer.

**Reading the Boot Sector (LBA 0):**

The first sector of `os.bin` is the FAT16 Boot Sector. `mkfs.vfat` wrote a BPB (BIOS Parameter Block) here. The bootloader reads these fields:

| BPB Offset | Field | Typical Value |
|---|---|---|
| 11 | Bytes Per Sector | 512 |
| 13 | Sectors Per Cluster | varies (e.g. 8) |
| 14 | Reserved Sector Count | 4 |
| 16 | Number of FATs | 2 |
| 17 | Root Entry Count | 512 |
| 22 | FAT Size (sectors) | varies |

From these, the bootloader calculates absolute sector positions:

```
fat_start      = rsvd_sec_cnt
root_dir_start = fat_start + (num_fats × fat_size_16)
root_dir_sects = (root_ent_cnt × 32 + 511) / 512
data_start     = root_dir_start + root_dir_sects
```

**Scanning the Root Directory:**

The bootloader reads each sector of the root directory region. Each 32-byte entry represents one file:

```
Offset 0-10 : 8.3 filename (e.g. "KERNEL  BIN" — no dot, space-padded)
Offset 11   : Attributes (0x20 = Archive)
Offset 26   : First Cluster Low word
Offset 28   : File Size in bytes
```

The bootloader compares each entry's name against `"KERNEL  BIN"` character by character. When found, it extracts the `First Cluster` number.

**Following the FAT Cluster Chain:**

A FAT16 cluster chain works like a linked list. Each entry in the FAT table is a 16-bit value:
- `0x0000` = free cluster
- `0x0002–0xFFEF` = next cluster in the chain
- `0xFFF8–0xFFFF` = end of chain

The bootloader loops:
1. Compute the absolute LBA of the current cluster's data:  
   `lba = data_start + (cluster - 2) × sec_per_clus`
2. Read `sec_per_clus` sectors from that LBA into RAM at `KERNEL_EXEC_ADDR` (`0x100000`), advancing the destination pointer.
3. Read the FAT entry for the current cluster:  
   `fat_sector = fat_start + (cluster × 2) / 512`  
   `entry_offset = (cluster × 2) % 512`
4. The 16-bit value at that offset is the next cluster number.
5. Repeat until the value is ≥ `0xFFF8` (end-of-chain marker).

This loads the complete `KERNEL.BIN` into RAM starting at `0x100000`.

#### Phase D: Kernel Integrity Verification

Before jumping, the bootloader checks for the magic string `"STAX"` at byte offset 4 in the loaded kernel binary (`0x100004`). This works because `kernel/startup.s` places `.ascii "STAX"` at exactly offset 4 (right after the first 4-byte branch instruction `b reset_handler`):

```asm
_start:
    b reset_handler   @ 4 bytes: branch instruction
    .ascii "STAX"     @ 4 bytes: magic at 0x100004
```

If the magic matches → prints `"OK"` and proceeds.  
If it doesn't → halts with an error message. This would indicate a corrupt read or wrong LBA.

#### Phase E: Jump to Kernel

```c
void (*kernel_entry)(void) = (void (*)(void))KERNEL_EXEC_ADDR;
kernel_entry();   // jumps to 0x100000
```

The CPU Program Counter is now at `0x100000`. The bootloader's job is done.

---

## Stage 2 — Kernel Entry (`kernel/startup.s`)

Execution starts at `_start` in `startup.s`, which is placed at `0x100000` by the linker script.

### Step 2.1 — Branch over Magic Number

```asm
_start:
    b reset_handler    @ skip past magic
    .ascii "STAX"      @ magic at 0x100004 (used by bootloader)
```

### Step 2.2 — Set Kernel Stack Pointer

```asm
ldr sp, =stack_top
```

`stack_top` is a linker symbol defined at the end of the kernel's reserved memory region. The stack grows downward from here. Value: `0x100000 + 7.85KB (.text) + 0KB (.data) + ~10KB (.bss) + 32KB (heap) + 8KB (stack) = ~0x10E448`.

### Step 2.3 — Zero the BSS Segment

```asm
ldr r0, =__bss_start
ldr r1, =__bss_end
mov r2, #0
zero_bss:
    cmp r0, r1
    bge bss_done
    str r2, [r0], #4   @ store 0, advance pointer by 4
    b zero_bss
bss_done:
```

All 10136 bytes of `.bss` are zeroed word by word. This is required by the C standard — uninitialized global variables must start at zero. Without this, they would contain whatever garbage was in RAM.

### Step 2.4 — Copy Exception Vector Table to `0x00000000`

```asm
ldr  r0, =vector_table     @ source: our vector table at 0x100000+
mov  r1, #0                @ destination: 0x00000000
ldm  r0!, {r2-r9}          @ load 8 vector entries (32 bytes)
stm  r1!, {r2-r9}          @ write to 0x00000000–0x0000001C
ldm  r0!, {r2-r9}          @ load 8 literal pool words (32 bytes)
stm  r1!, {r2-r9}          @ write to 0x00000020–0x0000003C
```

ARM926EJ-S with `SCTLR.V=0` (default) reads exception vectors from `0x00000000`. Our kernel is linked at `0x100000`, so without this copy, any IRQ would jump to garbage. The VersatilePB maps writable SSRAM at `0x00000000`, so this works.

The 8 entries in the vector table are:

| Address | Exception | Handler |
|---|---|---|
| `0x00` | Reset | `→ _start` |
| `0x04` | Undefined Instruction | `→ undef_handler` (prints `U`, hangs) |
| `0x08` | Software Interrupt (SVC) | `→ svc_handler` (prints `S`, hangs) |
| `0x0C` | Prefetch Abort | `→ prefetch_handler` (prints `P`, hangs) |
| `0x10` | Data Abort | `→ data_handler` (prints `D`, hangs) |
| `0x14` | Reserved | `→ reserved_handler` |
| `0x18` | IRQ | `→ irq_handler_stub` ← timer fires here |
| `0x1C` | FIQ | `→ fiq_handler` (immediate return) |

### Step 2.5 — Set up IRQ and FIQ Mode Stacks (`irq_init_stacks`)

ARM has separate banked `sp` registers per CPU mode. `irq_init_stacks` switches into each mode and sets a dedicated stack pointer:

```
IRQ mode (0x12):  sp = irq_stack_top   (4096 bytes in .bss)
FIQ mode (0x11):  sp = fiq_stack_top   (1024 bytes in .bss)
SVC mode (0x13):  sp = stack_top       (8192 bytes, kernel main stack)
```

This separation ensures an IRQ cannot corrupt the kernel's SVC mode stack.

### Step 2.6 — Call `kernel_main()`

```asm
bl kernel_main
```

---

## Stage 3 — Kernel Main (`kernel/kernel.c`)

`kernel_main()` initializes each subsystem in dependency order:

### Step 3.1 — IRQ Subsystem (`irq_system_init`)

- Sets up the IRQ dispatch table (an array of function pointers, one per VIC interrupt line).
- Does NOT enable global IRQs yet — that happens last.

### Step 3.2 — Scheduler (`scheduler_init`)

- Allocates Task Control Block (TCB) slot 0 for the current execution context (the kernel itself becomes Task 0 / the idle task).
- Initializes a circular linked list with a single entry pointing to itself.
- Sets `current_task` to Task 0 with state `RUNNING`.

A TCB stores the full CPU register file: `r0–r12`, `sp`, `lr`, `pc`, `cpsr` — everything needed to pause and resume a task.

### Step 3.3 — Heap (`heap_init`)

- Sets the bump allocator pointer `heap_bump = __heap_start` (the address right after `.bss`).
- Clears the free list.
- The heap region is 32768 bytes (32 KB), bounded by `__heap_start` and `__heap_end`.

**How `kmalloc` works:**
1. Round requested size up to 8-byte alignment.
2. Walk the free list looking for a block ≥ requested size. If found, remove and return it (reuse).
3. If not found, carve a new block from the bump region, advance `heap_bump`, return the data pointer.
4. Returns `NULL` if `heap_bump + size > __heap_end`.

**How `kfree` works:**
1. Walk back from the data pointer to the `block_t` header (using `offsetof`).
2. Insert into the free list sorted by address.
3. Call `coalesce()` to merge adjacent free blocks.

### Step 3.4 — FAT Filesystem (`fat_init`)

The in-kernel FAT driver is a simplified implementation used by the shell's `fs` and `test` commands. It operates on a small in-memory test image (not the SD card). It provides `fat_open()`, `fat_read()`, `fat_close()`.

> Note: This is separate from the bootloader's FAT parser. The bootloader's parser read the real SD card to load the kernel. The kernel's `fat.c` driver serves as a proof-of-concept VFS layer for the interactive shell.

### Step 3.5 — Timer (`timer_init` + `irq_register`)

```c
irq_register(VIC_TIMER0_INT, timer_isr);   // register callback
timer_init(100000);                         // 100,000 µs = 100 ms = 10 Hz
```

**`timer_init` programs the SP804 Dual Timer at `0x101E2000`:**
- Loads the countdown value (100000 for 10 Hz at 1 MHz reference clock).
- Configures: 32-bit mode, periodic mode, interrupt enable.
- Enables the timer. It will now count down and fire an IRQ every 100 ms.

**VIC (Vectored Interrupt Controller at `0x10140000`):**
- Enables the Timer 0 interrupt line in `VIC_INTENABLE`.

### Step 3.6 — Enable Global IRQs

```c
irq_enable();   // clears the I-bit in CPSR
```

From this moment, the 10 Hz timer interrupt is live.

---

## Stage 4 — Interrupt Handling Flow (Every 100ms)

When the SP804 timer fires:

1. CPU detects IRQ, saves current `pc` and `cpsr` into banked IRQ-mode registers, switches to IRQ mode.
2. PC jumps to `0x18` in the vector table → `irq_handler_stub`.

### `irq_handler_stub` (in `vectors.s`):

```asm
sub  lr, lr, #4                    @ correct return address (ARM pipeline)
stmfd sp!, {r0-r3, r12, lr}       @ save caller-saved regs on IRQ stack
mrs  r0, spsr
stmfd sp!, {r0}                    @ save pre-IRQ CPSR

bl irq_dispatch                    @ call C handler

ldr r0, =need_schedule
ldr r1, [r0]
cmp r1, #0
beq no_schedule
bl  schedule                       @ optional context switch

no_schedule:
ldmfd sp!, {r0}
msr   spsr_cxsf, r0               @ restore pre-IRQ CPSR
ldmfd sp!, {r0-r3, r12, pc}^     @ restore regs + CPSR, return from IRQ
```

### `irq_dispatch` (in `irq.c`):

- Reads `VIC_IRQSTATUS` to identify which interrupt line fired.
- Calls the registered handler for that line — in this case `timer_isr`.

### `timer_isr` (in `kernel.c`):

```c
static void timer_isr(void) {
    tick_count++;    // global uptime counter
    timer_ack();     // clear the SP804 interrupt flag so it can fire again
}
```

`timer_ack()` writes to the SP804's `TimerXIntClr` register. Without this, the interrupt would fire again immediately in an infinite loop.

---

## Stage 5 — Interactive Shell

Back in `kernel_main()`, after all subsystems are initialized:

```c
command_init();   // prints "Command system initialized"

while (1) {
    kputs("STAX> ");              // print prompt
    char c = kgetc();            // poll UART RX register (non-blocking)
    if (c == '\r' || c == '\n')
        command_process(input);  // parse and dispatch
    else
        accumulate(c, input);    // add char to buffer
}
```

`kgetc()` reads the PL011 UART at `0x101F1000`. It checks the Flag Register (`FR`) bit 4 (RXFE — RX FIFO empty). If the FIFO has data, it reads one byte from the Data Register (`DR`).

### Available Commands

| Command | What it does |
|---|---|
| `help` | Lists all commands |
| `status` | Prints kernel phase, CPU type, board, uptime in seconds (`tick_count / 10`) |
| `mem` | Calls `heap_stats()` — shows bump used, free blocks, free bytes |
| `tasks` | Prints scheduler info |
| `fs` | Opens `TEST.TXT` from the in-memory FAT image and confirms it's readable |
| `test` | Allocates 64B + 128B via `kmalloc`, frees them, reads `TEST.TXT`, prints content |
| `read` | Uses linker symbols to print exact `.text`, `.data`, `.bss`, heap, and stack sizes |
| `clear` | Sends ANSI `ESC[2J ESC[H` escape codes to clear terminal |

---

## Memory Map Summary

```
Physical Address Space (ARM926EJ-S, VersatilePB)
═══════════════════════════════════════════════════════════════════════

0x00000000  Exception Vector Table (64 bytes, copied here by startup.s)
            Writable SSRAM aliases the bottom of SDRAM on VersatilePB

0x00010000  bootloader.bin (~2 KB)         ← Injected by QEMU -kernel
            [grows up]
0x00080000  Bootloader Stack Top           ← Grows DOWN toward 0x10000

0x00080000 ─── 512 KB unmapped gap ───────────────────────────────────

0x00100000  KERNEL_EXEC_ADDR ─────────────────────────────────────────
│  .text    ~7.85 KB  │ Kernel code + rodata + glue sections
│  .data       4 B    │ Initialized globals (tick_count etc.)
│  .bss     ~9.89 KB  │ Zeroed by startup.s before kernel_main
│  Heap      64.00 KB │ __heap_start → __heap_end (bump + free list)
│  Stack      8.00 KB │ Grows DOWN from stack_top
0x0010E448  stack_top

0x0010E448 ─── ~3.94 MB unallocated kernel space ─────────────────────

0x00500000  End of 4 MB kernel region

0x00500000 ─── ~251 MB untouched physical SDRAM ──────────────────────

0x10000000  Memory-Mapped Peripheral I/O ─────────────────────────────
0x10005000  │  PL181 MMC/SD Controller  (SD card reads)
0x10140000  │  Vectored Interrupt Controller (VIC)
0x101E2000  │  SP804 Dual Timer 0/1     (10 Hz ticks)
0x101F1000  │  PL011 UART 0             (serial console)
0x1FFFFFFF  ─────────────────────────────────────────────────────────
```

---

## End-to-End Summary Flowchart

```
make qemu
   │
   ├─► Compile bootloader → bootloader.bin
   ├─► Compile kernel     → kernel.bin
   ├─► mkfs.vfat + mcopy  → os.bin (FAT16 with KERNEL.BIN inside)
   └─► QEMU launched
           │
           ▼
   QEMU injects bootloader.bin → RAM at 0x10000
   CPU PC = 0x10000
           │
           ▼
   boot_startup.s:  SP = 0x80000, zero BSS, call bootloader_main()
           │
           ▼
   bootloader_main():
     ├─ UART init
     ├─ PL181 SD init  (CMD0→CMD8→ACMD41→CMD2→CMD3→CMD7→CMD16)
     ├─ Read FAT Boot Sector (LBA 0) → parse BPB
     ├─ Scan Root Directory → find "KERNEL  BIN"
     ├─ Follow FAT cluster chain → load all clusters → 0x100000
     ├─ Verify magic "STAX" at 0x100004
     └─ Jump to 0x100000
           │
           ▼
   startup.s:
     ├─ SP = stack_top
     ├─ Zero .bss  (10136 bytes)
     ├─ Copy vector table → 0x00000000
     ├─ irq_init_stacks() (IRQ/FIQ/SVC mode stacks)
     └─ call kernel_main()
           │
           ▼
   kernel_main():
     ├─ irq_system_init()
     ├─ scheduler_init()    (Task 0 = kernel itself)
     ├─ heap_init()         (32 KB bump allocator)
     ├─ fat_init()          (in-memory test FS)
     ├─ irq_register(TIMER, timer_isr)
     ├─ timer_init(100000)  (10 Hz SP804)
     ├─ irq_enable()        (unmask CPSR I-bit)
     └─ Interactive shell loop (kgetc → command_process)
           │
           ├── Every 100ms ──► IRQ fires → irq_handler_stub
           │                      └─ irq_dispatch → timer_isr
           │                           └─ tick_count++, timer_ack()
           │
           └── On user input ──► command_process("read" / "mem" / ...)
```
