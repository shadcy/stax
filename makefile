# =============================================================================
# TIOS — Makefile
# Builds the bootloader and kernel into a raw binary for QEMU or real hardware.
#
# Usage:
#   make          — compile and link everything → kernel.bin
#   make qemu     — build then boot in QEMU (no window, UART to stdout)
#   make debug    — build then start QEMU in GDB-server mode (port 1234)
#   make gdb      — attach GDB to a running QEMU debug session
#   make dump     — disassemble kernel.elf (great for learning)
#   make size     — show section sizes (Flash / RAM usage)
#   make clean    — remove all generated files
#
# Requirements (Ubuntu):
#   sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi \
#                    gdb-multiarch qemu-system-arm make
# =============================================================================


# ---------------------------------------------------------------------------
# Toolchain — every tool is the arm-none-eabi cross-compiler variant
# ---------------------------------------------------------------------------
CROSS   := arm-none-eabi
CC      := $(CROSS)-gcc        # C compiler
AS      := $(CROSS)-gcc        # Use gcc to assemble (handles .s pre-processing)
LD      := $(CROSS)-ld         # Linker
OBJCOPY := $(CROSS)-objcopy    # Convert ELF → raw binary
OBJDUMP := $(CROSS)-objdump    # Disassembler
SIZE    := $(CROSS)-size       # Section-size reporter
GDB     := gdb-multiarch       # Multi-architecture GDB


# ---------------------------------------------------------------------------
# Compiler / assembler flags
# ---------------------------------------------------------------------------

# -mcpu=arm926ej-s  : matches the versatilepb CPU in QEMU
# -mthumb-interwork : allow interworking between ARM and Thumb instructions
# -ffreestanding    : no libc, no startup files, no hosted environment assumed
# -nostdlib         : do not link against the standard library
# -nostartfiles     : do not add the compiler's own crt0.o startup code
# -Wall -Wextra     : enable comprehensive warnings
# -O1               : light optimisation (safe for bare metal)
# -g                : include DWARF debug info (used by GDB)
CFLAGS  := -mcpu=arm926ej-s    \
            -mthumb-interwork   \
            -ffreestanding      \
            -nostdlib           \
            -nostartfiles       \
            -Wall               \
            -Wextra             \
            -O1                 \
            -g

ASFLAGS := $(CFLAGS)           # Same flags for the assembler pass


# ---------------------------------------------------------------------------
# Linker flags
# ---------------------------------------------------------------------------

# -T linker.ld  : use our custom linker script (defines RAM layout)
# -nostdlib     : don't pull in any standard libraries
# --gc-sections : remove dead code sections (saves space)
LDFLAGS := -T linker.ld        \
            -nostdlib           \
            --gc-sections


# ---------------------------------------------------------------------------
# Source files and derived object/output names
# ---------------------------------------------------------------------------
SRCS_C  := boot.c kernel.c irq.c vic.c timer.c scheduler.c heap.c
SRCS_S  := startup.s vectors.s

OBJS    := startup.o vectors.o boot.o kernel.o vic.o timer.o scheduler.o heap.o irq.o   # order matters: startup.o first

TARGET_ELF := kernel.elf   # linked ELF with debug symbols
TARGET_BIN := kernel.bin   # raw binary stripped of ELF headers (for QEMU/flash)


# ---------------------------------------------------------------------------
# QEMU invocation
# ---------------------------------------------------------------------------
QEMU        := qemu-system-arm
QEMU_MACHINE := versatilepb          # ARM Versatile Platform Baseboard
QEMU_FLAGS  := -M $(QEMU_MACHINE) -kernel $(TARGET_BIN) -nographic -serial mon:stdio


# =============================================================================
# Rules
# =============================================================================

# Default target
.PHONY: all
all: $(TARGET_BIN)
	@echo ""
	@echo "Build complete → $(TARGET_BIN)"
	@echo "Run:  make qemu"
	@echo "Quit: Ctrl-A then X"
	@echo ""


