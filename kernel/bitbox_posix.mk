# Bitbox Makefile helper for native targets (also called emulators)

# TARGETS
# --------
# $NAME_$BOARD (default), clean

# Variables used (export them)
# --------------
#   TYPE= sdl | test
#   BITBOX NAME GAME_C_FILES DEFINES (VGA_MODE, VGA_BPP, ...)
#   GAME_C_OPTS DEFINES NO_USB NO_AUDIO USE_SDCARD
# More arcane defines :
#   USE_SD_SENSE DISABLE_ESC_EXIT KEYB_FR

HOST = $(shell uname)
$(warning compiling $(NAME) with c files $(GAME_C_FILES))
DEFINES += EMULATOR

BUILD_DIR := build/$(TYPE)

VPATH=.:$(BITBOX)/kernel:$(BITBOX)/

INCLUDES=-I$(BITBOX)/kernel/  -I$(BITBOX)

# language specific (not specific to target)
FLAGS = -g -Wall \
    -ffast-math -fsingle-precision-constant -fsigned-char \
    -ffunction-sections -fdata-sections -funroll-loops -fomit-frame-pointer

AUTODEPENDENCY_CFLAGS=-MMD -MF$(@:.o=.d) -MT$@

# -- Target-specifics
ifeq ($(HOST), Haiku)
  HOSTLIBS =
else
  HOSTLIBS = -lm -lc -lstdc++
endif
ifeq ($(HOST), Darwin)
  CFLAGS += -O0
  LD_FLAGS = -dead_strip
else
  CFLAGS += -Og
  LD_FLAGS = -Wl,--gc-sections
endif

CC=gcc
CXX=g++

ifeq ($(TYPE), sdl)
  CPPFLAGS += $(shell sdl-config --cflags)
  HOSTLIBS += $(shell sdl-config --libs)
else ifeq ($(TYPE), test)
else 
  $(error unknown type $(TYPE) defined, please use sdl or test)
endif

KERNEL := bitbox_main.c main_$(TYPE).c micro_palette.c

# -- Optional features

ifdef USE_SDCARD
  DEFINES += USE_SDCARD
endif
ifdef NO_USB
  DEFINES += NO_USB
endif
ifdef NO_AUDIO
  DEFINES += NO_AUDIO
endif

# --- Compilation
CPPFLAGS += $(DEFINES:%=-D%) $(INCLUDES)
FLAGS 	 += $(GAME_C_OPTS) $(AUTODEPENDENCY_CFLAGS)

CFLAGS = -std=c99  $(FLAGS)
CXXFLAGS = -std=c++14 $(FLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS)    $(CPPFLAGS) -c $< -o $@
	@echo CC $<

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@
	@echo C++ $<

# --- link
CFILES   := $(filter %.c,$(GAME_C_FILES))
$(info CFILES $(CFILES))
CXXFILES := $(filter %.cpp,$(GAME_C_FILES))
OBJFILES := $(CFILES:%.c=$(BUILD_DIR)/%.o) $(CXXFILES:%.cpp=$(BUILD_DIR)/%.o) $(KERNEL:%.c=$(BUILD_DIR)/%.o)

$(NAME)_$(TYPE): $(OBJFILES)
	$(CC) $(LD_FLAGS) $^ -o $@ $(HOSTLIBS)

# --- Autodependecies (headers...)
-include $(BUILD_DIR)/*.d

# --- Helpers

# double colon to allow extra cleaning
clean::
	rm -rf $(BUILD_DIR) $(NAME)_$(TYPE)

.PHONY: clean
