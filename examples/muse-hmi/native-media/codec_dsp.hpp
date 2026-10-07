// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <sound/asound.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace muse_audio {
// ALSA mixer controls keep register ownership and locking inside the codec driver.
class Codec {
    int fd = -1;
    snd_ctl_elem_value read(const char* name) {
        snd_ctl_elem_value value{};
        value.id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
        std::strncpy(reinterpret_cast<char*>(value.id.name), name, sizeof(value.id.name)-1);
        if (fd < 0 || ioctl(fd, SNDRV_CTL_IOCTL_ELEM_READ, &value) < 0)
            throw std::runtime_error(std::string("codec control unavailable: ")+name);
        return value;
    }
    void write(snd_ctl_elem_value& value) {
        if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &value) < 0)
            throw std::runtime_error("codec control write failed");
        auto check = value;
        if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_READ, &check) < 0 ||
            std::memcmp(&check.value, &value.value, sizeof(value.value)))
            throw std::runtime_error("codec control readback mismatch");
    }
public:
    Codec() { fd = open("/dev/snd/controlC0", O_RDWR | O_CLOEXEC); }
    ~Codec() { if (fd >= 0) close(fd); }
    bool available() const { return fd >= 0; }
    int get(const char* name, bool enumeration=false) {
        auto value = read(name);
        return enumeration ? value.value.enumerated.item[0] : value.value.integer.value[0];
    }
    void set(const char* name, int enabled, bool enumeration=false) {
        auto value = read(name);
        if (enumeration) value.value.enumerated.item[0] = enabled;
        else value.value.integer.value[0] = enabled;
        write(value);
    }
    std::array<int,5> gains() {
        auto value = read("EQ Parameters");
        std::array<int,5> result{};
        for (int i=0;i<5;i++) result[i] = 12-(value.value.bytes.data[i*2+1]&31);
        return result;
    }
    // There is no EQ enable bit: Off is unity gain, retaining frequencies/bandwidth.
    static void encode(unsigned char* bytes, const std::array<int,5>& gains, bool enabled) {
        for (int i=0;i<5;i++) {
            bytes[i*2+1] = (bytes[i*2+1]&~31) | (12-(enabled?std::clamp(gains[i],-12,12):0));
        }
        bytes[0] |= 1; // R18 bit 8: DAC/playback path; never move EQ to the mic.
    }
    void equalizer(const std::array<int,5>& gains, bool enabled) {
        auto value = read("EQ Parameters");
        encode(value.value.bytes.data, gains, enabled);
        write(value);
    }
};
} // namespace muse_audio
