# MOON ENG
# Portable DJGPP build for ZEUS, MDPed, Tmuse, CGUI and MOON.

CONFIG ?= release
VALID_CONFIGS := debug release

ifeq ($(filter $(CONFIG),$(VALID_CONFIGS)),)
$(error Unsupported CONFIG '$(CONFIG)'; choose debug or release)
endif

ifdef COMSPEC
ifeq ($(origin CC),default)
CC := C:/djgpp/bin/gcc.exe
endif
ifeq ($(origin CC),undefined)
CC := C:/djgpp/bin/gcc.exe
endif
else
ifeq ($(origin CC),default)
CC := i586-pc-msdosdjgpp-gcc
endif
ifeq ($(origin CC),undefined)
CC := i586-pc-msdosdjgpp-gcc
endif
endif

BUILD_ROOT ?= build
DIST_ROOT ?= dist
OBJ_DIR := $(BUILD_ROOT)/obj/$(CONFIG)
DEP_DIR := $(BUILD_ROOT)/dep/$(CONFIG)
DOS_DIR := $(BUILD_ROOT)/dos/$(CONFIG)
GAME_DIST_ROOT := $(DIST_ROOT)/game
TOOLS_DIST_ROOT := $(DIST_ROOT)/tools
GAME_DIST_DIR := $(GAME_DIST_ROOT)/$(CONFIG)
TOOLS_DIST_DIR := $(TOOLS_DIST_ROOT)/$(CONFIG)

CPPFLAGS ?=
# GNU99 keeps C99 semantics while exposing DJGPP's required uclock extensions.
CFLAGS_COMMON := -std=gnu99 -Wall -Wextra
CFLAGS_release := -O2 -DNDEBUG
CFLAGS_debug := -O0 -g3 -DMOON_DEBUG=1
CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_$(CONFIG))
DEPFLAGS = -MMD -MP -MF $(DEP_DIR)/$*.d -MT $@
LDFLAGS ?=
LDLIBS := -lm

ZEUS_SRC := main.c map.c input.c render.c console.c player.c physics.c hud.c skybox.c sprite.c
MDPED_SRC := mdped/mdped.c cgui/cgui-main.c cgui/cgui-widgets.c cgui/cgui-font.c
TMUSE_SRC := tmuse/tmuse-main.c tmuse/tmuse-tui.c tmuse/tmuse-dsp.c tmuse/tmuse-mix.c tmuse/tmuse-io.c tmuse/tmuse-music.c tmuse/tmuse-dash.c cgui/cgui-input.c input.c
TMUSEGUI_SRC := tmuse/tmuse-gui.c tmuse/tmuse-main.c tmuse/tmuse-dsp.c tmuse/tmuse-mix.c tmuse/tmuse-io.c tmuse/tmuse-music.c tmuse/tmuse-dash.c cgui/cgui-main.c cgui/cgui-widgets.c cgui/cgui-input.c cgui/cgui-font.c input.c
MOON_SRC := moon.c

source_objects = $(addprefix $(OBJ_DIR)/,$(1:.c=.o))
source_deps = $(addprefix $(DEP_DIR)/,$(1:.c=.d))

ZEUS_OBJ := $(call source_objects,$(ZEUS_SRC))
MDPED_OBJ := $(call source_objects,$(MDPED_SRC))
TMUSE_OBJ := $(call source_objects,$(TMUSE_SRC))
TMUSEGUI_OBJ := $(call source_objects,$(TMUSEGUI_SRC))
MOON_OBJ := $(call source_objects,$(MOON_SRC))

ALL_OBJ := $(sort $(ZEUS_OBJ) $(MDPED_OBJ) $(TMUSE_OBJ) $(TMUSEGUI_OBJ) $(MOON_OBJ))
ALL_DEP := $(sort \
	$(call source_deps,$(ZEUS_SRC)) \
	$(call source_deps,$(MDPED_SRC)) \
	$(call source_deps,$(TMUSE_SRC)) \
	$(call source_deps,$(TMUSEGUI_SRC)) \
	$(call source_deps,$(MOON_SRC)))

ZEUS_BIN := $(DOS_DIR)/zeus.exe
MDPED_BIN := $(DOS_DIR)/mdped.exe
TMUSE_BIN := $(DOS_DIR)/tmuse.exe
TMUSEGUI_BIN := $(DOS_DIR)/tmusegui.exe
MOON_BIN := $(DOS_DIR)/moon.exe
ALL_BIN := $(ZEUS_BIN) $(MDPED_BIN) $(TMUSE_BIN) $(TMUSEGUI_BIN) $(MOON_BIN)

