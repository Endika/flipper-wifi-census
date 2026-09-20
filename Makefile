PROJECT_NAME = wifi_census
FAP_APPID = flipper_wifi_census

# Local override: create a gitignored `local.mk` with your real path, e.g.
#   FLIPPER_FIRMWARE_PATH = /home/you/flipperzero-firmware
# The committed default below is a placeholder on purpose — never commit a real path.
-include local.mk
FLIPPER_FIRMWARE_PATH ?= <Path>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -I.

.PHONY: all help test prepare fap clean clean_firmware format format-check linter tool

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Host unit tests (domain logic)"
	@echo "  make prepare        - Symlink app into firmware applications_user"
	@echo "  make fap            - Clean firmware build + compile .fap"
	@echo "  make format         - clang-format"
	@echo "  make linter         - cppcheck"
	@echo "  make clean          - Remove local objects"
	@echo "  make clean_firmware - rm firmware build dir"

# --- host tests: compile domain + test TU with gcc, run the binary ---
# Each tests/test_*.c is its own suite with its own `main`, linked into test_wc_<suite>;
# `make test` runs all of them. Add a suite by appending its .o list and its binary below.
OBJS_SMOKE = wc_version_info.o test_smoke.o
OBJS_SIGNATURE = wc_observation.o wc_signature.o test_signature.o
OBJS_CENSUS = wc_observation.o wc_signature.o wc_census.o test_census.o
OBJS_CODEC = wc_observation.o wc_signature.o wc_census.o wc_capture_codec.o test_codec.o
OBJS_COMPARE = wc_observation.o wc_signature.o wc_census.o wc_compare.o test_compare.o
OBJS_KNOWN = wc_observation.o wc_signature.o wc_known.o test_known.o
OBJS_PARSE = wc_observation.o wc_marauder_parse.o test_marauder_parse.o
OBJS_LINEASM = wc_line_assembler.o test_line_assembler.o
OBJS_TIMEFMT = wc_timefmt.o test_timefmt.o
OBJS_PROBE = wc_observation.o wc_probe_frame.o test_probe_frame.o
OBJS_PCAP = wc_pcap_reader.o test_pcap_reader.o
OBJS_APP = wc_observation.o wc_signature.o wc_census.o wc_capture_codec.o wc_compare.o wc_known.o wc_marauder_parse.o wc_scan_service.o wc_capture_service.o wc_compare_service.o wc_known_service.o wc_settings_service.o wc_merge_service.o wc_import_service.o wc_pcap_reader.o wc_probe_frame.o test_app_services.o
TEST_BINS = test_wc_smoke test_wc_signature test_wc_census test_wc_codec test_wc_compare test_wc_known test_wc_parse test_wc_lineasm test_wc_app test_wc_timefmt test_wc_probe test_wc_pcap

test: $(TEST_BINS)
	./test_wc_smoke
	./test_wc_signature
	./test_wc_census
	./test_wc_codec
	./test_wc_compare
	./test_wc_known
	./test_wc_parse
	./test_wc_lineasm
	./test_wc_app
	./test_wc_timefmt
	./test_wc_probe
	./test_wc_pcap

test_wc_smoke: $(OBJS_SMOKE)
	$(CC) $(CFLAGS) -o test_wc_smoke $(OBJS_SMOKE)

test_wc_signature: $(OBJS_SIGNATURE)
	$(CC) $(CFLAGS) -o test_wc_signature $(OBJS_SIGNATURE)

test_wc_census: $(OBJS_CENSUS)
	$(CC) $(CFLAGS) -o test_wc_census $(OBJS_CENSUS)

test_wc_codec: $(OBJS_CODEC)
	$(CC) $(CFLAGS) -o test_wc_codec $(OBJS_CODEC)

test_wc_compare: $(OBJS_COMPARE)
	$(CC) $(CFLAGS) -o test_wc_compare $(OBJS_COMPARE)

test_wc_known: $(OBJS_KNOWN)
	$(CC) $(CFLAGS) -o test_wc_known $(OBJS_KNOWN)

test_wc_parse: $(OBJS_PARSE)
	$(CC) $(CFLAGS) -o test_wc_parse $(OBJS_PARSE)

