# SPDX-License-Identifier: Apache-2.0
CROSS_COMPILE ?= aarch64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
CFLAGS ?= -O2
CXXFLAGS ?= -O2 -std=c++17
LDFLAGS ?= -static
BUILD := native-media/build

.PHONY: all check
all: $(BUILD)/muse-media $(BUILD)/youtube-resolve touch_reader framebuffer_writer

$(BUILD):
	mkdir -p $@

$(BUILD)/muse-media: native-media/media_service.cpp native-media/audio_eq.hpp native-media/codec_dsp.hpp native-media/vendor/json.hpp native-media/vendor/stb_image.h native-media/vendor/stb_truetype.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -pthread $< $(LDFLAGS) -o $@

$(BUILD)/youtube-resolve: native-media/youtube_resolve.cpp native-media/vendor/json.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

touch_reader: touch_reader.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

framebuffer_writer: framebuffer_writer.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

# Host-side tests compile the native API/filter tests with host g++.
# Optional gateway tests require the Python packages in requirements.txt.
check:
	python3 -m pytest -q .
