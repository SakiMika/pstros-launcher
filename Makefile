# Pstros NDS / Java4GBA merged build.
# Simple project layout: TARGET, BUILD, SOURCES, INCLUDES.
# Supports current libnds/Calico/libdvm and older split libfat layouts
# without requiring project-specific devkitPro paths.

TARGET       := pstro_launcher
BUILD        := build
SOURCES      := source
INCLUDES     := include \
                kvm/VmCommon/h \
                kvm/VmSkel/h \
                kvm/VmExtra/h \
                jam/h \
                kvm/VmCommon/src
EMBEDDED_AUDIO := embedded/audio_native.pcm
TEXT1        := Pstro Laucher
TEXT2        := J2ME FAT Test
TEXT3        := Pstros
NDS_GAME_CODE := J2DS
NDS_MAKER_CODE := HB
NDS_INTERNAL_TITLE := PSTROLAUNCH
NDS_ICON := assets/standalone_icon.bmp

# Static launcher metadata.
-include standalone_game.mk

# build_with_log.ps1 passes a real POSIX path such as /d/devkitPro. Keep a
# fallback for users who invoke make directly from the devkitPro MSYS2 shell.
ifeq ($(strip $(DEVKITPRO)),)
  ifneq ($(wildcard /opt/devkitpro/devkitARM),)
    DEVKITPRO := /opt/devkitpro
  else ifneq ($(wildcard /d/devkitPro/devkitARM),)
    DEVKITPRO := /d/devkitPro
  else ifneq ($(wildcard /c/devkitPro/devkitARM),)
    DEVKITPRO := /c/devkitPro
  else
    $(error DEVKITPRO is not set and devkitPro was not found)
  endif
endif
DEVKITARM := $(DEVKITPRO)/devkitARM

# Support both old arm-eabi-* and modern arm-none-eabi-* devkitARM prefixes.
ifneq ($(wildcard $(DEVKITARM)/bin/arm-eabi-gcc*),)
  PREFIX := $(DEVKITARM)/bin/arm-eabi-
else
  PREFIX := $(DEVKITARM)/bin/arm-none-eabi-
endif
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
NDSTOOL ?= ndstool

ARCH    := -march=armv5te -mtune=arm946e-s -mthumb -mthumb-interwork

# libnds 2.x is built on Calico. Its current ARM9 linker specification is
# calico/share/ds9.specs.
CALICO  := $(DEVKITPRO)/calico
SPECS   := -specs=$(CALICO)/share/ds9.specs

DEFS := -D__NDS__ -DARM9 -DNDS -DUNIX -DLINUX -D__arm__ \
        -DUSE_KNI=1 -DENABLE_JAVA_DEBUGGER=0 -DPADTABLE=1 \
        -DDISABLE_VERIFIER -DPRINT_BACKTRACE=1 -DCONSOLE_MD \
        -DENABLE_HEAP_COMPACTION=0 -DCHUNKY_HEAP=0 \
        -DPLATFORMNAME='"NintendoDS"'

# FAT is mandatory at runtime: the launcher scans and loads external JAR files.
# Detect both current and older libfat layouts.
LIBNDS_INC := $(DEVKITPRO)/libnds/include
LIBNDS_LIB := $(DEVKITPRO)/libnds/lib
CALICO_INC := $(CALICO)/include
CALICO_LIB := $(CALICO)/lib

FAT_INC := $(firstword $(foreach d,$(LIBNDS_INC) $(DEVKITPRO)/libfat/include,$(if $(wildcard $(d)/fat.h),$(d))))
FAT_LIB := $(firstword $(foreach d,$(LIBNDS_LIB) $(DEVKITPRO)/libfat/lib,$(if $(wildcard $(d)/libfat.a),$(d))))
STORAGE_INCS := $(sort $(FAT_INC))
STORAGE_LIBS := $(sort $(FAT_LIB))

ARM7_ELF := $(CALICO)/bin/ds7_maine.elf
# Do not redirect printf/fprintf/sprintf with command-line object macros.
# include/kvm_stdio_redirect.h first loads stdio normally, then installs safe
# function-like redirects for KVM source calls only.
CFLAGS  := $(ARCH) -std=gnu99 -O2 -g -ffunction-sections -fdata-sections -fcommon \
           -Wall -Wno-unused-function -Wno-unused-variable -Wno-missing-braces \
           -Wno-pointer-sign -Wno-implicit-function-declaration \
           -Wno-incompatible-pointer-types -Wno-builtin-declaration-mismatch \
           -Wno-strict-aliasing -Wno-dangling-pointer -Wno-misleading-indentation \
           $(DEFS) $(addprefix -I,$(INCLUDES)) -I$(CALICO_INC) -I$(LIBNDS_INC) \
           $(addprefix -I,$(STORAGE_INCS)) -include include/kvm_stdio_redirect.h
LDFLAGS := $(ARCH) $(SPECS) -Wl,-Map,$(BUILD)/$(TARGET).map -Wl,--gc-sections
LIBS    := $(addprefix -L,$(STORAGE_LIBS)) -L$(LIBNDS_LIB) -L$(CALICO_LIB) \
           -lfat -lnds9 -lcalico_ds9 -lm -lgcc

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
OFILES := $(CFILES:.c=.o)
OBJS   := $(addprefix $(BUILD)/,$(OFILES)) $(BUILD)/embedded_audio_native_pcm.o