SOURCE_DIRS := cgui mdped tmuse
BUILD_DIRS := \
	$(BUILD_ROOT) \
	$(BUILD_ROOT)/obj $(OBJ_DIR) $(addprefix $(OBJ_DIR)/,$(SOURCE_DIRS)) \
	$(BUILD_ROOT)/dep $(DEP_DIR) $(addprefix $(DEP_DIR)/,$(SOURCE_DIRS)) \
	$(BUILD_ROOT)/dos $(DOS_DIR)

.PHONY: all zeus mdped tmuse tmusegui moon runtime debug release dist dist-game dist-tools test clean help

all: zeus mdped tmuse tmusegui moon runtime

zeus: $(ZEUS_BIN)
mdped: $(MDPED_BIN)
tmuse: $(TMUSE_BIN)
tmusegui: $(TMUSEGUI_BIN)
moon: $(MOON_BIN)

RUNTIME_FILES := \
	$(DOS_DIR)/CWSDPMI.EXE \
	$(DOS_DIR)/GAME.MDP \
	$(DOS_DIR)/PALETTE.BMP \
	$(DOS_DIR)/MOON.BAT \
	$(DOS_DIR)/RUNTMUSE.BAT

runtime: $(RUNTIME_FILES)

debug:
	$(MAKE) CONFIG=debug all

release:
	$(MAKE) CONFIG=release all

$(ZEUS_BIN): $(ZEUS_OBJ) | $(DOS_DIR)
	$(CC) $(LDFLAGS) -o $@ $(ZEUS_OBJ) $(LDLIBS)

$(MDPED_BIN): $(MDPED_OBJ) | $(DOS_DIR)
	$(CC) $(LDFLAGS) -o $@ $(MDPED_OBJ) $(LDLIBS)

$(TMUSE_BIN): $(TMUSE_OBJ) | $(DOS_DIR)
	$(CC) $(LDFLAGS) -o $@ $(TMUSE_OBJ) $(LDLIBS)

$(TMUSEGUI_BIN): $(TMUSEGUI_OBJ) | $(DOS_DIR)
	$(CC) $(LDFLAGS) -o $@ $(TMUSEGUI_OBJ) $(LDLIBS)

$(MOON_BIN): $(MOON_OBJ) | $(DOS_DIR)
	$(CC) $(LDFLAGS) -o $@ $(MOON_OBJ) $(LDLIBS)