# ---------------------------------------------------------------------------
# Assemble startup.s → startup.o
# ---------------------------------------------------------------------------
startup.o: startup.s
	$(AS) $(ASFLAGS) -c $< -o $@


# ---------------------------------------------------------------------------
# Compile boot.c → boot.o
# ---------------------------------------------------------------------------
boot.o: boot.c
	$(CC) $(CFLAGS) -c $< -o $@


# ---------------------------------------------------------------------------
# Compile kernel.c → kernel.o
# ---------------------------------------------------------------------------
kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@
# ---------------------------------------------------------------------------
# Compile irq.c → irq.o
# ---------------------------------------------------------------------------
irq.o: irq.c irq.h vic.h
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# Compile vic.c → vic.o
# ---------------------------------------------------------------------------
vic.o: vic.c vic.h

# ---------------------------------------------------------------------------
# Compile timer.c → timer.o
# ---------------------------------------------------------------------------
timer.o: timer.c timer.h vic.h irq.h

# ---------------------------------------------------------------------------
# Compile scheduler.c → scheduler.o
# ---------------------------------------------------------------------------
scheduler.o: scheduler.c scheduler.h

# ---------------------------------------------------------------------------
# Compile heap.c → heap.o
# ---------------------------------------------------------------------------
heap.o: heap.c heap.h
	$(CC) $(CFLAGS) -c $< -o $@
	$(CC) $(CFLAGS) -c $< -o $@
	$(CC) $(CFLAGS) -c $< -o $@
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# Assemble vectors.s → irq.o
# ---------------------------------------------------------------------------
vectors.o: vectors.s
	$(AS) $(ASFLAGS) -c $< -o $@


# ---------------------------------------------------------------------------
# Link all object files → kernel.elf
# ---------------------------------------------------------------------------
$(TARGET_ELF): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "Linked → $@"


# ---------------------------------------------------------------------------
# Strip ELF → raw binary
#
# -O binary : output format is a flat raw binary
# The resulting kernel.bin is what QEMU's -kernel flag expects, and what
# OpenOCD / st-flash will write to real hardware.
# ---------------------------------------------------------------------------
$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Binary → $@ ($(shell wc -c < $@) bytes)"


# ---------------------------------------------------------------------------
# Run in QEMU
# Press Ctrl-A then X to exit QEMU.
# ---------------------------------------------------------------------------
.PHONY: qemu
qemu: $(TARGET_BIN)
	@echo "Booting TIOS in QEMU (Ctrl-A X to quit)..."
	$(QEMU) $(QEMU_FLAGS)


# ---------------------------------------------------------------------------
# Debug: start QEMU halted, waiting for GDB on port 1234
# In a second terminal run:  make gdb
# ---------------------------------------------------------------------------
.PHONY: debug
debug: $(TARGET_BIN)
	@echo "QEMU waiting for GDB on :1234  (run 'make gdb' in another terminal)"
	$(QEMU) $(QEMU_FLAGS) -S -gdb tcp::1234


# ---------------------------------------------------------------------------
# GDB: connect to QEMU debug session
# ---------------------------------------------------------------------------
.PHONY: gdb
gdb: $(TARGET_ELF)
	$(GDB) $(TARGET_ELF)                    \
	    -ex "target remote localhost:1234"   \
	    -ex "load"                           \
	    -ex "break boot_main"                \
	    -ex "continue"


# ---------------------------------------------------------------------------
# Disassemble: show the full mixed source + assembly listing
# ---------------------------------------------------------------------------
.PHONY: dump
dump: $(TARGET_ELF)
	$(OBJDUMP) -D -S $<


# ---------------------------------------------------------------------------
# Size: show how many bytes each section uses
# ---------------------------------------------------------------------------
.PHONY: size
size: $(TARGET_ELF)
	$(SIZE) $<


# ---------------------------------------------------------------------------
# Clean: remove all generated files
# ---------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET_ELF) $(TARGET_BIN)
	@echo "Cleaned."