# SPDX-License-Identifier: Apache-2.0
CROSS_COMPILE ?= aarch64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
CFLAGS ?= -O2
CXXFLAGS ?= -O2 -std=c++17
LDFLAGS ?= -static
BUILD := native-media/build
CORE := native-media/vendor/noise_core
CORE_SOURCES := $(addprefix $(CORE)/src/,ClientSession.cpp InitiatorHandshake.cpp Transport.cpp TransportFrameCodec.cpp ServiceCodec.cpp Status.cpp)
# For a cross build, point TLSLIB at OpenSSL built with the same target toolchain.
TLSLIB ?=
TLS_CPPFLAGS = $(if $(TLSLIB),-I$(TLSLIB)/include)
TLS_LDFLAGS = $(if $(TLSLIB),-L$(TLSLIB)) -lssl -lcrypto -ldl
# Enable only for compact static builds that cannot load the BSP's NSS modules.
USE_ICS_DNS ?= 0
DNS_OBJECT = $(if $(filter 1,$(USE_ICS_DNS)),$(BUILD)/ics_dns.o)
NATIVE_HEADERS := $(wildcard native-media/*.hpp native-media/vendor/*.h native-media/vendor/*.hpp)

.PHONY: all check check-crypto
all: $(BUILD)/muse-media $(BUILD)/muse-gateway $(BUILD)/quick-present $(BUILD)/youtube-resolve touch_reader framebuffer_writer

$(BUILD):
	mkdir -p $@

$(BUILD)/muse-media: native-media/media_service.cpp $(NATIVE_HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -pthread $< $(LDFLAGS) -o $@

$(BUILD)/muse-gateway: native-media/board_gateway.cpp $(NATIVE_HEADERS) $(CORE_SOURCES) $(wildcard $(CORE)/include/xplat/noise/core/*.h) $(DNS_OBJECT) | $(BUILD)
	$(CXX) $(CXXFLAGS) -pthread $(TLS_CPPFLAGS) -I$(CORE)/include $< $(CORE_SOURCES) $(DNS_OBJECT) $(LDFLAGS) $(TLS_LDFLAGS) -o $@

$(BUILD)/quick-present: native-media/quick_present.cpp $(NATIVE_HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -pthread $< $(LDFLAGS) -o $@

$(BUILD)/ics_dns.o: native-media/ics_dns.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

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

# Uses the target compiler. For a host check, pass CROSS_COMPILE= LDFLAGS=.
check-crypto: $(BUILD)/test-gateway-crypto
	./$(BUILD)/test-gateway-crypto

$(BUILD)/test-gateway-crypto: native-media/test_gateway_crypto.cpp native-media/openssl_backend.hpp $(wildcard $(CORE)/include/xplat/noise/core/*.h) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(TLS_CPPFLAGS) -I$(CORE)/include $< $(LDFLAGS) $(TLS_LDFLAGS) -o $@
