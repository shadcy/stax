# =============================================================================
# STAX — Makefile
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
# Configuration Flags
# ---------------------------------------------------------------------------
ENABLE_BENCH ?= 0
# //adding this in case i dont want to include benching to the kernel

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
APPS_DIR    := apps
UI_DIR      := ui
SHELL_DIR   := shell
LIB_DIR     := lib
BENCH_DIR   := bench

# ---------------------------------------------------------------------------
# Compiler / assembler flags
# ---------------------------------------------------------------------------
CFLAGS  := -mcpu=arm926ej-s    \
            -mthumb-interwork   \
            -ffreestanding      \
            -nostdlib           \
            -nostartfiles       \
            -ffunction-sections \
            -fdata-sections     \
            -Wall               \
            -Wextra             \
            -O2                 \
            -g                  \
            -I$(INC_DIR)        \
            -I$(FS_DIR)         \
            -I$(ENGINE_DIR)     \
            -I$(GFX_DIR)        \
            -I$(APPS_DIR)       \
            -I$(UI_DIR)         \
            -I$(SHELL_DIR)      \
            -I$(LIB_DIR)        \
            -I$(BENCH_DIR)      \
            -Ithird_party/lwip/src/include \
            -Inet

ifeq ($(ENABLE_BENCH), 1)
CFLAGS += -DENABLE_BENCH
endif

ASFLAGS := $(CFLAGS) -x assembler-with-cpp


LDFLAGS := -nostdlib --gc-sections
LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

# ---------------------------------------------------------------------------
# Source files and objects
# ---------------------------------------------------------------------------
# Bootloader targets
BOOT_STARTUP := $(BOOT_DIR)/boot_startup.s
BOOT_SRC     := $(BOOT_DIR)/bootloader.c
BOOT_OBJS    := $(BUILD_DIR)/boot_startup.o $(BUILD_DIR)/bootloader.o \
                $(BUILD_DIR)/sha256.o $(BUILD_DIR)/crc32.o $(BUILD_DIR)/monocypher.o \
                $(BUILD_DIR)/firmware_format.o
BOOT_LD_IN   := $(BOOT_DIR)/bootloader.ld.in
BOOT_LD      := $(BUILD_DIR)/bootloader.ld
BOOT_ELF     := $(BUILD_DIR)/bootloader.elf
BOOT_BIN     := $(BUILD_DIR)/bootloader.bin

# Kernel targets (Order of objects matters: startup.o must be first)
KERNEL_OBJS  := $(BUILD_DIR)/startup.o \
                $(BUILD_DIR)/vectors.o \
                $(BUILD_DIR)/kernel.o \
                $(BUILD_DIR)/syscall.o \
                $(BUILD_DIR)/system.o \
                $(BUILD_DIR)/string.o \
                $(BUILD_DIR)/vic.o \
                $(BUILD_DIR)/timer.o \
                $(BUILD_DIR)/rtc.o \
                $(BUILD_DIR)/scheduler.o \
                $(BUILD_DIR)/heap.o \
                $(BUILD_DIR)/mmu.o \
                $(BUILD_DIR)/page.o \
                $(BUILD_DIR)/elf.o \
                $(BUILD_DIR)/launch.o \
                $(BUILD_DIR)/vfs.o \
                $(BUILD_DIR)/devfs.o \
                $(BUILD_DIR)/pipe.o \
                $(BUILD_DIR)/signal.o \
                $(BUILD_DIR)/process.o \
                $(BUILD_DIR)/fat.o \
                $(BUILD_DIR)/disk.o \
                $(BUILD_DIR)/irq.o \
                $(BUILD_DIR)/keyboard.o \
                $(BUILD_DIR)/mouse.o \
                $(BUILD_DIR)/pty.o \
                $(BUILD_DIR)/wm.o \
                $(BUILD_DIR)/wm_render.o \
                $(BUILD_DIR)/wm_desktop.o \
                $(BUILD_DIR)/console.o \
                $(BUILD_DIR)/app_file_manager.o \
                $(BUILD_DIR)/app_editor.o \
                $(BUILD_DIR)/app_taskmgr.o \
                $(BUILD_DIR)/app_terminal.o \
                $(BUILD_DIR)/app_calculator.o \
                $(BUILD_DIR)/app_sysinfo.o \
                $(BUILD_DIR)/app_image_viewer.o \
                $(BUILD_DIR)/app_memview.o \
                $(BUILD_DIR)/app_firmware_viewer.o \
                $(BUILD_DIR)/app_browser.o \
                $(BUILD_DIR)/app_settings.o \
                $(BUILD_DIR)/browser_html.o \
                $(BUILD_DIR)/framebuffer.o \
                $(BUILD_DIR)/font8x16.o \
                $(BUILD_DIR)/ubuntu_font_data.o \
                $(BUILD_DIR)/font.o \
                $(BUILD_DIR)/icons.o \
                $(BUILD_DIR)/gfx_console.o \
                $(BUILD_DIR)/command.o \
                $(BUILD_DIR)/bmp.o