test_wc_lineasm: $(OBJS_LINEASM)
	$(CC) $(CFLAGS) -o test_wc_lineasm $(OBJS_LINEASM)

test_wc_app: $(OBJS_APP)
	$(CC) $(CFLAGS) -o test_wc_app $(OBJS_APP)

test_wc_timefmt: $(OBJS_TIMEFMT)
	$(CC) $(CFLAGS) -o test_wc_timefmt $(OBJS_TIMEFMT)

test_wc_probe: $(OBJS_PROBE)
	$(CC) $(CFLAGS) -o test_wc_probe $(OBJS_PROBE)

test_wc_pcap: $(OBJS_PCAP)
	$(CC) $(CFLAGS) -o test_wc_pcap $(OBJS_PCAP)

wc_version_info.o: src/domain/wc_version_info.c include/domain/wc_version_info.h include/version.h
	$(CC) $(CFLAGS) -c src/domain/wc_version_info.c -o wc_version_info.o

test_smoke.o: tests/test_smoke.c include/domain/wc_version_info.h
	$(CC) $(CFLAGS) -c tests/test_smoke.c -o test_smoke.o

wc_observation.o: src/domain/wc_observation.c include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c src/domain/wc_observation.c -o wc_observation.o

wc_signature.o: src/domain/wc_signature.c include/domain/wc_signature.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c src/domain/wc_signature.c -o wc_signature.o

test_signature.o: tests/test_signature.c include/domain/wc_signature.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c tests/test_signature.c -o test_signature.o

wc_census.o: src/domain/wc_census.c include/domain/wc_census.h include/domain/wc_signature.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c src/domain/wc_census.c -o wc_census.o

test_census.o: tests/test_census.c include/domain/wc_census.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c tests/test_census.c -o test_census.o

wc_capture_codec.o: src/domain/wc_capture_codec.c include/domain/wc_capture_codec.h include/domain/wc_census.h
	$(CC) $(CFLAGS) -c src/domain/wc_capture_codec.c -o wc_capture_codec.o

test_codec.o: tests/test_codec.c include/domain/wc_capture_codec.h include/domain/wc_census.h
	$(CC) $(CFLAGS) -c tests/test_codec.c -o test_codec.o

wc_compare.o: src/domain/wc_compare.c include/domain/wc_compare.h include/domain/wc_census.h
	$(CC) $(CFLAGS) -c src/domain/wc_compare.c -o wc_compare.o

test_compare.o: tests/test_compare.c include/domain/wc_compare.h include/domain/wc_census.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c tests/test_compare.c -o test_compare.o

wc_known.o: src/domain/wc_known.c include/domain/wc_known.h include/domain/wc_signature.h
	$(CC) $(CFLAGS) -c src/domain/wc_known.c -o wc_known.o

test_known.o: tests/test_known.c include/domain/wc_known.h include/domain/wc_signature.h
	$(CC) $(CFLAGS) -c tests/test_known.c -o test_known.o

wc_marauder_parse.o: src/domain/wc_marauder_parse.c include/domain/wc_marauder_parse.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c src/domain/wc_marauder_parse.c -o wc_marauder_parse.o

test_marauder_parse.o: tests/test_marauder_parse.c include/domain/wc_marauder_parse.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c tests/test_marauder_parse.c -o test_marauder_parse.o

wc_line_assembler.o: src/domain/wc_line_assembler.c include/domain/wc_line_assembler.h include/domain/wc_marauder_parse.h
	$(CC) $(CFLAGS) -c src/domain/wc_line_assembler.c -o wc_line_assembler.o

test_line_assembler.o: tests/test_line_assembler.c include/domain/wc_line_assembler.h
	$(CC) $(CFLAGS) -c tests/test_line_assembler.c -o test_line_assembler.o

wc_scan_service.o: src/application/wc_scan_service.c include/application/wc_scan_service.h include/domain/wc_census.h include/ports/wc_clock_port.h include/domain/wc_marauder_parse.h
	$(CC) $(CFLAGS) -c src/application/wc_scan_service.c -o wc_scan_service.o

