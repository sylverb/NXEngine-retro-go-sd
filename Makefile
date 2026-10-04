# Retro-Go SD — Cave Story (NXEngine) GWHB homebrew
#
#   make                 → CaveStory.bin (device, links NXEngine)
#   make host            → CaveStory_host (desktop SDL 1.2)
#   make docker

#######################################
# Project identity
#######################################
PROJECT_KIND ?= homebrew

CORE_NAME  := cavestory
CORE_ENTRY := app_main

include scripts/nxengine_sources.mk

CORE_C_SOURCES := \
src/gw_app.c \
src/platform/gw_flash_assets.c \
src/platform/gw_mem.c \
src/platform/gw_pack.c \
src/platform/sdl_gw.c \
src/platform/gw_libc_stubs.c

CORE_CXX_SOURCES := \
src/platform/gw_fputl_bridge.cpp \
$(NXENGINE_CXX_SOURCES)

CORE_C_INCLUDES := \
-Isrc \
-Isrc/platform \
-Isrc/platform/SDL \
-Ithird_party/nxengine \
-Ithird_party/nxengine/common

CORE_C_DEFS += \
-D_320X240=1 \
-DNXENGINE_GW=1 \
-DGW_PACK_STDIO_WRAP=1 \
-DGW_NX_FS_ROOT=\"/homebrews\"

CORE_LDLIBS := -lm \
	-Wl,--wrap=core_fread \
	-Wl,--wrap=core_fwrite \
	-Wl,--wrap=core_fclose \
	-Wl,--wrap=core_fseek \
	-Wl,--wrap=core_ftell \
	-Wl,--wrap=core_feof \
	-Wl,--wrap=core_ferror \
	-Wl,--wrap=core_fgetc

# Hot .text → ITCM; sprite dirs / font pixels → DTCM via dtc_*.
CORE_LDSCRIPT := cavestory.ld
CORE_EXTRA_SEGMENTS := itcm:core_itcm

GNW_CORE_SDK ?= sdk
BUILD_DIR ?= build/$(PROJECT_KIND)

#######################################
# Kind-specific compile defs + packing
#######################################
ifeq ($(PROJECT_KIND),core)
$(error This project is a GWHB homebrew — use PROJECT_KIND=homebrew)
else ifeq ($(PROJECT_KIND),homebrew)
CORE_C_DEFS += \
-DPROJECT_KIND_HOMEBREW=1

PACKED_BIN := CaveStory.bin
HB_NAME    := Cave Story
COVER_SRC  := src/assets/cover.png

else
$(error PROJECT_KIND must be 'homebrew' (got '$(PROJECT_KIND)'))
endif

include $(GNW_CORE_SDK)/Makefile

PACK_HOMEBREW := $(GNW_CORE_SDK)/tools/pack_homebrew.py

CORE_VERSION ?= $(shell git describe --tags --dirty 2>/dev/null || echo NOTAG)

.PHONY: pack

pack: $(TARGET_BIN) $(COVER_SRC)
	$(V)$(ECHO) [ PACK GWHB ] $(PACKED_BIN) version=$(CORE_VERSION)
	$(V)python3 $(PACK_HOMEBREW) \
		--elf $(TARGET_ELF) --bin $(TARGET_BIN) \
		--name "$(HB_NAME)" --version "$(CORE_VERSION)" \
		--cover $(COVER_SRC) \
		--out $(PACKED_BIN)
	$(V)mkdir -p sd_content/homebrews
	$(V)cp -f $(PACKED_BIN) sd_content/homebrews/$(PACKED_BIN)

all: pack

.PHONY: print-PROJECT_KIND print-PACKED_BIN print-CORE_NAME print-DOCKER_IMAGE \
	print-TARGET_ELF print-TARGET_MAP print-CORE_VERSION
print-PROJECT_KIND:
	@echo $(PROJECT_KIND)
print-PACKED_BIN:
	@echo $(PACKED_BIN)
print-CORE_NAME:
	@echo $(CORE_NAME)
print-DOCKER_IMAGE:
	@echo $(DOCKER_IMAGE)
print-TARGET_ELF:
	@echo $(TARGET_ELF)
print-TARGET_MAP:
	@echo $(BUILD_DIR)/$(CORE_NAME)_core.map
print-CORE_VERSION:
	@echo $(CORE_VERSION)

clean::
	$(V)rm -f $(PACKED_BIN)

#######################################
# Docker
#######################################
.PHONY: docker docker_pull docker_shell

RELEASE_VERSION ?= v1.5
DOCKER_REPOSITORY ?= sylverb/retro-go-sd-builder
DOCKER_IMAGE ?= $(DOCKER_REPOSITORY):$(RELEASE_VERSION)