vpath %.c $(SOURCES)

.PHONY: all clean check env

all: check $(TARGET).nds

check:
	@echo "DEVKITPRO=$(DEVKITPRO)"
	@echo "DEVKITARM=$(DEVKITARM)"
	@echo "FAT header=$(if $(FAT_INC),$(FAT_INC)/fat.h,NOT FOUND)"
	@echo "FAT library=$(if $(FAT_LIB),$(FAT_LIB)/libfat.a,NOT FOUND)"
	@test -x "$(CC)" || (echo "Missing devkitARM compiler: $(CC)"; exit 1)
	@test -f "$(CALICO_INC)/calico.h" || (echo "Missing Calico headers: $(CALICO_INC)"; echo "Install/update with: pacman -Syu --needed nds-dev"; exit 1)
	@test -n "$(FAT_INC)" || (echo "Missing fat.h. Checked $(LIBNDS_INC) and $(DEVKITPRO)/libfat/include"; echo "Install/update with: pacman -Syu --needed nds-dev"; exit 1)
	@test -n "$(FAT_LIB)" || (echo "Missing libfat.a. Checked $(LIBNDS_LIB) and $(DEVKITPRO)/libfat/lib"; echo "Install/update with: pacman -Syu --needed nds-dev"; exit 1)
	@test -f "$(CALICO)/share/ds9.specs" || (echo "Missing Calico ARM9 specs: $(CALICO)/share/ds9.specs"; exit 1)
	@test -f "$(ARM7_ELF)" || (echo "Missing default ARM7 binary: $(ARM7_ELF)"; exit 1)
	@test -f "$(EMBEDDED_AUDIO)" || (echo "Missing native PCM pack: $(EMBEDDED_AUDIO)"; exit 1)
	@test -f "$(NDS_ICON)" || (echo "Missing generated NDS icon: $(NDS_ICON)"; echo "Run build.bat to prepare it from the JAR"; exit 1)
	@test -f "include/standalone_game.h" || (echo "Missing generated game config: include/standalone_game.h"; echo "Run build.bat"; exit 1)
	@grep -q '^int pstrosConfigureSaveStorage(void)' kvm/VmSkel/src/Java_nds_File.c || (echo "Missing pstrosConfigureSaveStorage implementation"; exit 1)
	@grep -q '^const char \*pstrosGetSavePath(void)' kvm/VmSkel/src/Java_nds_File.c || (echo "Missing pstrosGetSavePath implementation"; exit 1)
	@grep -q '^int pstrosGetSaveErrno(void)' kvm/VmSkel/src/Java_nds_File.c || (echo "Missing pstrosGetSaveErrno implementation"; exit 1)
	@command -v $(NDSTOOL) >/dev/null 2>&1 || (echo "Missing ndstool in PATH"; exit 1)

# This target is intentionally verbose so last_build.log shows every resolved
# SDK path when a future toolchain update changes the layout.
env:
	@echo "TARGET=$(TARGET)"
	@echo "BUILD=$(BUILD)"
	@echo "SOURCES=$(SOURCES)"
	@echo "INCLUDES=$(INCLUDES)"
	@echo "CC=$(CC)"
	@echo "DEVKITPRO=$(DEVKITPRO)"
	@echo "LIBNDS_INC=$(LIBNDS_INC)"
	@echo "LIBNDS_LIB=$(LIBNDS_LIB)"
	@echo "FAT_INC=$(FAT_INC)"
	@echo "FAT_LIB=$(FAT_LIB)"
	@echo "EMBEDDED_AUDIO=$(EMBEDDED_AUDIO)"

$(TARGET).nds: $(BUILD)/$(TARGET).elf
	$(NDSTOOL) -c $@ -9 $< -7 $(ARM7_ELF) -b $(NDS_ICON) \
		"$(TEXT1);$(TEXT2);$(TEXT3)" \
		-g $(NDS_GAME_CODE) $(NDS_MAKER_CODE) $(NDS_INTERNAL_TITLE) 0


# External JAR files are loaded from FAT at runtime.

# Link an empty PCM table only to satisfy the shared video/media native ABI.
# The launcher always starts MIDlets with -mute and does not provide audio.
$(BUILD)/embedded_audio_native_pcm.o: $(EMBEDDED_AUDIO) | $(BUILD)
	cp $< $(BUILD)/embedded_audio_native.pcm
	cd $(BUILD) && $(OBJCOPY) -I binary -O elf32-littlearm -B arm \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		embedded_audio_native.pcm embedded_audio_native_pcm.o

# Link the known-good first-boot RMS template into ARM9 as read-only data.
$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# printf.c implements the redirected functions, so do not apply the call macros
# while compiling that one translation unit.
$(BUILD)/vmskel_05_printf.o: CFLAGS += -DKVM_STDIO_IMPLEMENTATION=1

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(TARGET).nds
