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
GAMES_DIR   := games
ENGINE_DIR  := engine
GFX_DIR     := gfx

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
            -I$(INC_DIR)        \
            -I$(FS_DIR)         \
            -I$(ENGINE_DIR)

ASFLAGS := $(CFLAGS) -x assembler-with-cpp

LDFLAGS := -nostdlib --gc-sections
LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

# ---------------------------------------------------------------------------
# Source files and objects
# ---------------------------------------------------------------------------
# Bootloader targets
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
                $(BUILD_DIR)/string.o \
                $(BUILD_DIR)/vic.o \
                $(BUILD_DIR)/timer.o \
                $(BUILD_DIR)/scheduler.o \
                $(BUILD_DIR)/heap.o \
                $(BUILD_DIR)/fat.o \
                $(BUILD_DIR)/disk.o \
                $(BUILD_DIR)/irq.o \
                $(BUILD_DIR)/keyboard.o \
                $(BUILD_DIR)/console.o \
                $(BUILD_DIR)/framebuffer.o \
                $(BUILD_DIR)/font8x16.o \
                $(BUILD_DIR)/gfx_console.o \
                $(BUILD_DIR)/command.o \
                $(BUILD_DIR)/bmp.o \
                $(BUILD_DIR)/engine2d.o

# FatFs objects
KERNEL_OBJS  += $(BUILD_DIR)/fatfs_diskio.o \
                $(BUILD_DIR)/ff.o \
                $(BUILD_DIR)/ffunicode.o \
                $(BUILD_DIR)/ffsystem.o

# tasks.o was added in previous git commits but was not in makefile. I'll add it.
KERNEL_OBJS  += $(BUILD_DIR)/tasks.o

# Software rendering subsystem
KERNEL_OBJS  += $(BUILD_DIR)/math_fixed.o \
                $(BUILD_DIR)/palette.o \
                $(BUILD_DIR)/backbuffer.o \
                $(BUILD_DIR)/renderer.o \
                $(BUILD_DIR)/sprite.o \
                $(BUILD_DIR)/texture.o \
                $(BUILD_DIR)/profiler.o

# Games
KERNEL_OBJS  += $(BUILD_DIR)/snake.o
KERNEL_OBJS  += $(BUILD_DIR)/doom.o
KERNEL_OBJS  += $(BUILD_DIR)/test_game.o
KERNEL_OBJS  += $(BUILD_DIR)/slime.o

