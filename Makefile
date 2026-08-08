# ClouDS Music -- Nintendo 3DS homebrew client
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
ifeq ($(filter host-test clean azahar-install emulator-build cia-build cia-tools old3ds-stress print-version release-build release-sources repo-check run run-old3ds-stress debug,$(MAKECMDGOALS)),)
$(error "DEVKITARM is not set. Install devkitPro and run: source /etc/profile.d/devkit-env.sh")
endif
endif

TOPDIR ?= $(CURDIR)
PROJECT_ROOT := $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
# Multi-platform manifest resolved from devkitarm:latest on 2026-07-17.
DEVKITARM_IMAGE ?= devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5
ifneq ($(strip $(DEVKITARM)),)
include $(DEVKITARM)/3ds_rules
endif

TARGET      := ClouDS-Music
BUILD       := build
SOURCES     := source external/qrcodegen
INCLUDES    := include external/minimp3 external/qrcodegen
ROMFS       := romfs
ROMFS_FILES := $(wildcard $(PROJECT_ROOT)/$(ROMFS)/*)
ICON        := icon-v4.png

APP_TITLE       := ClouDS Music
APP_DESCRIPTION := streaming music client for Nintendo 3DS
APP_AUTHOR      := cadl
APP_VERSION     := 1.1.0
APP_RELEASE_DATE := 2026-07-25
APP_VERSION_PARTS := $(subst ., ,$(APP_VERSION))

CIA_TARGET       := $(TARGET)
CIA_RSF          := $(PROJECT_ROOT)/cia/build-cia.rsf
CIA_BANNER_IMAGE := $(PROJECT_ROOT)/banner-v2.png
CIA_BANNER_AUDIO := $(PROJECT_ROOT)/banner.wav
CIA_BANNER       := $(PROJECT_ROOT)/$(BUILD)/$(CIA_TARGET).bnr
CIA_OUTPUT       := $(PROJECT_ROOT)/$(CIA_TARGET).cia
CIA_VERSION_MAJOR := $(word 1,$(APP_VERSION_PARTS))
CIA_VERSION_MINOR := $(word 2,$(APP_VERSION_PARTS))
CIA_VERSION_MICRO := $(word 3,$(APP_VERSION_PARTS))
CIA_HOST_TAG      := $(shell uname -s)-$(shell uname -m)
CIA_TOOLS_BIN     := $(PROJECT_ROOT)/.tools/cia-tools/$(CIA_HOST_TAG)/bin
MAKEROM           ?= $(CIA_TOOLS_BIN)/makerom
BANNERTOOL        ?= $(CIA_TOOLS_BIN)/bannertool

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -Wextra -Wshadow -O2 -std=gnu11 \
               -mword-relocations -ffunction-sections $(ARCH)
CFLAGS      += $(INCLUDE) -D__3DS__ -DMINIMP3_ONLY_MP3 -DMINIMP3_NO_SIMD
CFLAGS      += -DNM3DS_APP_VERSION=\"$(APP_VERSION)\" \
               -DNM3DS_APP_RELEASE_DATE=\"$(APP_RELEASE_DATE)\"
CFLAGS      += $(EXTRA_CFLAGS)
CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS        := -lcitro2d -lcitro3d -lcurl \
               -lmbedtls -lmbedx509 -lmbedcrypto -lpng -ljpeg -lz -lctru -lm
LIBDIRS     := $(CTRULIB) $(PORTLIBS)

AZAHAR_APP ?= $(PROJECT_ROOT)/.tools/Azahar.app
GDB_PORT   ?= 24689
OLD3DS_STRESS_BUILD  := build-old3ds-stress
OLD3DS_STRESS_TARGET := $(TARGET)-old3ds-stress

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
export LD   := $(CC)
else
export LD   := $(CXX)
endif

export OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_ICON := $(PROJECT_ROOT)/$(ICON)
export _3DSXDEPS := $(if $(NO_SMDH),,$(OUTPUT).smdh)
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
export ROMFS_FILES

.PHONY: all clean host-test azahar-install emulator-build cia cia-build \
	cia-tools old3ds-stress release release-build release-package \
	release-sources print-version repo-check run run-old3ds-stress debug $(BUILD)

all: $(BUILD)

print-version:
	@printf '%s\n' "$(APP_VERSION)"

repo-check:
	@bash tools/check-repository-hygiene.sh

$(BUILD):
	@test -f external/minimp3/minimp3_ex.h || { \
		echo "error: minimp3 submodule missing" >&2; \
		echo "run: git submodule update --init --recursive" >&2; exit 1; }
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

host-test:
	@python3 tests/test_ui_font_whitelist.py
	@python3 tests/test_content_font_whitelist.py
	@python3 tests/test_bcfnt_normalize.py
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/i18n.c tests/test_i18n.c -o tests/test_i18n
	@tests/test_i18n
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		tests/test_dsp_firmware_help.c -o tests/test_dsp_firmware_help
	@tests/test_dsp_firmware_help
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/json.c tests/test_json.c -o tests/test_json
	@tests/test_json
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/playlist_index.c source/i18n.c tests/test_playlist_index.c \
		-o tests/test_playlist_index
	@tests/test_playlist_index
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/song_index.c source/song_text.c source/unicode_text.c \
		source/i18n.c tests/test_song_index.c -o tests/test_song_index
	@tests/test_song_index
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/ime_pinyin.c tests/test_ime.c -o tests/test_ime
	@tests/test_ime
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/ime_candidate_layout.c tests/test_ime_candidate_layout.c \
		-o tests/test_ime_candidate_layout
	@tests/test_ime_candidate_layout
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/playlist.c source/cache.c source/lyric_cache.c \
		source/song_text.c source/unicode_text.c \
		source/settings.c source/i18n.c tests/test_storage.c \
		-lz -o tests/test_storage
	@tests/test_storage
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/netease_session.c source/auth.c source/i18n.c tests/test_auth.c \
		-o tests/test_auth
	@tests/test_auth
	@cc -std=c11 -Wall -Wextra -Werror -Wno-type-limits \
		-Iexternal/qrcodegen external/qrcodegen/qrcodegen.c \
		tests/test_qrcode.c -o tests/test_qrcode
	@tests/test_qrcode
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/navigation.c tests/test_navigation.c -o tests/test_navigation
	@tests/test_navigation
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/playback_navigation.c tests/test_playback_navigation.c \
		-o tests/test_playback_navigation
	@tests/test_playback_navigation
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/search_page.c tests/test_search_page.c -o tests/test_search_page
	@tests/test_search_page
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		tests/test_ui_layout.c -o tests/test_ui_layout
	@tests/test_ui_layout
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/control_hint_layout.c tests/test_control_hint_layout.c \
		-lm -o tests/test_control_hint_layout
	@tests/test_control_hint_layout
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		tests/test_gpu_texture.c -o tests/test_gpu_texture
	@tests/test_gpu_texture
	@if command -v pkg-config >/dev/null && \
		pkg-config --exists libpng libjpeg; then \
		set -e; \
		cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		$$(pkg-config --cflags libpng libjpeg) \
		source/cover_decode.c source/i18n.c tests/test_cover_decode.c \
		$$(pkg-config --libs libpng libjpeg) -o tests/test_cover_decode; \
		tests/test_cover_decode; \
	else \
		echo "cover decoder tests: skipped (host libpng/libjpeg unavailable)"; \
	fi
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/lyric_animation.c tests/test_lyric_animation.c -lm \
		-o tests/test_lyric_animation
	@tests/test_lyric_animation
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/immersive_font_data.c tests/test_immersive_font_data.c \
		-o tests/test_immersive_font_data
	@tests/test_immersive_font_data
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/song_text.c source/unicode_text.c tests/test_unicode_text.c \
		-o tests/test_unicode_text
	@tests/test_unicode_text
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/media_policy.c tests/test_media_policy.c -o tests/test_media_policy
	@tests/test_media_policy
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/now_playing_policy.c source/immersive_lyrics.c \
		tests/test_now_playing_policy.c \
		-o tests/test_now_playing_policy
	@tests/test_now_playing_policy
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/download_policy.c tests/test_download_policy.c \
		-o tests/test_download_policy
	@tests/test_download_policy
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/power_policy.c tests/test_power_policy.c -o tests/test_power_policy
	@tests/test_power_policy
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/network_retry.c tests/test_network_retry.c \
		-o tests/test_network_retry
	@tests/test_network_retry
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/diagnostic_text.c tests/test_diagnostic_text.c \
		-o tests/test_diagnostic_text
	@tests/test_diagnostic_text
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/playback_order.c tests/test_playback_order.c \
		-o tests/test_playback_order
	@tests/test_playback_order
	@cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		source/prefetch_policy.c tests/test_prefetch_policy.c \
		-o tests/test_prefetch_policy
	@tests/test_prefetch_policy
	@if command -v pkg-config >/dev/null && pkg-config --exists mbedcrypto; then \
		set -e; \
		cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		$$(pkg-config --cflags mbedcrypto) source/weapi.c source/i18n.c \
		tests/test_weapi.c \
		$$(pkg-config --libs mbedcrypto) -o tests/test_weapi; \
		tests/test_weapi; \
		cc -std=c11 -Wall -Wextra -Werror -Iinclude \
		$$(pkg-config --cflags mbedcrypto) source/eapi.c source/i18n.c \
		tests/test_eapi_stream.c \
		$$(pkg-config --libs mbedcrypto) -lz -o tests/test_eapi_stream; \
		tests/test_eapi_stream; \
	else \
		echo "weapi/eapi tests: skipped (host mbedTLS unavailable)"; \
	fi

azahar-install:
	@bash tools/install-azahar.sh

emulator-build:
ifneq ($(strip $(DEVKITARM)),)
	@$(MAKE) --no-print-directory all
else
	@command -v docker >/dev/null || { \
		echo "error: install devkitARM or Docker to build before launching Azahar" >&2; \
		exit 1; }
	@docker run --rm -v "$(PROJECT_ROOT):/project" -w /project \
		"$(DEVKITARM_IMAGE)" make -j2
endif

cia-tools:
	@bash tools/install-cia-tools.sh

cia: cia-tools $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile cia

cia-build:
ifneq ($(strip $(DEVKITARM)),)
	@$(MAKE) --no-print-directory cia
else
	@command -v docker >/dev/null || { \
		echo "error: install devkitARM or Docker to build the CIA" >&2; \
		exit 1; \
	}
	@docker run --rm -v "$(PROJECT_ROOT):/project" -w /project \
		"$(DEVKITARM_IMAGE)" make -j2 cia
endif

release-sources:
	@bash tools/fetch-ime-assets.sh

release-package:
	@$(MAKE) --no-print-directory release-sources
	@$(MAKE) --no-print-directory -j2 cia
	@python3 tools/package_release.py \
		--version "$(APP_VERSION)" --project-root "$(PROJECT_ROOT)"

release:
	@$(MAKE) --no-print-directory repo-check
	@$(MAKE) --no-print-directory host-test
	@$(MAKE) --no-print-directory release-package

release-build:
ifneq ($(strip $(DEVKITARM)),)
	@$(MAKE) --no-print-directory release
else
	@command -v docker >/dev/null || { \
		echo "error: install devkitARM or Docker to build release packages" >&2; \
		exit 1; }
	@docker run --rm -v "$(PROJECT_ROOT):/project" -w /project \
		"$(DEVKITARM_IMAGE)" make -j2 release
endif

old3ds-stress:
ifneq ($(strip $(DEVKITARM)),)
	@$(MAKE) --no-print-directory \
		BUILD="$(OLD3DS_STRESS_BUILD)" \
		TARGET="$(OLD3DS_STRESS_TARGET)" \
		EXTRA_CFLAGS=-DNM3DS_OLD3DS_STRESS all
else
	@command -v docker >/dev/null || { \
		echo "error: install devkitARM or Docker to build the Old 3DS stress target" >&2; \
		exit 1; }
	@docker run --rm -v "$(PROJECT_ROOT):/project" -w /project \
		"$(DEVKITARM_IMAGE)" make -j2 \
		BUILD="$(OLD3DS_STRESS_BUILD)" \
		TARGET="$(OLD3DS_STRESS_TARGET)" \
		EXTRA_CFLAGS=-DNM3DS_OLD3DS_STRESS all
endif

run: emulator-build
	@AZAHAR_APP="$(AZAHAR_APP)" bash tools/run-azahar.sh \
		-w "$(OUTPUT).3dsx"

run-old3ds-stress: old3ds-stress
	@AZAHAR_APP="$(AZAHAR_APP)" bash tools/run-azahar.sh \
		-w "$(PROJECT_ROOT)/$(OLD3DS_STRESS_TARGET).3dsx"

debug: emulator-build
	@AZAHAR_APP="$(AZAHAR_APP)" bash tools/run-azahar.sh \
		-w -g "$(GDB_PORT)" "$(OUTPUT).3dsx"

clean:
	@rm -rf $(BUILD) $(OLD3DS_STRESS_BUILD) dist \
		$(TARGET).3dsx $(TARGET).elf $(TARGET).smdh \
		$(OLD3DS_STRESS_TARGET).3dsx $(OLD3DS_STRESS_TARGET).elf \
		$(OLD3DS_STRESS_TARGET).smdh \
		$(CIA_OUTPUT) $(CIA_BANNER) \
		tests/test_json tests/test_ime tests/test_storage tests/test_qrcode
	@rm -f tests/test_auth tests/test_navigation tests/test_lyric_animation \
		tests/test_ime_candidate_layout \
		tests/test_immersive_font_data \
		tests/test_unicode_text \
		tests/test_search_page tests/test_ui_layout tests/test_gpu_texture \
		tests/test_control_hint_layout \
		tests/test_download_policy tests/test_media_policy \
		tests/test_now_playing_policy tests/test_power_policy \
		tests/test_network_retry tests/test_diagnostic_text \
		tests/test_playback_navigation tests/test_playback_order \
		tests/test_prefetch_policy tests/test_weapi tests/test_eapi_stream \
		tests/test_playlist_index tests/test_song_index tests/test_i18n \
		tests/test_cover_decode \
		tests/test_dsp_firmware_help

else

DEPENDS := $(OFILES:.o=.d)

# Upstream qrcodegen contains defensive enum assertions that GCC correctly
# identifies as tautological for this target's enum representation.
qrcodegen.o: CFLAGS += -Wno-type-limits

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS) $(ROMFS_FILES)
$(OUTPUT).elf: $(OFILES)

cia: $(CIA_OUTPUT)

$(CIA_BANNER): $(CIA_BANNER_IMAGE) $(CIA_BANNER_AUDIO) $(BANNERTOOL)
	@mkdir -p $(dir $@)
	@$(BANNERTOOL) makebanner \
		-i "$(CIA_BANNER_IMAGE)" -a "$(CIA_BANNER_AUDIO)" -o "$@"

$(CIA_OUTPUT): $(OUTPUT).elf $(OUTPUT).smdh $(CIA_BANNER) $(CIA_RSF) \
		$(ROMFS_FILES) $(MAKEROM)
	@$(MAKEROM) -f cia -target t -exefslogo \
		-o "$@" -elf "$(OUTPUT).elf" -rsf "$(CIA_RSF)" \
		-icon "$(OUTPUT).smdh" -banner "$(CIA_BANNER)" \
		-DAPP_ROMFS="$(PROJECT_ROOT)/$(ROMFS)" \
		-major $(CIA_VERSION_MAJOR) -minor $(CIA_VERSION_MINOR) \
		-micro $(CIA_VERSION_MICRO)

-include $(DEPENDS)

endif
