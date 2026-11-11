# Makefile for fn_console_unlocker (sploit.sprx) - Reconstructed
#
# Richiede:
#   - OpenOrbis PS4 Toolchain (export OO_PS4_TOOLCHAIN=/path/to/toolchain)
#   - GoldHEN Plugins SDK      (export GOLDHEN_SDK=/path/to/sdk)
#
# Build: make
# Clean: make clean

# Toolchain
ifdef OO_PS4_TOOLCHAIN
  CC      := $(OO_PS4_TOOLCHAIN)/bin/clang
  LD      := $(OO_PS4_TOOLCHAIN)/bin/ld.lld
  OBJCOPY := $(OO_PS4_TOOLCHAIN)/bin/objcopy
else
  CC      := clang
  LD      := ld.lld
  OBJCOPY := objcopy
endif

# Directories
SRC_DIR    := source
COMMON_DIR := common
INC_DIR    := include
OBJ_DIR    := build
BIN_DIR    := bin

# Output
TARGET := $(BIN_DIR)/fn_console_unlocker.sprx
ELF    := $(OBJ_DIR)/fn_console_unlocker.elf

# Sources
SRCS := $(SRC_DIR)/main.c $(COMMON_DIR)/plugin_common.c
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(SRCS)))

# GoldHEN SDK includes and sources
ifdef GOLDHEN_SDK
  GH_INC := -I$(GOLDHEN_SDK)/include
  GH_SRC := $(wildcard $(GOLDHEN_SDK)/source/*.c)
  GH_OBJS := $(patsubst %.c,$(OBJ_DIR)/gh_%.o,$(notdir $(GH_SRC)))
else
  GH_INC :=
  GH_SRC :=
  GH_OBJS :=
endif

# Flags
CFLAGS  := -target x86_64-scei-ps4 -ffreestanding -fPIC -fno-builtin
CFLAGS  += -I$(INC_DIR) $(GH_INC)
CFLAGS  += -Wall -Wno-unused-variable -Wno-unused-function
CFLAGS  += -O2

LDFLAGS := -shared -z now --eh-frame-hdr
ifdef OO_PS4_TOOLCHAIN
  LDFLAGS += -L$(OO_PS4_TOOLCHAIN)/lib
endif
LDFLAGS += -lkernel -lc -lSceUserService -lSceSysmodule

# Rules
all: dirs $(TARGET)

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/plugin_common.o: $(COMMON_DIR)/plugin_common.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/gh_%.o: $(GOLDHEN_SDK)/source/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS) $(GH_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(TARGET): $(ELF)
	@echo "Creating SPRX from ELF..."
	@# OpenOrbis create-fself or similar tool
	@# create-fself --paid 0x3800000000000011 $(ELF) $(TARGET)
	@cp $(ELF) $(TARGET)
	@echo "Built: $(TARGET)"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all dirs clean
