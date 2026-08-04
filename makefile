# MOON ENG
# Portable DJGPP build for ZEUS, MDPed, Tmuse, CGUI and MOON.

CONFIG ?= release
DOS_SHELL ?= 0
VALID_CONFIGS := debug release

ifeq ($(filter $(CONFIG),$(VALID_CONFIGS)),)
$(error Unsupported CONFIG '$(CONFIG)'; choose debug or release)
endif

ifeq ($(DOS_SHELL),1)
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
HOST_ROOT := $(BUILD_ROOT)/host
HOST_OBJ_DIR := $(HOST_ROOT)/obj/$(CONFIG)
HOST_DEP_DIR := $(HOST_ROOT)/dep/$(CONFIG)
HOST_BIN_DIR := $(HOST_ROOT)/bin/$(CONFIG)
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
MDP_CPPFLAGS := $(CPPFLAGS) -Iinclude

HOST_CC ?= cc
HOST_CPPFLAGS ?=
HOST_CFLAGS_COMMON := -std=c99 -Wall -Wextra -Wpedantic
HOST_CFLAGS ?= $(HOST_CFLAGS_COMMON) $(CFLAGS_$(CONFIG))
HOST_LDFLAGS ?=

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
MDP_CORE_OBJ := $(OBJ_DIR)/mdp-core.o
MDP_MAP_OBJ := $(OBJ_DIR)/mdp-map.o
MDPC_OBJ := $(OBJ_DIR)/mdpc.o
MDPTEST_DOS_OBJ := $(OBJ_DIR)/mdptest.o
MDPMAPTEST_DOS_OBJ := $(OBJ_DIR)/maptest.o

MDP_CORE_DEP := $(DEP_DIR)/mdp-core.d
MDP_MAP_DEP := $(DEP_DIR)/mdp-map.d
MDPC_DEP := $(DEP_DIR)/mdpc.d
MDPTEST_DOS_DEP := $(DEP_DIR)/mdptest.d
MDPMAPTEST_DOS_DEP := $(DEP_DIR)/maptest.d

HOST_MDP_CORE_OBJ := $(HOST_OBJ_DIR)/mdp-core.o
HOST_MDP_MAP_OBJ := $(HOST_OBJ_DIR)/mdp-map.o
HOST_MDPC_OBJ := $(HOST_OBJ_DIR)/mdpc.o
HOST_MDP_TEST_OBJ := $(HOST_OBJ_DIR)/mdptest.o
HOST_MDP_MAP_TEST_OBJ := $(HOST_OBJ_DIR)/maptest.o
HOST_MDP_CORE_DEP := $(HOST_DEP_DIR)/mdp-core.d
HOST_MDP_MAP_DEP := $(HOST_DEP_DIR)/mdp-map.d
HOST_MDPC_DEP := $(HOST_DEP_DIR)/mdpc.d
HOST_MDP_TEST_DEP := $(HOST_DEP_DIR)/mdptest.d
HOST_MDP_MAP_TEST_DEP := $(HOST_DEP_DIR)/maptest.d
HOST_DEP := $(HOST_MDP_CORE_DEP) $(HOST_MDP_MAP_DEP) $(HOST_MDPC_DEP) \
	$(HOST_MDP_TEST_DEP) $(HOST_MDP_MAP_TEST_DEP)

ALL_OBJ := $(sort $(ZEUS_OBJ) $(MDPED_OBJ) $(TMUSE_OBJ) $(TMUSEGUI_OBJ) \
	$(MOON_OBJ) $(MDP_CORE_OBJ) $(MDP_MAP_OBJ) $(MDPC_OBJ) \
	$(MDPTEST_DOS_OBJ) $(MDPMAPTEST_DOS_OBJ))
ALL_DEP := $(sort \
	$(call source_deps,$(ZEUS_SRC)) \
	$(call source_deps,$(MDPED_SRC)) \
	$(call source_deps,$(TMUSE_SRC)) \
	$(call source_deps,$(TMUSEGUI_SRC)) \
	$(call source_deps,$(MOON_SRC)) \
	$(MDP_CORE_DEP) $(MDP_MAP_DEP) $(MDPC_DEP) $(MDPTEST_DOS_DEP) \
	$(MDPMAPTEST_DOS_DEP))