# FatFs objects
KERNEL_OBJS  += $(BUILD_DIR)/fatfs_diskio.o \
                $(BUILD_DIR)/ff.o \
                $(BUILD_DIR)/ffunicode.o \
                $(BUILD_DIR)/ffsystem.o

# tasks.o was added in previous git commits but was not in makefile. I'll add it.
KERNEL_OBJS  += $(BUILD_DIR)/tasks.o

# Firmware update subsystem
KERNEL_OBJS  += $(BUILD_DIR)/firmware_update.o
KERNEL_OBJS  += $(BUILD_DIR)/crc32.o \
                $(BUILD_DIR)/sha256.o \
                $(BUILD_DIR)/firmware_format.o

# Benchmark infrastructure
ifeq ($(ENABLE_BENCH), 1)
KERNEL_OBJS  += $(BUILD_DIR)/bench.o \
                 $(BUILD_DIR)/bench_main.o \
                 $(BUILD_DIR)/bench_memory.o \
                 $(BUILD_DIR)/bench_vm.o \
                 $(BUILD_DIR)/bench_scheduler.o \
                 $(BUILD_DIR)/bench_fs.o \
                 $(BUILD_DIR)/bench_gfx.o \
                 $(BUILD_DIR)/bench_stress.o \
                 $(BUILD_DIR)/bench_kernel_test.o \
                 $(BUILD_DIR)/firmware_bench.o \
                 $(BUILD_DIR)/secure_boot_bench.o \
                 $(BUILD_DIR)/update_bench.o \
                 $(BUILD_DIR)/fault_injection.o \
                 $(BUILD_DIR)/metadata_test.o \
                 $(BUILD_DIR)/image_test.o \
                 $(BUILD_DIR)/rollback_test.o \
                $(BUILD_DIR)/fault_campaign.o
endif