$(OBJ_DIR)/%.o: %.c | $(BUILD_DIRS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

ifdef COMSPEC
define make_directory
	@if not exist $(subst /,\,$@)\NUL mkdir $(subst /,\,$@)
endef
define copy_file
	@copy /Y $(subst /,\,$<) $(subst /,\,$@) >NUL
endef
else
define make_directory
	@mkdir -p "$@"
endef
define copy_file
	@cp "$<" "$@"
endef
endif

$(BUILD_ROOT):
	$(make_directory)

$(BUILD_ROOT)/obj $(BUILD_ROOT)/dep $(BUILD_ROOT)/dos: | $(BUILD_ROOT)
	$(make_directory)

$(OBJ_DIR): | $(BUILD_ROOT)/obj
	$(make_directory)

$(DEP_DIR): | $(BUILD_ROOT)/dep
	$(make_directory)

$(DOS_DIR): | $(BUILD_ROOT)/dos
	$(make_directory)

$(addprefix $(OBJ_DIR)/,$(SOURCE_DIRS)): | $(OBJ_DIR)
	$(make_directory)

$(addprefix $(DEP_DIR)/,$(SOURCE_DIRS)): | $(DEP_DIR)
	$(make_directory)

ifdef COMSPEC
$(GAME_DIST_ROOT) $(TOOLS_DIST_ROOT):
	@if not exist $(subst /,\,$(DIST_ROOT))\NUL mkdir $(subst /,\,$(DIST_ROOT))
	$(make_directory)

$(GAME_DIST_DIR): | $(GAME_DIST_ROOT)
	$(make_directory)

$(TOOLS_DIST_DIR): | $(TOOLS_DIST_ROOT)
	$(make_directory)
else
$(GAME_DIST_ROOT) $(TOOLS_DIST_ROOT) $(GAME_DIST_DIR) $(TOOLS_DIST_DIR):
	$(make_directory)
endif

$(DOS_DIR)/CWSDPMI.EXE: CWSDPMI.EXE | $(DOS_DIR)
	$(copy_file)
$(DOS_DIR)/GAME.MDP: GAME.MDP | $(DOS_DIR)
	$(copy_file)
$(DOS_DIR)/PALETTE.BMP: palette.bmp | $(DOS_DIR)
	$(copy_file)
$(DOS_DIR)/MOON.BAT: moon.bat | $(DOS_DIR)
	$(copy_file)
$(DOS_DIR)/RUNTMUSE.BAT: runtmuse.bat | $(DOS_DIR)
	$(copy_file)

GAME_DIST_FILES := \
	$(GAME_DIST_DIR)/ZEUS.EXE \
	$(GAME_DIST_DIR)/CWSDPMI.EXE \
	$(GAME_DIST_DIR)/GAME.MDP

TOOLS_DIST_FILES := \
	$(TOOLS_DIST_DIR)/MDPED.EXE \
	$(TOOLS_DIST_DIR)/TMUSE.EXE \
	$(TOOLS_DIST_DIR)/TMUSEGUI.EXE \
	$(TOOLS_DIST_DIR)/MOON.EXE \
	$(TOOLS_DIST_DIR)/CWSDPMI.EXE \
	$(TOOLS_DIST_DIR)/PALETTE.BMP \
	$(TOOLS_DIST_DIR)/MOON.BAT \
	$(TOOLS_DIST_DIR)/RUNTMUSE.BAT

dist: dist-game dist-tools

dist-game: $(GAME_DIST_FILES)
	@echo Game distribution staged in $(GAME_DIST_DIR)

dist-tools: $(TOOLS_DIST_FILES)
	@echo Tool distribution staged in $(TOOLS_DIST_DIR)

$(GAME_DIST_DIR)/ZEUS.EXE: $(ZEUS_BIN) | $(GAME_DIST_DIR)
	$(copy_file)
$(GAME_DIST_DIR)/CWSDPMI.EXE: $(DOS_DIR)/CWSDPMI.EXE | $(GAME_DIST_DIR)
	$(copy_file)
$(GAME_DIST_DIR)/GAME.MDP: $(DOS_DIR)/GAME.MDP | $(GAME_DIST_DIR)
	$(copy_file)

$(TOOLS_DIST_DIR)/MDPED.EXE: $(MDPED_BIN) | $(TOOLS_DIST_DIR)
	$(copy_file)
$(TOOLS_DIST_DIR)/TMUSE.EXE: $(TMUSE_BIN) | $(TOOLS_DIST_DIR)
	$(copy_file)
$(TOOLS_DIST_DIR)/TMUSEGUI.EXE: $(TMUSEGUI_BIN) | $(TOOLS_DIST_DIR)
	$(copy_file)
$(TOOLS_DIST_DIR)/MOON.EXE: $(MOON_BIN) | $(TOOLS_DIST_DIR)
	$(copy_file)
$(TOOLS_DIST_DIR)/CWSDPMI.EXE: $(DOS_DIR)/CWSDPMI.EXE | $(TOOLS_DIST_DIR)
	$(copy_file)
$(TOOLS_DIST_DIR)/PALETTE.BMP: $(DOS_DIR)/PALETTE.BMP | $(TOOLS_DIST_DIR)
	$(copy_file)
$(TOOLS_DIST_DIR)/MOON.BAT: $(DOS_DIR)/MOON.BAT | $(TOOLS_DIST_DIR)
	$(copy_file)
$(TOOLS_DIST_DIR)/RUNTMUSE.BAT: $(DOS_DIR)/RUNTMUSE.BAT | $(TOOLS_DIST_DIR)
	$(copy_file)

test: all
	@echo Compile/link smoke test passed for CONFIG=$(CONFIG).
	@echo Run linbuild.sh or winbuild.sh for the vanilla DOSBox runtime gate.

ifdef COMSPEC
ifeq ($(OS),Windows_NT)
clean:
	@if exist $(subst /,\,$(BUILD_ROOT))\NUL rmdir /S /Q $(subst /,\,$(BUILD_ROOT))
	@if exist $(subst /,\,$(DIST_ROOT))\NUL rmdir /S /Q $(subst /,\,$(DIST_ROOT))
else
clean:
	@if exist $(subst /,\,$(BUILD_ROOT))\NUL deltree /Y $(subst /,\,$(BUILD_ROOT))
	@if exist $(subst /,\,$(DIST_ROOT))\NUL deltree /Y $(subst /,\,$(DIST_ROOT))
endif
else
clean:
	rm -rf -- "$(BUILD_ROOT)" "$(DIST_ROOT)"
endif

help:
	@echo "MOON ENG build targets:"
	@echo "  all zeus mdped tmuse tmusegui moon runtime"
	@echo "  test dist dist-game dist-tools clean debug release"
	@echo "Select a configuration with CONFIG=debug or CONFIG=release."

-include $(ALL_DEP)