ZEUS_BIN := $(DOS_DIR)/zeus.exe
MDPED_BIN := $(DOS_DIR)/mdped.exe
TMUSE_BIN := $(DOS_DIR)/tmuse.exe
TMUSEGUI_BIN := $(DOS_DIR)/tmusegui.exe
MOON_BIN := $(DOS_DIR)/moon.exe
MDPC_BIN := $(DOS_DIR)/mdpc.exe
MDPTEST_DOS_BIN := $(DOS_DIR)/mdptest.exe
MDPMAPTEST_DOS_BIN := $(DOS_DIR)/maptest.exe
HOST_MDPC_BIN := $(HOST_BIN_DIR)/mdpc
HOST_MDP_TEST_BIN := $(HOST_BIN_DIR)/mdptest
HOST_MDP_MAP_TEST_BIN := $(HOST_BIN_DIR)/maptest
ALL_BIN := $(ZEUS_BIN) $(MDPED_BIN) $(TMUSE_BIN) $(TMUSEGUI_BIN) \
	$(MOON_BIN) $(MDPC_BIN)

SOURCE_DIRS := cgui mdped tmuse
BUILD_DIRS := \
	$(BUILD_ROOT) \
	$(BUILD_ROOT)/obj $(OBJ_DIR) $(addprefix $(OBJ_DIR)/,$(SOURCE_DIRS)) \
	$(BUILD_ROOT)/dep $(DEP_DIR) $(addprefix $(DEP_DIR)/,$(SOURCE_DIRS)) \
	$(BUILD_ROOT)/dos $(DOS_DIR)

.PHONY: all zeus mdped tmuse tmusegui moon mdpc mdp-test-dos \
	mdp-map-test-dos test runtime \
	debug release dist dist-game dist-tools dosbox-smoke clean help

all: zeus mdped tmuse tmusegui moon mdpc runtime

zeus: $(ZEUS_BIN)
mdped: $(MDPED_BIN)
tmuse: $(TMUSE_BIN)
tmusegui: $(TMUSEGUI_BIN)
moon: $(MOON_BIN)
mdpc: $(MDPC_BIN)
mdp-test-dos: $(MDPTEST_DOS_BIN)
mdp-map-test-dos: $(MDPMAPTEST_DOS_BIN)

RUNTIME_FILES := \
	$(DOS_DIR)/CWSDPMI.EXE \
	$(DOS_DIR)/GAME.MDP \
	$(DOS_DIR)/PALETTE.BMP

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

$(MDPC_BIN): $(MDPC_OBJ) $(MDP_CORE_OBJ) $(MDP_MAP_OBJ) | $(DOS_DIR)
	$(CC) $(LDFLAGS) -o $@ $(MDPC_OBJ) $(MDP_CORE_OBJ) $(MDP_MAP_OBJ)

$(MDPTEST_DOS_BIN): $(MDPTEST_DOS_OBJ) $(MDP_CORE_OBJ) | $(DOS_DIR)
	$(CC) $(LDFLAGS) -o $@ $(MDPTEST_DOS_OBJ) $(MDP_CORE_OBJ)

$(MDPMAPTEST_DOS_BIN): $(MDPMAPTEST_DOS_OBJ) $(MDP_MAP_OBJ) \
		$(MDP_CORE_OBJ) | $(DOS_DIR)
	$(CC) $(LDFLAGS) -o $@ $(MDPMAPTEST_DOS_OBJ) $(MDP_MAP_OBJ) \
		$(MDP_CORE_OBJ)

