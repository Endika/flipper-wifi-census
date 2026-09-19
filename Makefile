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

.PHONY: all help test prepare fap clean clean_firmware format format-check linter

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
TEST_BINS = test_wc_smoke test_wc_signature test_wc_census test_wc_codec test_wc_compare test_wc_known

test: $(TEST_BINS)
	./test_wc_smoke
	./test_wc_signature
	./test_wc_census
	./test_wc_codec
	./test_wc_compare
	./test_wc_known

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
linter:
	cppcheck --enable=all --inline-suppr --error-exitcode=1 -I. \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction:main.c \
		--suppress=checkersReport \
		--suppress=normalCheckLevelMaxBranches \
		src/domain/wc_version_info.c \
		src/domain/wc_observation.c \
		src/domain/wc_signature.c \
		src/domain/wc_census.c \
		src/domain/wc_capture_codec.c \
		src/domain/wc_compare.c \
		src/domain/wc_known.c \
		tests/test_smoke.c \
		tests/test_signature.c \
		tests/test_census.c \
		tests/test_codec.c \
		tests/test_compare.c \
		tests/test_known.c

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

clean:
	rm -f *.o tests/*.o $(TEST_BINS)
