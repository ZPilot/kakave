# kakave firmware - command-line build of firmware401/ (STM32F401) and
# firmware/ (STM32F411), with the same flags and link order as the
# STM32CubeIDE 1.4 Release build that produced */Release/*.bin.
#
#   make            build/f401test005.{elf,bin} and build/f411test005.{elf,bin}
#   make f401       only the STM32F401 one
#   make f411       only the STM32F411 one
#   make release    build both and copy them into firmware401/Release/ and
#                   firmware/Release/
#   make clean
#
# Toolchain: arm-none-eabi-gcc from PATH, or TOOLCHAIN=<dir with arm-none-eabi-gcc>.
# The files in */Release/ are built with GNU Arm Embedded 7-2018-q2-update
# (GCC 7.3.1 20180622), the compiler STM32CubeIDE 1.4 used; with it this
# Makefile gives exactly those files. Newer GCC builds work too (checked:
# 13.2), but generate different code.

TOOLCHAIN ?=
PREFIX    := $(if $(TOOLCHAIN),$(TOOLCHAIN)/)arm-none-eabi-
CC        := $(PREFIX)gcc
OBJCOPY   := $(PREFIX)objcopy
SIZE      := $(PREFIX)size
OUT       ?= build

MCU    := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS   := -DUSE_FULL_LL_DRIVER -DHSE_VALUE=25000000 -DHSE_STARTUP_TIMEOUT=100 \
          -DLSE_STARTUP_TIMEOUT=5000 -DLSE_VALUE=32768 \
          -DEXTERNAL_CLOCK_VALUE=12288000 -DHSI_VALUE=16000000 \
          -DLSI_VALUE=32000 -DVDD_VALUE=3300 \
          -DPREFETCH_ENABLE=1 -DINSTRUCTION_CACHE_ENABLE=1 -DDATA_CACHE_ENABLE=1
CFLAGS  = $(MCU) $(DEFS) $(INCS) -O0 -std=gnu11 -ffunction-sections -fdata-sections -Wall
ASFLAGS = $(MCU) -x assembler-with-cpp
LDFLAGS = $(MCU) -T$(SRC_DIR)/$(LDSCRIPT) --specs=nosys.specs --specs=nano.specs \
          -Wl,--gc-sections -Wl,-Map=$(OUT)/$(NAME).map -static \
          -Wl,--start-group -lc -lm -Wl,--end-group

.PHONY: all f401 f411 release clean
all: f401 f411

f401:
	$(MAKE) -f Makefile build SRC_DIR=firmware401 NAME=f401test005 \
	    LDSCRIPT=STM32F401CCUX_FLASH.ld CHIP=-DSTM32F401xC
f411:
	$(MAKE) -f Makefile build SRC_DIR=firmware NAME=f411test005 \
	    LDSCRIPT=STM32F411CEUX_FLASH.ld CHIP=-DSTM32F411xE

release: all
	cp $(OUT)/f401test005.bin $(OUT)/f401test005.elf firmware401/Release/
	cp $(OUT)/f411test005.bin firmware/Release/

clean:
	rm -rf $(OUT)

ifdef SRC_DIR
DEFS += $(CHIP)
INCS := -I$(SRC_DIR)/Core/Inc \
        -I$(SRC_DIR)/Drivers/STM32F4xx_HAL_Driver/Inc \
        -I$(SRC_DIR)/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
        -I$(SRC_DIR)/Drivers/CMSIS/Include
OBJ_DIR := $(OUT)/obj/$(SRC_DIR)

# Link order matters for a byte-identical image: CubeIDE's objects.list.
objs = $(sort $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename \
        $(wildcard $(SRC_DIR)/$(1)/*.c $(SRC_DIR)/$(1)/*.s))))
OBJS := $(call objs,Core/Src/ctrl128) $(call objs,Core/Src) \
        $(call objs,Core/Src/ff14) $(call objs,Core/Src/ssd1306) \
        $(call objs,Core/Startup) $(call objs,Drivers/STM32F4xx_HAL_Driver/Src)

.PHONY: build
build: $(OUT)/$(NAME).bin

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OUT)/$(NAME).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(OUT)/$(NAME).bin: $(OUT)/$(NAME).elf
	$(OBJCOPY) -O binary $< $@
endif
