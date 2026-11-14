
TARGET  := XAVRX_PSP_3DS
BUILD   := build
SOURCES := source
INCLUDES := include

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard

CFLAGS := -g -Wall -O2 -mword-relocations \
          -ffunction-sections -fdata-sections $(ARCH)

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions

LDFLAGS := -specs=3dsx.specs -Wl,--gc-sections

LIBS := -lctru

include $(DEVKITPRO)/libctru/3ds_rules
