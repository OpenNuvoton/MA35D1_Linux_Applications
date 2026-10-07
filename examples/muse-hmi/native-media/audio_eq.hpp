// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace muse_audio {
constexpr int rate = 48000;
constexpr int bands = 10;
constexpr double pi = 3.14159265358979323846;
constexpr std::array<double, bands> frequencies = {
    31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

// Stereo peaking filters; flat bands bypass arithmetic and preserve samples.
class Equalizer {
    struct Filter {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1[2]{}, z2[2]{};
        double gain = 0;
        void configure(double frequency, double db) {
            gain = db;
            const double a = std::pow(10.0, db / 40.0);
            const double w = 2 * pi * frequency / rate;
            const double alpha = std::sin(w) / (2 * 1.41421356237);
            const double denominator = 1 + alpha / a;
            b0 = (1 + alpha * a) / denominator;
            b1 = -2 * std::cos(w) / denominator;
            b2 = (1 - alpha * a) / denominator;
            a1 = b1;
            a2 = (1 - alpha / a) / denominator;
        }
        double process(double x, int channel) {
            double y = b0 * x + z1[channel];
            z1[channel] = b1 * x - a1 * y + z2[channel];
            z2[channel] = b2 * x - a2 * y;
            return y;
        }
    };
    std::array<Filter, bands> filters{};
public:
    void configure(const std::array<double, bands>& gains, bool smooth = true) {
        for (int i = 0; i < bands; ++i) {
            double target = std::clamp(gains[i], -12.0, 12.0);
            double next = smooth ? filters[i].gain +
                std::clamp(target - filters[i].gain, -1.0, 1.0) : target;
            filters[i].configure(frequencies[i], next);
        }
    }
    void process(int16_t* pcm, size_t frames) {
        for (size_t frame = 0; frame < frames; ++frame) {
            for (int channel = 0; channel < 2; ++channel) {
                double sample = pcm[frame * 2 + channel];
                for (auto& filter : filters)
                    if (std::abs(filter.gain) > 0.00001)
                        sample = filter.process(sample, channel);
                pcm[frame * 2 + channel] = static_cast<int16_t>(
                    std::clamp(std::round(sample), -32768.0, 32767.0));
            }
        }
    }
};

// 4096-point Hann-windowed FFT. Spectrum is measured after EQ, before volume.
class Spectrum {
    static constexpr size_t count = 4096;
    std::array<double, count> samples{}, window{}, real{}, imaginary{};
    std::array<double, bands> envelope{};
    size_t used = 0;
    void analyze() {
        for (size_t i = 0; i < count; ++i) {
            real[i] = samples[i] * window[i];
            imaginary[i] = 0;
        }
        for (size_t i = 1, j = 0; i < count; ++i) {
            size_t bit = count >> 1;
            while (j & bit) { j ^= bit; bit >>= 1; }
            j ^= bit;
            if (i < j) std::swap(real[i], real[j]);
        }
        for (size_t length = 2; length <= count; length <<= 1) {
            double wr = std::cos(-2 * pi / length);
            double wi = std::sin(-2 * pi / length);
            for (size_t start = 0; start < count; start += length) {
                double ur = 1, ui = 0;
                for (size_t j = 0; j < length / 2; ++j) {
                    size_t even = start + j, odd = even + length / 2;
                    double vr = real[odd] * ur - imaginary[odd] * ui;
                    double vi = real[odd] * ui + imaginary[odd] * ur;
                    real[odd] = real[even] - vr;
                    imaginary[odd] = imaginary[even] - vi;
                    real[even] += vr; imaginary[even] += vi;
                    double next = ur * wr - ui * wi;
                    ui = ur * wi + ui * wr; ur = next;
                }
            }
        }
        for (int band = 0; band < bands; ++band) {
            size_t first = std::max<size_t>(1, std::ceil(
                frequencies[band] / std::sqrt(2.0) * count / rate));
            size_t last = std::min<size_t>(count / 2 - 1, std::floor(
                frequencies[band] * std::sqrt(2.0) * count / rate));
            double power = 0;
            for (size_t bin = first; bin <= last; ++bin)
                power += real[bin] * real[bin] + imaginary[bin] * imaginary[bin];
            double amplitude = std::sqrt(power) * 4 / count;
            double db = 20 * std::log10(std::max(amplitude, 1e-9));
            double value = std::clamp((db + 72) / 72, 0.0, 1.0);
            envelope[band] = value > envelope[band] ? value :
                0.7 * envelope[band] + 0.3 * value;
        }
    }
public:
    Spectrum() {
        for (size_t i = 0; i < count; ++i)
            window[i] = 0.5 - 0.5 * std::cos(2 * pi * i / (count - 1));
    }
    bool process(const int16_t* pcm, size_t frames) {
        bool updated = false;
        for (size_t frame = 0; frame < frames; ++frame) {
            samples[used++] = (pcm[frame * 2] + pcm[frame * 2 + 1]) / 65536.0;
            if (used == count) { analyze(); used = 0; updated = true; }
        }
        return updated;
    }
    const std::array<double, bands>& levels() const { return envelope; }
};
} // namespace muse_audio