wc_capture_service.o: src/application/wc_capture_service.c include/application/wc_capture_service.h include/application/wc_files.h include/domain/wc_capture_codec.h include/ports/wc_store_port.h
	$(CC) $(CFLAGS) -c src/application/wc_capture_service.c -o wc_capture_service.o

wc_compare_service.o: src/application/wc_compare_service.c include/application/wc_compare_service.h include/application/wc_capture_service.h include/domain/wc_compare.h include/ports/wc_store_port.h
	$(CC) $(CFLAGS) -c src/application/wc_compare_service.c -o wc_compare_service.o

wc_known_service.o: src/application/wc_known_service.c include/application/wc_known_service.h include/application/wc_files.h include/domain/wc_known.h include/ports/wc_store_port.h
	$(CC) $(CFLAGS) -c src/application/wc_known_service.c -o wc_known_service.o

wc_settings_service.o: src/application/wc_settings_service.c include/application/wc_settings_service.h include/application/wc_files.h include/ports/wc_store_port.h
	$(CC) $(CFLAGS) -c src/application/wc_settings_service.c -o wc_settings_service.o

wc_merge_service.o: src/application/wc_merge_service.c include/application/wc_merge_service.h include/application/wc_capture_service.h include/domain/wc_census.h include/ports/wc_store_port.h
	$(CC) $(CFLAGS) -c src/application/wc_merge_service.c -o wc_merge_service.o

wc_timefmt.o: src/domain/wc_timefmt.c include/domain/wc_timefmt.h
	$(CC) $(CFLAGS) -c src/domain/wc_timefmt.c -o wc_timefmt.o

test_timefmt.o: tests/test_timefmt.c include/domain/wc_timefmt.h
	$(CC) $(CFLAGS) -c tests/test_timefmt.c -o test_timefmt.o

wc_probe_frame.o: src/domain/wc_probe_frame.c include/domain/wc_probe_frame.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c src/domain/wc_probe_frame.c -o wc_probe_frame.o

test_probe_frame.o: tests/test_probe_frame.c include/domain/wc_probe_frame.h include/domain/wc_observation.h
	$(CC) $(CFLAGS) -c tests/test_probe_frame.c -o test_probe_frame.o

wc_pcap_reader.o: src/domain/wc_pcap_reader.c include/domain/wc_pcap_reader.h
	$(CC) $(CFLAGS) -c src/domain/wc_pcap_reader.c -o wc_pcap_reader.o

test_pcap_reader.o: tests/test_pcap_reader.c include/domain/wc_pcap_reader.h
	$(CC) $(CFLAGS) -c tests/test_pcap_reader.c -o test_pcap_reader.o

wc_import_service.o: src/application/wc_import_service.c include/application/wc_import_service.h include/domain/wc_pcap_reader.h include/domain/wc_probe_frame.h
	$(CC) $(CFLAGS) -c src/application/wc_import_service.c -o wc_import_service.o

test_app_services.o: tests/test_app_services.c include/application/wc_scan_service.h include/application/wc_capture_service.h include/application/wc_compare_service.h include/application/wc_known_service.h include/application/wc_settings_service.h include/application/wc_files.h
	$(CC) $(CFLAGS) -c tests/test_app_services.c -o test_app_services.o

# --- format / lint ---
FORMAT_FILES := $(shell git ls-files '*.c' '*.h' 2>/dev/null)
ifeq ($(strip $(FORMAT_FILES)),)
FORMAT_FILES := $(shell find . -type f \( -name '*.c' -o -name '*.h' \) ! -path './.git/*' | sort)
endif

format:
	@find . -type f \( -name '*.c' -o -name '*.h' \) ! -path './.git/*' | sort | xargs clang-format -i

# Check formatting of ALL source files (find-based, not git-tracked-only) so a
# not-yet-committed file cannot slip past the format gate locally.
format-check:
	@find . -type f \( -name '*.c' -o -name '*.h' \) ! -path './.git/*' | sort | xargs clang-format --dry-run --Werror

