# =============================================================================
# TIOS — Makefile
# Builds the bootloader and kernel into a raw binary for QEMU or real hardware.
# =============================================================================

CROSS   := arm-none-eabi
CC      := $(CROSS)-gcc
AS      := $(CROSS)-gcc
LD      := $(CROSS)-ld
OBJCOPY := $(CROSS)-objcopy
OBJDUMP := $(CROSS)-objdump
SIZE    := $(CROSS)-size
GDB     := gdb-multiarch

# ---------------------------------------------------------------------------
# Directories
# ---------------------------------------------------------------------------
BUILD_DIR   := build
INC_DIR     := include
BOOT_DIR    := boot
KERNEL_DIR  := kernel
DRIVERS_DIR := drivers
FS_DIR      := fs
MM_DIR      := mm

# ---------------------------------------------------------------------------
# Compiler / assembler flags
# ---------------------------------------------------------------------------
CFLAGS  := -mcpu=arm926ej-s    \
            -mthumb-interwork   \
            -ffreestanding      \
            -nostdlib           \
            -nostartfiles       \
            -Wall               \
            -Wextra             \
            -O1                 \
            -g                  \
            -I$(INC_DIR)

ASFLAGS := $(CFLAGS) -x assembler-with-cpp

LDFLAGS := -nostdlib --gc-sections

# ---------------------------------------------------------------------------
# Source files and objects
# ---------------------------------------------------------------------------
# Bootloader targets
MBR_SRC      := $(BOOT_DIR)/mbr.s
MBR_OBJ      := $(BUILD_DIR)/mbr.o
MBR_BIN      := $(BUILD_DIR)/mbr.bin

BOOT_STARTUP := $(BOOT_DIR)/boot_startup.s
BOOT_SRC     := $(BOOT_DIR)/bootloader.c
BOOT_OBJS    := $(BUILD_DIR)/boot_startup.o $(BUILD_DIR)/bootloader.o
BOOT_LD_IN   := $(BOOT_DIR)/bootloader.ld.in
BOOT_LD      := $(BUILD_DIR)/bootloader.ld
BOOT_ELF     := $(BUILD_DIR)/bootloader.elf
BOOT_BIN     := $(BUILD_DIR)/bootloader.bin

# Kernel targets (Order of objects matters: startup.o must be first)
KERNEL_OBJS  := $(BUILD_DIR)/startup.o \
                $(BUILD_DIR)/vectors.o \
                $(BUILD_DIR)/kernel.o \
                $(BUILD_DIR)/vic.o \
                $(BUILD_DIR)/timer.o \
                $(BUILD_DIR)/scheduler.o \
                $(BUILD_DIR)/heap.o \
                $(BUILD_DIR)/fat.o \
                $(BUILD_DIR)/disk.o \
                $(BUILD_DIR)/irq.o \
                $(BUILD_DIR)/console.o \
                $(BUILD_DIR)/command.o

# tasks.o was added in previous git commits but was not in makefile. I'll add it.
KERNEL_OBJS  += $(BUILD_DIR)/tasks.o

KERNEL_LD_IN := linker.ld.in
KERNEL_LD    := $(BUILD_DIR)/linker.ld
KERNEL_ELF   := $(BUILD_DIR)/kernel.elf
KERNEL_BIN   := $(BUILD_DIR)/kernel.bin

OS_BIN       := os.bin

# ---------------------------------------------------------------------------
# QEMU invocation
# ---------------------------------------------------------------------------
QEMU         := qemu-system-arm
QEMU_MACHINE := versatilepb
QEMU_FLAGS   := -M $(QEMU_MACHINE) -kernel $(OS_BIN) -nographic -serial mon:stdio

# =============================================================================
# Rules
# =============================================================================
.PHONY: all clean qemu debug gdb dump size

all: $(BUILD_DIR) $(OS_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OS_BIN): $(MBR_BIN) $(BOOT_BIN) $(KERNEL_BIN)
	@echo ""
	@echo "Assembling OS Image → $@"
	@dd if=/dev/zero of=$@ bs=512 count=1000 2>/dev/null
	@dd if=$(MBR_BIN) of=$@ conv=notrunc 2>/dev/null
	@dd if=$(BOOT_BIN) of=$@ seek=1 conv=notrunc 2>/dev/null
	@dd if=$(KERNEL_BIN) of=$@ seek=63 conv=notrunc 2>/dev/null
	@echo "Build complete → $@"
	@echo "Run:  make qemu"
	@echo "Quit: Ctrl-A then X"
	@echo ""

# ---------------------------------------------------------------------------
# MBR
# ---------------------------------------------------------------------------
$(MBR_OBJ): $(MBR_SRC) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(MBR_BIN): $(MBR_OBJ)
	$(OBJCOPY) -O binary $< $@

# ---------------------------------------------------------------------------
# Bootloader
# ---------------------------------------------------------------------------
$(BUILD_DIR)/boot_startup.o: $(BOOT_STARTUP) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/bootloader.o: $(BOOT_SRC) $(INC_DIR)/memory_map.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BOOT_LD): $(BOOT_LD_IN) $(INC_DIR)/memory_map.h | $(BUILD_DIR)
	$(CC) -E -P -x c -I$(INC_DIR) $< -o $@

$(BOOT_ELF): $(BOOT_OBJS) $(BOOT_LD)
	$(LD) -T $(BOOT_LD) $(LDFLAGS) $(BOOT_OBJS) -o $@

$(BOOT_BIN): $(BOOT_ELF)
	$(OBJCOPY) -O binary $< $@

# ---------------------------------------------------------------------------
# Kernel Pattern Rules
# ---------------------------------------------------------------------------
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.s | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(DRIVERS_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(FS_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(MM_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# Kernel Linking
# ---------------------------------------------------------------------------
$(KERNEL_LD): $(KERNEL_LD_IN) $(INC_DIR)/memory_map.h | $(BUILD_DIR)
	$(CC) -E -P -x c -I$(INC_DIR) $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) $(KERNEL_LD)
	$(LD) -T $(KERNEL_LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@
	@echo "Linked → $@"

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Binary → $@ ($(shell wc -c < $@) bytes)"

# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------
qemu: $(OS_BIN)
	@echo "Booting TIOS in QEMU (Ctrl-A X to quit)..."
	$(QEMU) $(QEMU_FLAGS)

debug: $(OS_BIN)
	@echo "QEMU waiting for GDB on :1234  (run 'make gdb' in another terminal)"
	$(QEMU) $(QEMU_FLAGS) -S -gdb tcp::1234

gdb: $(KERNEL_ELF)
	$(GDB) $(KERNEL_ELF) -ex "target remote localhost:1234" -ex "load" -ex "continue"

dump: $(KERNEL_ELF)
	$(OBJDUMP) -D -S $<

size: $(KERNEL_ELF)
	$(SIZE) $<

clean:
	rm -rf $(BUILD_DIR) $(OS_BIN)
	@echo "Cleaned."