DOCKER_TTY_FLAG := $(shell if [ -t 0 ]; then echo -it; else echo; fi)
DOCKER_USER := $(shell id -u):$(shell id -g)
DOCKER_RUN := docker run --rm $(DOCKER_TTY_FLAG) \
	--user $(DOCKER_USER) \
	-v "$(CURDIR):/opt/workdir" \
	-w /opt/workdir \
	$(DOCKER_IMAGE)

docker:
	$(V)$(ECHO) "[ DOCKER ]" $(DOCKER_IMAGE) "PROJECT_KIND=$(PROJECT_KIND)"
	$(V)$(DOCKER_RUN) make --no-print-directory -j$$(nproc) PROJECT_KIND=$(PROJECT_KIND)

docker_pull:
	$(V)$(ECHO) "[ PULL ]" $(DOCKER_IMAGE)
	$(V)docker pull $(DOCKER_IMAGE)

docker_shell:
	$(DOCKER_RUN) bash

#######################################
# Host (desktop NXEngine)
#######################################
include host/Makefile.nxengine_host

# Locale for asset packs: en (default), fr, … — see scripts/cavestory_locales.py
LOCALE ?= en

.PHONY: host_scaffold prepare-assets pack-assets prepare-cavestory-tree ci-assets \
	ci-assets-all pack-assets-fr list-locales
host_scaffold:
	$(MAKE) -f host/Makefile.host_scaffold host

# Offline 4bpp → 8bpp siblings (optional debug; pack-assets embeds 8bpp).
prepare-assets:
	$(V)$(ECHO) "[ ASSETS ]" prepare CaveStory *.u8.bmp
	$(V)python3 scripts/prepare_cavestory_assets.py CaveStory

list-locales:
	$(V)python3 scripts/cavestory_locales.py

# Download freeware (+ optional translation overlay) + extract Doukutsu.exe.
# Does not build drum.pcm / sndcache.pcm — use ./CaveStory_host --ci-prepare.
# Examples:
#   make prepare-cavestory-tree
#   make prepare-cavestory-tree LOCALE=fr
prepare-cavestory-tree:
	$(V)$(ECHO) "[ CS ]" prepare CaveStory/ locale=$(LOCALE)
	$(V)python3 scripts/prepare_cavestory_tree.py --locale $(LOCALE)

# Full asset pipeline: tree → host audio caches → cavestory[_xx].nxpk.
# Prepare CaveStory/ *before* `make host` — host_nx_prepare needs data/npc.tbl.
#   make ci-assets
#   make ci-assets LOCALE=fr
ci-assets:
	$(V)$(ECHO) "[ CS ]" CI asset pack locale=$(LOCALE)
	$(V)python3 scripts/prepare_cavestory_tree.py --locale $(LOCALE)
	$(V)$(MAKE) --no-print-directory host
	$(V)./$(HOST_NX_BIN) --ci-prepare
	$(V)$(MAKE) --no-print-directory pack-assets LOCALE=$(LOCALE)

# Pack every locale in scripts/cavestory_locales.py (EN first, then overlays).
# Audio caches are built once and kept across locale re-prepares.
ci-assets-all:
	$(V)$(ECHO) "[ CS ]" CI asset pack (all locales)
	$(V)first=1; \
	for loc in $$(python3 scripts/cavestory_locales.py --ids); do \
		$(ECHO) "[ CS ]" locale=$$loc; \
		python3 scripts/prepare_cavestory_tree.py --locale $$loc; \
		if [ $$first -eq 1 ]; then \
			$(MAKE) --no-print-directory host; \
			./$(HOST_NX_BIN) --ci-prepare; \
			first=0; \
		fi; \
		$(MAKE) --no-print-directory pack-assets LOCALE=$$loc; \
	done

# Build NXPK (8bpp images + cleartext TSC) and copy to sd_content/homebrews/.
# Runtime still loads /homebrews/cavestory.nxpk — rename/copy the locale file
# on the SD card (e.g. cavestory_fr.nxpk → cavestory.nxpk). Release zips do
# that rename for you (CaveStory-<tag>-<locale>.zip).
pack-assets:
	$(V)$(ECHO) "[ NXPK ]" locale=$(LOCALE)
	$(V)python3 scripts/pack_cavestory_nxpk.py CaveStory --locale $(LOCALE) --also-sd
	$(V)mkdir -p sd_content/homebrews
	$(V)if [ -f $(PACKED_BIN) ]; then \
		cp -f $(PACKED_BIN) sd_content/homebrews/$(PACKED_BIN); \
		$(ECHO) "[ SD ]" sd_content/homebrews/$(PACKED_BIN); \
	fi

pack-assets-fr:
	$(V)$(MAKE) --no-print-directory ci-assets LOCALE=fr

.PHONY: print-NXPK print-LOCALE_IDS
print-NXPK:
	@python3 -c "import sys; sys.path.insert(0,'scripts'); from cavestory_locales import get_locale; \
print('sd_content/homebrews/' + get_locale('$(LOCALE)').nxpk)"
print-LOCALE_IDS:
	@python3 scripts/cavestory_locales.py --ids