# unusedFunction suppression: main.c's entry point (wc_app) is only called by the firmware
# loader, invisible to cppcheck from this host source set.
# tools/cppcheck_shims.h expands with_view_model() for the analyser: without it cppcheck hits
# an AST error and skips the whole file, reporting nothing while looking green.
linter:
	cppcheck --enable=all --inline-suppr --error-exitcode=1 -I. \
		--include=tools/cppcheck_shims.h \
		--suppress=missingIncludeSystem \
		--suppress=unmatchedSuppression \
		--suppress=unusedFunction:main.c \
		--suppress=checkersReport \
		--suppress=normalCheckLevelMaxBranches \
		--suppress=nullPointerOutOfMemory \
		src/domain/wc_version_info.c \
		src/domain/wc_observation.c \
		src/domain/wc_signature.c \
		src/domain/wc_census.c \
		src/domain/wc_capture_codec.c \
		src/domain/wc_compare.c \
		src/domain/wc_known.c \
		src/domain/wc_marauder_parse.c \
		src/domain/wc_line_assembler.c \
		src/domain/wc_timefmt.c \
		src/domain/wc_probe_frame.c \
		src/domain/wc_pcap_reader.c \
		src/application/wc_import_service.c \
		src/application/wc_scan_service.c \
		src/application/wc_capture_service.c \
		src/application/wc_compare_service.c \
		src/application/wc_known_service.c \
		src/application/wc_settings_service.c \
		src/application/wc_merge_service.c \
		src/platform/wc_clock_furi.c \
		src/platform/wc_store_furi.c \
		src/platform/wc_serial_furi.c \
		src/views/wc_scroll_list.c \
		src/app/wc_app.c \
		src/scenes/wc_scene.c \
		src/scenes/wc_scenes_common.c \
		src/scenes/wc_scene_home.c \
		src/scenes/wc_scene_scan.c \
		src/scenes/wc_scene_browse.c \
		src/scenes/wc_scene_known.c \
		src/scenes/wc_scene_crossing.c \
		src/scenes/wc_scene_import.c \
		main.c \
		tests/test_smoke.c \
		tests/test_signature.c \
		tests/test_census.c \
		tests/test_codec.c \
		tests/test_compare.c \
		tests/test_known.c \
		tests/test_marauder_parse.c \
		tests/test_line_assembler.c \
		tests/test_app_services.c \
		tests/test_timefmt.c \
		tests/test_probe_frame.c \
		tools/wc_import.c \
		tools/wc_merge.c \
		tools/wc_iefp.c \
		tools/wc_compare.c \
		tools/wc_clean.c

# --- build the .fap via the firmware tree (ufbt/fbt; not available in this sandbox) ---
prepare:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		mkdir -p $(FLIPPER_FIRMWARE_PATH)/applications_user; \
		ln -sfn $(PWD) $(FLIPPER_FIRMWARE_PATH)/applications_user/$(PROJECT_NAME); \
		echo "Linked to $(FLIPPER_FIRMWARE_PATH)/applications_user/$(PROJECT_NAME)"; \
	else \
		echo "Firmware not found at $(FLIPPER_FIRMWARE_PATH)"; \
	fi

clean_firmware:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)/build" ]; then \
		rm -rf $(FLIPPER_FIRMWARE_PATH)/build; \
	fi

fap: prepare clean_firmware clean
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		cd $(FLIPPER_FIRMWARE_PATH) && ./fbt fap_$(FAP_APPID); \
	fi

TOOL_DOMAIN = src/domain/wc_compare.c src/domain/wc_pcap_reader.c src/domain/wc_probe_frame.c src/domain/wc_observation.c src/domain/wc_signature.c src/domain/wc_census.c src/domain/wc_capture_codec.c

tool:
	$(CC) $(CFLAGS) -o wc_import tools/wc_import.c $(TOOL_DOMAIN)
	$(CC) $(CFLAGS) -o wc_merge tools/wc_merge.c $(TOOL_DOMAIN)
	$(CC) $(CFLAGS) -o wc_iefp tools/wc_iefp.c $(TOOL_DOMAIN)
	$(CC) $(CFLAGS) -o wc_compare tools/wc_compare.c $(TOOL_DOMAIN)
	$(CC) $(CFLAGS) -o wc_clean tools/wc_clean.c $(TOOL_DOMAIN)

clean:
	rm -f *.o tests/*.o $(TEST_BINS) wc_import wc_merge wc_iefp wc_compare wc_clean