$(MDP_CORE_OBJ): src/mdp/mdp.c include/moon/mdp.h | $(BUILD_DIRS)
	$(CC) $(MDP_CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(MDP_CORE_DEP) -MT $@ -c $< -o $@

$(MDP_MAP_OBJ): src/mdp/map_codec.c include/moon/mdp_map.h \
		include/moon/mdp.h | $(BUILD_DIRS)
	$(CC) $(MDP_CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(MDP_MAP_DEP) -MT $@ -c $< -o $@

$(MDPC_OBJ): tools/mdpc/mdpc.c include/moon/mdp.h \
		include/moon/mdp_map.h | $(BUILD_DIRS)
	$(CC) $(MDP_CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(MDPC_DEP) -MT $@ -c $< -o $@

$(MDPTEST_DOS_OBJ): tests/mdp/test_mdp.c include/moon/mdp.h | $(BUILD_DIRS)
	$(CC) $(MDP_CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(MDPTEST_DOS_DEP) -MT $@ -c $< -o $@

$(MDPMAPTEST_DOS_OBJ): tests/mdp/test_mdp_map.c include/moon/mdp_map.h \
		include/moon/mdp.h | $(BUILD_DIRS)
	$(CC) $(MDP_CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(MDPMAPTEST_DOS_DEP) -MT $@ -c $< -o $@

$(OBJ_DIR)/%.o: %.c | $(BUILD_DIRS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

ifeq ($(DOS_SHELL),1)
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

ifneq ($(DOS_SHELL),1)
.PHONY: mdpc-host mdpc-smoke mdp-test-host mdp-map-test-host \
	mdp-test-dosbox test-full

$(HOST_ROOT): | $(BUILD_ROOT)
	$(make_directory)

$(HOST_ROOT)/obj $(HOST_ROOT)/dep $(HOST_ROOT)/bin: | $(HOST_ROOT)
	$(make_directory)

$(HOST_OBJ_DIR): | $(HOST_ROOT)/obj
	$(make_directory)

$(HOST_DEP_DIR): | $(HOST_ROOT)/dep
	$(make_directory)

$(HOST_BIN_DIR): | $(HOST_ROOT)/bin
	$(make_directory)

$(HOST_MDP_CORE_OBJ): src/mdp/mdp.c include/moon/mdp.h | $(HOST_OBJ_DIR) $(HOST_DEP_DIR)
	$(HOST_CC) $(HOST_CPPFLAGS) -Iinclude $(HOST_CFLAGS) -MMD -MP -MF $(HOST_MDP_CORE_DEP) -MT $@ -c $< -o $@

$(HOST_MDP_MAP_OBJ): src/mdp/map_codec.c include/moon/mdp_map.h \
		include/moon/mdp.h | $(HOST_OBJ_DIR) $(HOST_DEP_DIR)
	$(HOST_CC) $(HOST_CPPFLAGS) -Iinclude $(HOST_CFLAGS) -MMD -MP -MF $(HOST_MDP_MAP_DEP) -MT $@ -c $< -o $@

$(HOST_MDPC_OBJ): tools/mdpc/mdpc.c include/moon/mdp.h \
		include/moon/mdp_map.h | $(HOST_OBJ_DIR) $(HOST_DEP_DIR)
	$(HOST_CC) $(HOST_CPPFLAGS) -Iinclude $(HOST_CFLAGS) -MMD -MP -MF $(HOST_MDPC_DEP) -MT $@ -c $< -o $@

$(HOST_MDP_TEST_OBJ): tests/mdp/test_mdp.c include/moon/mdp.h | $(HOST_OBJ_DIR) $(HOST_DEP_DIR)
	$(HOST_CC) $(HOST_CPPFLAGS) -Iinclude $(HOST_CFLAGS) -MMD -MP -MF $(HOST_MDP_TEST_DEP) -MT $@ -c $< -o $@

$(HOST_MDP_MAP_TEST_OBJ): tests/mdp/test_mdp_map.c \
		include/moon/mdp_map.h include/moon/mdp.h | $(HOST_OBJ_DIR) $(HOST_DEP_DIR)
	$(HOST_CC) $(HOST_CPPFLAGS) -Iinclude $(HOST_CFLAGS) -MMD -MP -MF $(HOST_MDP_MAP_TEST_DEP) -MT $@ -c $< -o $@

$(HOST_MDPC_BIN): $(HOST_MDPC_OBJ) $(HOST_MDP_CORE_OBJ) \
		$(HOST_MDP_MAP_OBJ) | $(HOST_BIN_DIR)
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $(HOST_MDPC_OBJ) $(HOST_MDP_CORE_OBJ) \
		$(HOST_MDP_MAP_OBJ)

$(HOST_MDP_TEST_BIN): $(HOST_MDP_TEST_OBJ) $(HOST_MDP_CORE_OBJ) | $(HOST_BIN_DIR)
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $(HOST_MDP_TEST_OBJ) $(HOST_MDP_CORE_OBJ)

$(HOST_MDP_MAP_TEST_BIN): $(HOST_MDP_MAP_TEST_OBJ) $(HOST_MDP_MAP_OBJ) \
		$(HOST_MDP_CORE_OBJ) | $(HOST_BIN_DIR)
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $(HOST_MDP_MAP_TEST_OBJ) \
		$(HOST_MDP_MAP_OBJ) $(HOST_MDP_CORE_OBJ)

mdpc-host: $(HOST_MDPC_BIN)

mdpc-smoke: mdpc-host
	./scripts/mdpc-smoke.sh "$(HOST_MDPC_BIN)"

mdp-test-host: $(HOST_MDP_TEST_BIN)
	"$(HOST_MDP_TEST_BIN)"

mdp-map-test-host: $(HOST_MDP_MAP_TEST_BIN)
	"$(HOST_MDP_MAP_TEST_BIN)"
endif

ifeq ($(DOS_SHELL),1)
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
GAME_DIST_FILES := \
	$(GAME_DIST_DIR)/ZEUS.EXE \
	$(GAME_DIST_DIR)/CWSDPMI.EXE \
	$(GAME_DIST_DIR)/GAME.MDP

TOOLS_DIST_FILES := \
	$(TOOLS_DIST_DIR)/MDPED.EXE \
	$(TOOLS_DIST_DIR)/MDPC.EXE \
	$(TOOLS_DIST_DIR)/TMUSE.EXE \
	$(TOOLS_DIST_DIR)/TMUSEGUI.EXE \
	$(TOOLS_DIST_DIR)/MOON.EXE \
	$(TOOLS_DIST_DIR)/CWSDPMI.EXE \
	$(TOOLS_DIST_DIR)/PALETTE.BMP

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
$(TOOLS_DIST_DIR)/MDPC.EXE: $(MDPC_BIN) | $(TOOLS_DIST_DIR)
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
ifeq ($(DOS_SHELL),1)
test: all mdp-test-dos mdp-map-test-dos
	@$(subst /,\,$(MDPTEST_DOS_BIN))
	@$(subst /,\,$(MDPMAPTEST_DOS_BIN))
	@echo Compile/link and MDP tests passed for CONFIG=$(CONFIG).
else
test: all mdp-test-host mdp-map-test-host mdpc-smoke mdp-test-dos \
		mdp-map-test-dos
	@echo Host MDP tests passed and DOS MDP tests compiled for CONFIG=$(CONFIG).
	@echo Run make test-full for the vanilla DOSBox compatibility gates.

mdp-test-dosbox: mdp-test-dos mdp-map-test-dos mdpc \
		$(DOS_DIR)/CWSDPMI.EXE
	./scripts/mdp-dosbox-test.sh $(CONFIG) "$(BUILD_ROOT)"

test-full: test dosbox-smoke mdp-test-dosbox
	@echo All host and vanilla DOSBox gates passed for CONFIG=$(CONFIG).
endif

dosbox-smoke: dist-game
	./scripts/dosbox-smoke.sh $(CONFIG) "$(DIST_ROOT)"

ifeq ($(DOS_SHELL),1)
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
	@echo "  all zeus mdped tmuse tmusegui moon mdpc runtime"
	@echo "  test mdp-test-dos mdp-map-test-dos dosbox-smoke"
	@echo "  dist dist-game dist-tools clean"
	@echo "  debug release"
ifneq ($(DOS_SHELL),1)
	@echo "  test-full mdpc-host mdpc-smoke mdp-test-host"
	@echo "  mdp-map-test-host mdp-test-dosbox"
endif
	@echo "Select a configuration with CONFIG=debug or CONFIG=release."

-include $(ALL_DEP)
ifneq ($(DOS_SHELL),1)
-include $(HOST_DEP)
endif