# Standalone Userland em-doom objects
DOOM_SRCS := $(wildcard $(GAMES_DIR)/em-doom/linuxdoom-1.10/*.c)
# Filter out original platform files, we use stax_platform.c
DOOM_SRCS := $(filter-out %/i_main.c %/i_system.c %/i_sound.c %/i_video.c %/i_net.c, $(DOOM_SRCS))
DOOM_OBJS := $(patsubst $(GAMES_DIR)/em-doom/linuxdoom-1.10/%.c, $(BUILD_DIR)/doom_objs/%.o, $(DOOM_SRCS))

KERNEL_LD_IN := linker.ld.in
KERNEL_LD    := $(BUILD_DIR)/linker.ld
KERNEL_ELF   := $(BUILD_DIR)/kernel.elf
KERNEL_BIN   := $(BUILD_DIR)/kernel.bin

OS_BIN       := os.bin

# Networking
KERNEL_OBJS  += $(BUILD_DIR)/smc91c111.o \
                $(BUILD_DIR)/sys_arch.o \
                $(BUILD_DIR)/net_init.o \
                $(BUILD_DIR)/netif_smc.o \
                $(BUILD_DIR)/ping.o

LWIP_DIR := third_party/lwip/src
LWIP_SRCS := $(wildcard $(LWIP_DIR)/core/*.c) \
             $(wildcard $(LWIP_DIR)/core/ipv4/*.c) \
             $(wildcard $(LWIP_DIR)/netif/*.c) \
             $(wildcard $(LWIP_DIR)/apps/sntp/*.c)
LWIP_OBJS := $(patsubst $(LWIP_DIR)/%.c, $(BUILD_DIR)/lwip/%.o, $(LWIP_SRCS))
KERNEL_OBJS  += $(LWIP_OBJS)

# ---------------------------------------------------------------------------
# QEMU / Emulation Variables
# ---------------------------------------------------------------------------
QEMU         := qemu-system-arm
QEMU_MACHINE := versatilepb
# Guest NIC MAC must differ from slirp's gateway MAC (52:54:00:12:34:56) to
# avoid src==dst collisions that cause slirp to drop all outbound TCP frames.
QEMU_NIC     := -net nic,model=smc91c111,macaddr=52:54:00:12:34:57
QEMU_FLAGS   := -M $(QEMU_MACHINE) -kernel $(BOOT_BIN) -drive file=$(OS_BIN),if=sd,format=raw -nographic -serial mon:stdio $(QEMU_NIC) -net user
QEMU_GFX_FLAGS := -M $(QEMU_MACHINE) -kernel $(BOOT_BIN) -drive file=$(OS_BIN),if=sd,format=raw -serial stdio $(QEMU_NIC) -net user

# =============================================================================
# Rules
# =============================================================================
.PHONY: all clean clean-all distclean qemu qemu-gfx debug gdb dump size bench bench-memory bench-vm bench-scheduler bench-fs bench-gfx stress test bench-compare
.PRECIOUS: $(BUILD_DIR)/%.elf $(BUILD_DIR)/%.bin $(BUILD_DIR)/%.ld

all: $(BUILD_DIR) $(BOOT_BIN) $(OS_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) $(BUILD_DIR)/doom_objs

$(OS_BIN): $(KERNEL_BIN) $(BOOT_BIN) $(BUILD_DIR)/hello.elf $(BUILD_DIR)/doom.elf $(BUILD_DIR)/doom.launch tools/stax-sign/stax-sign tools/launch-pack/launch-pack scripts/create_mbr.py
	@echo ""
	@if [ ! -f $@ ]; then \
		echo "Creating new Flash Image → $@"; \
		dd if=/dev/zero of=$@ bs=1M count=32 2>/dev/null; \
		python3 scripts/create_mbr.py 4099 60000; \
		dd if=mbr.bin of=$@ bs=512 conv=notrunc 2>/dev/null; \
		rm mbr.bin; \
		echo "Formatting FAT16 partition..."; \
		dd if=/dev/zero of=fat.bin bs=512 count=60000 2>/dev/null; \
		mkfs.vfat -F 16 fat.bin; \
		dd if=fat.bin of=$@ bs=512 seek=4099 conv=notrunc 2>/dev/null; \
		rm fat.bin; \
		if [ ! -f stax_key.priv ]; then \
			./tools/stax-sign/stax-sign --gen-key stax_key; \
		fi; \
		mmd -i $@@@2098688 ::/BIN; \
		mmd -i $@@@2098688 ::/DOCS; \
		mmd -i $@@@2098688 ::/DOWNLOADS; \
		mmd -i $@@@2098688 ::/TRASH; \
		mmd -i $@@@2098688 ::/BMP; \
		if [ -d assets/bmp ]; then mcopy -i $@@@2098688 assets/bmp/*.BMP ::/BMP/; fi; \
	fi
	@echo "Signing KERNEL.BIN as Firmware v1..."
	@if [ ! -f stax_key.priv ]; then ./tools/stax-sign/stax-sign --gen-key stax_key; fi
	@./tools/stax-sign/stax-sign --sign $(KERNEL_BIN) --version 1 --key stax_key.priv --output $(BUILD_DIR)/firmware.stax
	@echo "Flashing Firmware to Slot A..."
	@dd if=$(BUILD_DIR)/firmware.stax of=$@ bs=512 seek=3 conv=notrunc 2>/dev/null
	@mcopy -o -i $@@@2098688 build/kernel.bin ::/KERNEL.BIN
	@mcopy -o -i $@@@2098688 build/firmware.stax ::/fw.stax
	@if [ -f $(BUILD_DIR)/hello.elf ]; then \
		mcopy -o -i $@@@2098688 $(BUILD_DIR)/hello.elf ::/HELLO.ELF; \
		mcopy -o -i $@@@2098688 $(BUILD_DIR)/hello.elf ::/BIN/HELLO.ELF; \
	fi
	@if [ -f $(BUILD_DIR)/doom.launch ]; then \
		mcopy -o -i $@@@2098688 $(BUILD_DIR)/doom.launch ::/DOOM.LAUNCH; \
		mcopy -o -i $@@@2098688 $(BUILD_DIR)/doom.launch ::/BIN/DOOM.LAUNCH; \
	fi
	@echo "Build complete → $@"
	@echo "Run:  make qemu"
	@echo "Quit: Ctrl-A then X"
	@echo ""

# ---------------------------------------------------------------------------
# Cryptographic Signing Keys
# ---------------------------------------------------------------------------
stax_key.priv: tools/stax-sign/stax-sign
	@if [ ! -f stax_key.priv ]; then \
		echo "Generating signing key pair..."; \
		./tools/stax-sign/stax-sign --gen-key stax_key; \
	fi

stax_key.pub.h: stax_key.priv

# ---------------------------------------------------------------------------
# Bootloader
# ---------------------------------------------------------------------------
$(BUILD_DIR)/boot_startup.o: $(BOOT_STARTUP) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/bootloader.o: $(BOOT_SRC) $(INC_DIR)/memory_map.h stax_key.pub.h | $(BUILD_DIR)
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

$(BUILD_DIR)/%.o: $(APPS_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(UI_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: crypto/sha256/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: crypto/crc32/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: crypto/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: firmware/image_format/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SHELL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(LIB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: firmware/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: boot/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(BENCH_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(BENCH_DIR)/firmware/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: drivers/net/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: net/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: apps/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lwip/%.o: $(LWIP_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

tools/stax-sign/stax-sign: tools/stax-sign/stax-sign.c firmware/image_format/firmware_format.c crypto/sha256/sha256.c crypto/crc32/crc32.c crypto/monocypher.c
	gcc -O2 -Iinclude tools/stax-sign/stax-sign.c firmware/image_format/firmware_format.c crypto/sha256/sha256.c crypto/crc32/crc32.c crypto/monocypher.c -o tools/stax-sign/stax-sign


# DOOM files need special flags and the stax_compat.h included
$(BUILD_DIR)/%.o: $(GAMES_DIR)/em-doom/linuxdoom-1.10/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -w -O2 -include $(GAMES_DIR)/em-doom/linuxdoom-1.10/stax_compat.h -c $< -o $@

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
# Userland Standalone ELF-32 Binaries
# ---------------------------------------------------------------------------
$(BUILD_DIR)/crt0.o: user/crt0.s | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/ulib.o: user/ulib.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hello.o: user/hello.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hello.elf: $(BUILD_DIR)/crt0.o $(BUILD_DIR)/ulib.o $(BUILD_DIR)/hello.o user/user.ld
	$(LD) -T user/user.ld $(LDFLAGS) -z max-page-size=4096 $(BUILD_DIR)/crt0.o $(BUILD_DIR)/ulib.o $(BUILD_DIR)/hello.o $(LIBGCC) -o $@
	@echo "Built Userland ELF → $@"

$(BUILD_DIR)/doom_objs/%.o: $(GAMES_DIR)/em-doom/linuxdoom-1.10/%.c | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/doom_objs
	$(CC) -mcpu=arm926ej-s -march=armv5te -mthumb-interwork -ffreestanding -nostdlib -nostartfiles -O3 -fomit-frame-pointer -ffast-math -w -std=gnu89 -Iinclude -I$(GAMES_DIR)/em-doom/linuxdoom-1.10 -include $(GAMES_DIR)/em-doom/linuxdoom-1.10/stax_compat.h -c $< -o $@

$(BUILD_DIR)/doom.elf: $(BUILD_DIR)/crt0.o $(BUILD_DIR)/ulib.o $(DOOM_OBJS) user/user.ld
	$(LD) -T user/user.ld $(LDFLAGS) -z max-page-size=4096 $(BUILD_DIR)/crt0.o $(BUILD_DIR)/ulib.o $(DOOM_OBJS) $(LIBGCC) -o $@
	@echo "Built Standalone Userland DOOM ELF → $@"

# Build host-side launch-pack tool
tools/launch-pack/launch-pack: tools/launch-pack/launch-pack.c
	@echo "Building host tool: launch-pack"
	gcc -O2 -Wall -o tools/launch-pack/launch-pack tools/launch-pack/launch-pack.c

# Package doom.launch — requires doom.elf and doom1.wad
$(BUILD_DIR)/doom.launch: $(BUILD_DIR)/doom.elf tools/launch-pack/launch-pack $(GAMES_DIR)/em-doom/manifest.txt
	@echo "Packaging doom.launch..."
	@if [ -f $(GAMES_DIR)/em-doom/doom1.wad ]; then \
		tools/launch-pack/launch-pack $@ $(GAMES_DIR)/em-doom/manifest.txt $(BUILD_DIR)/doom.elf $(GAMES_DIR)/em-doom/doom1.wad; \
	else \
		tools/launch-pack/launch-pack $@ $(GAMES_DIR)/em-doom/manifest.txt $(BUILD_DIR)/doom.elf; \
	fi
	@echo "Built DOOM package → $@"

# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------
qemu: $(BOOT_BIN) $(OS_BIN)
	@echo "Booting STAX in QEMU (Ctrl-A X to quit)..."
	$(QEMU) $(QEMU_FLAGS)

qemu-gfx: $(BOOT_BIN) $(OS_BIN)
	@echo "Booting STAX in QEMU with graphics (Ctrl-C to quit)..."
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
	rm -rf $(BUILD_DIR) tools/stax-sign/stax-sign tools/launch-pack/launch-pack
	@echo "Cleaned build directory. (os.bin is preserved to protect your data)"

clean-all: clean
	rm -f $(OS_BIN)
	@echo "Cleaned everything, including os.bin."

distclean: clean-all

# ---------------------------------------------------------------------------
# Benchmark targets — build + run in QEMU, capture output
# ---------------------------------------------------------------------------
# USAGE: make bench         → full benchmark suite
#        make bench-memory  → memory sub-bench
#        make bench-vm      → VM/page sub-bench
#        make bench-scheduler → scheduler sub-bench
#        make bench-fs      → filesystem sub-bench
#        make bench-gfx     → graphics sub-bench
#        make stress        → stress tests
#        make test          → automated test suite
#        make bench-compare → compare bench/baseline.csv with last run
#
# Results are captured to bench/results.csv (BENCH: prefixed lines)
# ---------------------------------------------------------------------------
bench: $(BOOT_BIN) $(OS_BIN)
	@echo "Running STAX benchmark suite in QEMU..."
	@echo "(Type 'bench' at the STAX prompt, then Ctrl-A X to exit)"
	@echo "Capturing BENCH: CSV lines to bench/results.csv"
	@mkdir -p bench
	$(QEMU) $(QEMU_FLAGS) 2>&1 | tee /tmp/stax_bench_raw.txt | grep '^BENCH:' > bench/results.csv || true
	@echo ""
	@echo "Benchmark CSV saved to bench/results.csv"
	@wc -l bench/results.csv 2>/dev/null && echo "benchmark data points" || echo "(no CSV output captured)"

bench-memory: $(BOOT_BIN) $(OS_BIN)
	@echo "Run 'bench --memory' at the STAX prompt"
	$(QEMU) $(QEMU_FLAGS)

bench-vm: $(BOOT_BIN) $(OS_BIN)
	@echo "Run 'bench --vm' at the STAX prompt"
	$(QEMU) $(QEMU_FLAGS)

bench-scheduler: $(BOOT_BIN) $(OS_BIN)
	@echo "Run 'bench --scheduler' at the STAX prompt"
	$(QEMU) $(QEMU_FLAGS)

bench-fs: $(BOOT_BIN) $(OS_BIN)
	@echo "Run 'bench --fs' at the STAX prompt"
	$(QEMU) $(QEMU_FLAGS)

bench-gfx: $(BOOT_BIN) $(OS_BIN)
	@echo "Run 'bench --gfx' at the STAX prompt"
	$(QEMU) $(QEMU_FLAGS)

stress: $(BOOT_BIN) $(OS_BIN)
	@echo "Run 'stress' at the STAX prompt"
	$(QEMU) $(QEMU_FLAGS)

test: $(BOOT_BIN) $(OS_BIN)
	@echo "Run 'test' at the STAX prompt for automated [PASS]/[FAIL] output"
	$(QEMU) $(QEMU_FLAGS)

# Compare two benchmark result CSV files
# Usage: make bench-compare OLD=bench/baseline.csv NEW=bench/results.csv
bench-compare:
	@OLD=$${OLD:-bench/baseline.csv}; NEW=$${NEW:-bench/results.csv}; \
	if [ ! -f "$$OLD" ]; then echo "No baseline: $$OLD. Run bench first and cp bench/results.csv bench/baseline.csv"; exit 1; fi; \
	if [ ! -f "$$NEW" ]; then echo "No current results: $$NEW. Run bench first."; exit 1; fi; \
	echo "Comparing $$OLD vs $$NEW"; \
	echo "Name,Old_mean,New_mean,Delta%"; \
	awk -F, 'NR==FNR{old[$$1]=$$5; next} { if(old[$$1]>0) { delta=int(($$5-old[$$1])*100/old[$$1]); print $$1 "," old[$$1] "," $$5 "," delta "%" } }' "$$OLD" "$$NEW"
