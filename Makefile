TARGET ?= iQueDiagExtend

ifeq ($(DEBUG),1)
	OPTFLAGS ?= -DEBUG
	EXTFLAGS ?= -D DEBUG -Zi -nologo
	OUT_DIR  ?= Debug
else
	OPTFLAGS ?= -O1 -Gy -Oi -Zc:inline -GL -MD
	EXTFLAGS ?= -nologo
	OUT_DIR  ?= Release
endif

CFLAGS := -std:c17 -W4 $(OPTFLAGS) $(EXTFLAGS)
ASMFLAGS := $(EXTFLAGS)

BUILD_DIR := $(TARGET)/$(OUT_DIR)

INC_FLAGS := -I $(TARGET) -I $(TARGET)/include -I $(TARGET)/include/MinHook -I $(TARGET)/include/MinHook/hde

SRC_FILES := $(TARGET)/dllmain.c $(TARGET)/MinHook/buffer.c $(TARGET)/MinHook/hde/hde32.c $(TARGET)/MinHook/hde/hde64.c $(TARGET)/MinHook/hook.c $(TARGET)/MinHook/trampoline.c $(TARGET)/export.asm
OBJ_FILES := $(foreach file,$(SRC_FILES),$(BUILD_DIR)/$(addsuffix .obj,$(file)))

CC  := cl.exe
AS  := ml.exe

all: $(OUT_DIR)/$(TARGET).dll

$(BUILD_DIR)/%.c.obj: %.c
	mkdir -p $(dir $@)
	$(CC) $(INC_FLAGS) $(CFLAGS) -Fo"$@" -c $<
	
$(BUILD_DIR)/%.asm.obj: %.asm
	mkdir -p $(dir $@)
	$(AS) $(ASMFLAGS) -Fo"$@" -c $<

$(OUT_DIR)/$(TARGET).dll: $(OBJ_FILES)
	mkdir -p $(dir $@)
	$(CC) $(OBJ_FILES) -LD $(INC_FLAGS) $(CFLAGS) -Fe"$@"

clean:
	$(RM) -r $(BUILD_DIR) $(OUT_DIR)

.PHONY: all clean