# em-doom objects
DOOM_SRCS := $(wildcard $(GAMES_DIR)/em-doom/linuxdoom-1.10/*.c)
# Filter out the original platform files, we use tios_platform.c instead
DOOM_SRCS := $(filter-out %/i_main.c %/i_system.c %/i_sound.c %/i_video.c %/i_net.c, $(DOOM_SRCS))
DOOM_OBJS := $(patsubst $(GAMES_DIR)/em-doom/linuxdoom-1.10/%.c, $(BUILD_DIR)/%.o, $(DOOM_SRCS))
KERNEL_OBJS  += $(DOOM_OBJS)
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
QEMU_FLAGS   := -M $(QEMU_MACHINE) -kernel $(BOOT_BIN) -drive file=$(OS_BIN),if=sd,format=raw -nographic -serial mon:stdio
QEMU_GFX_FLAGS := -M $(QEMU_MACHINE) -kernel $(BOOT_BIN) -drive file=$(OS_BIN),if=sd,format=raw -serial stdio

# =============================================================================
# Rules
# =============================================================================
.PHONY: all clean qemu qemu-gfx debug gdb dump size assets

all: assets $(BUILD_DIR) $(BOOT_BIN) $(OS_BIN)

assets:
	@echo "Checking/Building Game Assets..."
	@python3 scripts/build_assets.py || echo "Warning: Asset build failed (Pillow missing?)"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OS_BIN): $(KERNEL_BIN)
	@echo ""
	@if [ ! -f $@ ]; then \
		echo "Creating new FAT16 OS Image → $@"; \
		dd if=/dev/zero of=$@ bs=1M count=32 2>/dev/null; \
		mkfs.vfat -F 16 $@; \
		if [ -f $(GAMES_DIR)/em-doom/doom.wad ]; then mcopy -i $@ $(GAMES_DIR)/em-doom/doom.wad ::/DOOM.WAD; fi; \
		if [ -f $(GAMES_DIR)/em-doom/doom2.wad ]; then mcopy -i $@ $(GAMES_DIR)/em-doom/doom2.wad ::/DOOM2.WAD; fi; \
		if [ -f /tmp/KITTEN.BMP ]; then mcopy -i $@ /tmp/KITTEN.BMP ::/KITTEN.BMP; fi; \
	else \
		echo "Updating KERNEL.BIN in existing OS Image → $@"; \
	fi
	@mcopy -o -i $@ build/kernel.bin ::/KERNEL.BIN
	# =========================================================================
	# [USER CUSTOMIZATION]: IMPORT IMAGES HERE
	# To add custom BMP images to the OS disk, uncomment and modify the line below:
	# @mcopy -o -i $@ MYIMAGE.BMP ::/MYIMAGE.BMP
	# Note: Image must be a valid .BMP file
	# =========================================================================
	@echo "Build complete → $@"
	@echo "Run:  make qemu"
	@echo "Quit: Ctrl-A then X"
	@echo ""

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
	$(LD) -T $(BOOT_LD) $(LDFLAGS) $(BOOT_OBJS) $(LIBGCC) -o $@

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

$(BUILD_DIR)/%.o: $(FS_DIR)/fatfs/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(MM_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(GAMES_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(ENGINE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(GFX_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -O2 -ffast-math -c $< -o $@

# DOOM files need special flags and the tios_compat.h included
$(BUILD_DIR)/%.o: $(GAMES_DIR)/em-doom/linuxdoom-1.10/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -w -O2 -include $(GAMES_DIR)/em-doom/linuxdoom-1.10/tios_compat.h -c $< -o $@

# ---------------------------------------------------------------------------
# Kernel Linking
# ---------------------------------------------------------------------------
$(KERNEL_LD): $(KERNEL_LD_IN) $(INC_DIR)/memory_map.h | $(BUILD_DIR)
	$(CC) -E -P -x c -I$(INC_DIR) $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS) $(KERNEL_LD)
	$(LD) -T $(KERNEL_LD) $(LDFLAGS) $(KERNEL_OBJS) $(LIBGCC) -o $@
	@echo "Linked → $@"

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Binary → $@ ($$(wc -c < $@) bytes)"

# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------
qemu: $(BOOT_BIN) $(OS_BIN)
	@echo "Booting TIOS in QEMU (Ctrl-A X to quit)..."
	$(QEMU) $(QEMU_FLAGS)

qemu-gfx: $(BOOT_BIN) $(OS_BIN)
	@echo "Booting TIOS in QEMU with graphics (Ctrl-C to quit)..."
	@echo "Use 'game --doom' command to run graphical DOOM"
	$(QEMU) $(QEMU_GFX_FLAGS)

debug: $(BOOT_BIN) $(OS_BIN)
	@echo "QEMU waiting for GDB on :1234  (run 'make gdb' in another terminal)"
	$(QEMU) $(QEMU_FLAGS) -S -gdb tcp::1234

gdb: $(KERNEL_ELF)
	$(GDB) $(KERNEL_ELF) -ex "target remote localhost:1234" -ex "load" -ex "continue"

dump: $(KERNEL_ELF)
	$(OBJDUMP) -D -S $<

size: $(KERNEL_ELF)
	$(SIZE) $<

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build directory. (os.bin is preserved to protect your data)"

clean-all: clean
	rm -f $(OS_BIN)
	@echo "Cleaned everything, including os.bin."