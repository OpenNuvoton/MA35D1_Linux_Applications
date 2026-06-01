// SPDX-License-Identifier: Apache-2.0

/*
* pdm2pcm.c
*
* Userspace PDM-to-PCM converter for MP34DT01-M microphones on MA35D1.
*
* The MA35D1 I2S controller captures the microphone PDM bitstream.
* This utility performs PDM decimation and filtering to generate
* signed 16-bit PCM audio samples.
*
* Portions of the PDM decimation and filtering implementation are
* derived from the STMicroelectronics OpenPDMFilter library.
*
* Copyright (c) 2018 STMicroelectronics
* Modifications Copyright (c) 2026 Nuvoton Technology Corp.
*
* Licensed under the Apache License, Version 2.0.
*/

 #include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SINCN 			3
#define DECIMATION_MAX 		128
#define FILTER_GAIN 		16
#define MAX_VOLUME 		64
#define DEFAULT_PCM_RATE 	47348
#define DEFAULT_VOLUME 		4
#define DEFAULT_HP_HZ 		10
#define DEFAULT_DISCARD_MS 	100

struct st_pdm_filter {
	uint32_t coef[SINCN][DECIMATION_MAX];
	uint32_t prev_coef[SINCN - 1];
	int64_t sub_const;
	int64_t div_const;
	uint16_t lp_alpha;
	uint16_t hp_alpha;
	int64_t old_out;
	int64_t old_in;
	int64_t old_z[3];
	unsigned int pcm_rate;
	unsigned int decimation;
	unsigned int volume;
	int64_t gate_env;
	int32_t gate_gain;
	int32_t gate_min_gain;
	bool gate_open;
	unsigned int gate_hold_samples;
	unsigned int gate_hold_counter;
	unsigned int gate_open_threshold;
	unsigned int gate_close_threshold;
	unsigned int gate_attack_shift;
	unsigned int gate_release_shift;
	unsigned int lpf_stages;
	unsigned int voice_smooth;
	int64_t voice_hist[7];
	bool enable_hpf;
	bool enable_lpf;
};

struct pcm_stats {
	uint64_t samples;
	uint64_t sum_abs;
	long double sum_squares;
	uint32_t max_abs;
	uint64_t near_clipping_count;
	uint64_t clipping_count;
	uint64_t gate_active_count;
};

enum bit_order {
	BIT_ORDER_MSB,
	BIT_ORDER_LSB,
};

enum input_format {
	INPUT_PACKED_BYTES,
	INPUT_S16LE_PACKED,
	INPUT_S16LE_MSB,
	INPUT_S16LE_LSB,
};

struct bit_reader {
	const uint8_t *buf;
	size_t size;
	enum input_format format;
	enum bit_order order;
	size_t bit_pos;
};

static int64_t round_div_s64(int64_t value, int64_t divisor)
{
	if (divisor == 0)
		return 0;
	if (value >= 0)
		return (value + divisor / 2) / divisor;
	return (value - divisor / 2) / divisor;
}

static int16_t clamp_s16(int64_t value)
{
	if (value > 32700)
		return 32700;
	if (value < -32700)
		return -32700;
	return (int16_t)value;
}

static void convolve(const uint32_t *signal, unsigned int signal_len,
		     const uint32_t *kernel, unsigned int kernel_len,
		     uint32_t *result)
{
	unsigned int n, k, kmin, kmax;

	for (n = 0; n < signal_len + kernel_len - 1; n++) {
		result[n] = 0;
		kmin = n >= kernel_len - 1 ? n - (kernel_len - 1) : 0;
		kmax = n < signal_len - 1 ? n : signal_len - 1;
		for (k = kmin; k <= kmax; k++)
			result[n] += signal[k] * kernel[n - k];
	}
}

static int st_pdm_filter_init(struct st_pdm_filter *f, unsigned int pcm_rate,
			      unsigned int decimation, unsigned int volume,
			      unsigned int hp_hz, unsigned int lp_hz,
			      unsigned int gate_open_threshold,
			      unsigned int gate_close_threshold,
			      unsigned int gate_min_gain,
			      unsigned int gate_hold_ms,
			      unsigned int gate_attack_ms,
			      unsigned int gate_release_ms,
			      unsigned int lpf_stages, unsigned int voice_smooth,
			      bool enable_hpf, bool enable_lpf)
{
	uint32_t sinc1[DECIMATION_MAX];
	uint32_t sinc2[DECIMATION_MAX * 2];
	uint32_t sinc[DECIMATION_MAX * SINCN];
	int64_t sum = 0;
	unsigned int i, j;

	if (pcm_rate == 0) {
		fprintf(stderr, "pcm-rate must be non-zero\n");
		return -1;
	}
	if (decimation != 64 && decimation != 128) {
		fprintf(stderr, "this ST-style wrapper currently supports decimation 64 or 128\n");
		return -1;
	}
	if (volume > MAX_VOLUME) {
		fprintf(stderr, "volume must be 0..%u\n", MAX_VOLUME);
		return -1;
	}
	if (gate_close_threshold > gate_open_threshold && gate_open_threshold) {
		fprintf(stderr, "gate-close must be <= gate-open\n");
		return -1;
	}
	if (gate_min_gain > 256) {
		fprintf(stderr, "gate-min-gain must be 0..256\n");
		return -1;
	}
	if (lpf_stages > 3) {
		fprintf(stderr, "lpf-stages must be 0..3\n");
		return -1;
	}
	if (voice_smooth > 3) {
		fprintf(stderr, "voice-smooth must be 0..3\n");
		return -1;
	}

	memset(f, 0, sizeof(*f));
	f->pcm_rate = pcm_rate;
	f->decimation = decimation;
	f->volume = volume;
	f->gate_gain = 256;
	f->gate_min_gain = gate_min_gain;
	f->gate_open = true;
	f->gate_hold_samples = ((uint64_t)pcm_rate * gate_hold_ms) / 1000;
	f->gate_hold_counter = 0;
	f->gate_open_threshold = gate_open_threshold;
	f->gate_close_threshold = gate_close_threshold;
	f->gate_attack_shift = gate_attack_ms <= 5 ? 3 :
		gate_attack_ms <= 10 ? 4 :
		gate_attack_ms <= 20 ? 5 : 6;
	f->gate_release_shift = gate_release_ms <= 50 ? 6 :
		gate_release_ms <= 100 ? 7 :
		gate_release_ms <= 200 ? 8 : 9;
	f->lpf_stages = lpf_stages;
	f->voice_smooth = voice_smooth;
	f->enable_hpf = enable_hpf;
	f->enable_lpf = enable_lpf && lpf_stages > 0;

	memset(sinc1, 0, sizeof(sinc1));
	memset(sinc2, 0, sizeof(sinc2));
	memset(sinc, 0, sizeof(sinc));

	for (i = 0; i < decimation; i++)
		sinc1[i] = 1;

	sinc[0] = 0;
	sinc[decimation * SINCN - 1] = 0;
	convolve(sinc1, decimation, sinc1, decimation, sinc2);
	convolve(sinc2, decimation * 2 - 1, sinc1, decimation, &sinc[1]);

	for (j = 0; j < SINCN; j++) {
		for (i = 0; i < decimation; i++) {
			f->coef[j][i] = sinc[j * decimation + i];
			sum += f->coef[j][i];
		}
	}

	f->sub_const = sum >> 1;
	f->div_const = f->sub_const * MAX_VOLUME / 32768 / FILTER_GAIN;
	if (!f->div_const)
		f->div_const = 1;

	if (lp_hz == 0)
		lp_hz = pcm_rate / 2;

	f->lp_alpha = lp_hz != 0 ?
		(uint16_t)(lp_hz * 256.0 / (lp_hz + pcm_rate / (2.0 * 3.14159))) : 0;
	f->hp_alpha = hp_hz != 0 ?
		(uint16_t)(pcm_rate * 256.0 / (2.0 * 3.14159 * hp_hz + pcm_rate)) : 0;

	return 0;
}

static int64_t voice_smooth_sample(struct st_pdm_filter *f, int64_t sample)
{
	static const int coeffs[3][7] = {
		{ 1, 2, 1, 0, 0, 0, 0 },       /* 3-tap:  [1 2 1] / 4 */
		{ 1, 4, 6, 4, 1, 0, 0 },       /* 5-tap:  [1 4 6 4 1] / 16 */
		{ 1, 6, 15, 20, 15, 6, 1 },    /* 7-tap:  binomial / 64 */
	};
	static const int shifts[3] = { 2, 4, 6 };
	unsigned int level = f->voice_smooth;
	int64_t acc = 0;
	unsigned int i;

	if (!level)
		return sample;

	memmove(&f->voice_hist[1], &f->voice_hist[0],
		(sizeof(f->voice_hist) - sizeof(f->voice_hist[0])));
	f->voice_hist[0] = sample;

	for (i = 0; i < 7; i++)
		acc += f->voice_hist[i] * coeffs[level - 1][i];

	return round_div_s64(acc, 1LL << shifts[level - 1]);
}

static int bit_reader_get(const struct bit_reader *br, size_t pos)
{
	if (br->format == INPUT_PACKED_BYTES) {
		size_t byte_pos = pos >> 3;
		unsigned int bit_in_byte = pos & 7;
		if (byte_pos >= br->size)
			return 0;
		if (br->order == BIT_ORDER_MSB)
			return (br->buf[byte_pos] >> (7 - bit_in_byte)) & 1;
		return (br->buf[byte_pos] >> bit_in_byte) & 1;
	}

	if (br->format == INPUT_S16LE_PACKED) {
		size_t word_pos = pos >> 4;
		unsigned int bit_in_word = pos & 15;
		size_t byte_pos = word_pos * 2;
		uint16_t w;
		if (byte_pos + 1 >= br->size)
			return 0;
		w = (uint16_t)br->buf[byte_pos] | ((uint16_t)br->buf[byte_pos + 1] << 8);
		if (br->order == BIT_ORDER_MSB)
			return (w >> (15 - bit_in_word)) & 1;
		return (w >> bit_in_word) & 1;
	}

	if (br->format == INPUT_S16LE_MSB || br->format == INPUT_S16LE_LSB) {
		size_t byte_pos = pos * 2;
		uint16_t w;
		if (byte_pos + 1 >= br->size)
			return 0;
		w = (uint16_t)br->buf[byte_pos] | ((uint16_t)br->buf[byte_pos + 1] << 8);
		if (br->format == INPUT_S16LE_MSB)
			return (w >> 15) & 1;
		return w & 1;
	}

	return 0;
}

static size_t bits_available_for_format(enum input_format fmt, size_t bytes)
{
	switch (fmt) {
	case INPUT_PACKED_BYTES:
		return bytes * 8;
	case INPUT_S16LE_PACKED:
		return (bytes / 2) * 16;
	case INPUT_S16LE_MSB:
	case INPUT_S16LE_LSB:
		return bytes / 2;
	default:
		return 0;
	}
}

static int64_t filter_table_bits(const struct bit_reader *br,
				 const struct st_pdm_filter *f,
				 unsigned int sincn)
{
	unsigned int i;
	int64_t value = 0;

	for (i = 0; i < f->decimation; i++) {
		if (bit_reader_get(br, br->bit_pos + i))
			value += f->coef[sincn][i];
	}

	return value;
}

static int16_t st_pdm_filter_sample(struct st_pdm_filter *f,
				    const struct bit_reader *br,
				    bool *gate_active, bool *clipped)
{
	int64_t z, z0, z1, z2;

	if (gate_active)
		*gate_active = false;
	if (clipped)
		*clipped = false;

	z0 = filter_table_bits(br, f, 0);
	z1 = filter_table_bits(br, f, 1);
	z2 = filter_table_bits(br, f, 2);

	z = f->prev_coef[1] + z2 - f->sub_const;
	f->prev_coef[1] = f->prev_coef[0] + z1;
	f->prev_coef[0] = (uint32_t)z0;

	if (f->enable_hpf)
		f->old_out = (f->hp_alpha * (f->old_out + z - f->old_in)) >> 8;
	else
		f->old_out = z;
	f->old_in = z;

	z = f->old_out;
	if (f->enable_lpf) {
		unsigned int stage;

		for (stage = 0; stage < f->lpf_stages; stage++) {
			f->old_z[stage] = ((256 - f->lp_alpha) * f->old_z[stage] +
					       f->lp_alpha * z) >> 8;
			z = f->old_z[stage];
		}
	}

	/* Reduce post-decimation high-frequency residue before gain/gating. */
	z = voice_smooth_sample(f, z);

	/* Scale first so --noise-gate is expressed in final S16 PCM units. */
	z *= f->volume;
	z = round_div_s64(z, f->div_const);

	if (f->gate_open_threshold) {
		int64_t abs_z = z >= 0 ? z : -z;
		int32_t target_gain;
		int32_t delta;

		/* Envelope follower: fast rise, slow decay. */
		if (abs_z > f->gate_env)
			f->gate_env = (f->gate_env * 7 + abs_z) / 8;
		else
			f->gate_env = (f->gate_env * 255 + abs_z) / 256;

		/* Hysteresis plus hold time prevents rapid gate chatter. */
		if (f->gate_env >= f->gate_open_threshold) {
			f->gate_open = true;
			f->gate_hold_counter = f->gate_hold_samples;
		} else if (f->gate_hold_counter > 0) {
			f->gate_hold_counter--;
		} else if (f->gate_env < f->gate_close_threshold) {
			f->gate_open = false;
		}

		target_gain = f->gate_open ? 256 : f->gate_min_gain;

		if (target_gain > f->gate_gain) {
			delta = target_gain - f->gate_gain;
			f->gate_gain += delta >> f->gate_attack_shift;
			if (f->gate_gain < target_gain && (delta >> f->gate_attack_shift) == 0)
				f->gate_gain++;
		} else if (target_gain < f->gate_gain) {
			delta = f->gate_gain - target_gain;
			f->gate_gain -= delta >> f->gate_release_shift;
			if (f->gate_gain > target_gain && (delta >> f->gate_release_shift) == 0)
				f->gate_gain--;
		}

		if (f->gate_gain < 256 && gate_active)
			*gate_active = true;

		z = z * f->gate_gain / 256;
	}

	if (z > 32700 || z < -32700) {
		if (clipped)
			*clipped = true;
	}

	return clamp_s16(z);
}

static const char *format_name(enum input_format fmt)
{
	switch (fmt) {
	case INPUT_PACKED_BYTES: return "packed-bytes";
	case INPUT_S16LE_PACKED: return "s16le-packed";
	case INPUT_S16LE_MSB: return "s16le-msb";
	case INPUT_S16LE_LSB: return "s16le-lsb";
	default: return "unknown";
	}
}

static int parse_format(const char *s, enum input_format *fmt)
{
	if (!strcmp(s, "packed-bytes")) {
		*fmt = INPUT_PACKED_BYTES;
		return 0;
	}
	if (!strcmp(s, "s16le-packed")) {
		*fmt = INPUT_S16LE_PACKED;
		return 0;
	}
	if (!strcmp(s, "s16le-msb")) {
		*fmt = INPUT_S16LE_MSB;
		return 0;
	}
	if (!strcmp(s, "s16le-lsb")) {
		*fmt = INPUT_S16LE_LSB;
		return 0;
	}
	return -1;
}

static int parse_bit_order(const char *s, enum bit_order *order)
{
	if (!strcmp(s, "msb")) {
		*order = BIT_ORDER_MSB;
		return 0;
	}
	if (!strcmp(s, "lsb")) {
		*order = BIT_ORDER_LSB;
		return 0;
	}
	return -1;
}

static void print_diagnostics(const uint8_t *buf, size_t bytes,
			      enum input_format fmt, enum bit_order order,
			      unsigned int pcm_rate, unsigned int decimation)
{
	struct bit_reader br = {
		.buf = buf,
		.size = bytes,
		.format = fmt,
		.order = order,
		.bit_pos = 0,
	};
	size_t bits = bits_available_for_format(fmt, bytes);
	size_t sample_bits = bits < 4096 ? bits : 4096;
	size_t ones = 0;
	size_t i;

	for (i = 0; i < sample_bits; i++)
		ones += bit_reader_get(&br, i) ? 1 : 0;

	fprintf(stderr, "diagnostic:\n");
	fprintf(stderr, "  input bytes in first block: %zu\n", bytes);
	fprintf(stderr, "  input format: %s\n", format_name(fmt));
	fprintf(stderr, "  bit order: %s\n", order == BIT_ORDER_MSB ? "msb" : "lsb");
	fprintf(stderr, "  interpreted bits in first block: %zu\n", bits);
	fprintf(stderr, "  configured PCM rate: %u Hz\n", pcm_rate);
	fprintf(stderr, "  configured decimation: %u\n", decimation);
	fprintf(stderr, "  expected PDM bit clock: %u Hz\n", pcm_rate * decimation);
	if (sample_bits)
		fprintf(stderr, "  duty ratio of first %zu bits: %.2f%%\n",
				sample_bits, 100.0 * (double)ones / (double)sample_bits);
	fprintf(stderr, "  first 64 interpreted bits: ");
	for (i = 0; i < bits && i < 64; i++)
		fputc(bit_reader_get(&br, i) ? '1' : '0', stderr);
	fputc('\n', stderr);

#if 1
fprintf(stderr, "  first 32 S16 samples:\n");

for (i = 0; i < 32 && (i * 2 + 1) < bytes; i++) {
	uint16_t w;

	w = (uint16_t)buf[i * 2] |
	    ((uint16_t)buf[i * 2 + 1] << 8);

	fprintf(stderr,
		"[%02zu] 0x%04x  hi=0x%02x lo=0x%02x\n",
		i,
		w,
		(w >> 8) & 0xff,
		w & 0xff);
}
#endif
}

static void pcm_stats_update(struct pcm_stats *stats, int16_t sample)
{
	uint32_t abs_sample = sample < 0 ? (uint32_t)-sample : (uint32_t)sample;

	stats->samples++;
	stats->sum_abs += abs_sample;
	stats->sum_squares += (long double)sample * (long double)sample;
	if (abs_sample > stats->max_abs)
		stats->max_abs = abs_sample;
	if (abs_sample >= 30000)
		stats->near_clipping_count++;
}

static void pcm_stats_print(const struct pcm_stats *stats)
{
	long double avg_abs = 0.0;
	long double rms = 0.0;

	if (stats->samples) {
		avg_abs = (long double)stats->sum_abs / (long double)stats->samples;
		rms = stats->sum_squares / (long double)stats->samples;
		/* Newton iterations avoid requiring libm for sqrtl(). */
		if (rms > 0.0) {
			long double x = rms;
			unsigned int i;

			for (i = 0; i < 16; i++)
				x = 0.5 * (x + rms / x);
			rms = x;
		}
	}

	fprintf(stderr, "pcm_stats:\n");
	fprintf(stderr, "  samples=%" PRIu64 "\n", stats->samples);
	fprintf(stderr, "  avg_abs=%.2Lf\n", avg_abs);
	fprintf(stderr, "  max_abs=%u\n", stats->max_abs);
	fprintf(stderr, "  rms=%.2Lf\n", rms);
	fprintf(stderr, "  near_clipping_count=%" PRIu64 "\n", stats->near_clipping_count);
	fprintf(stderr, "  clipping_count=%" PRIu64 "\n", stats->clipping_count);
	fprintf(stderr, "  gate_active_count=%" PRIu64 "\n", stats->gate_active_count);
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [options] [input.raw|-] [output.pcm|-]\n"
		"\n"
		"Default MA35D1 release mode:\n"
		"  PCM rate 47348 Hz, decimation 64, volume 4, HPF 10 Hz, LPF pcm-rate/2,\n"
		"  input packed-bytes, bit order MSB-first, discard first 100 ms.\n"
		"\n"
		"Options:\n"
		"  -r, --pcm-rate HZ           output PCM sample rate, default 47348\n"
		"  -d, --decimation N          PDM bits per PCM sample, 64 or 128, default 64\n"
		"  -v, --volume N              volume 0..64, default 4\n"
		"      --input-format FMT      packed-bytes | s16le-packed | s16le-msb | s16le-lsb\n"
		"                              default packed-bytes\n"
		"      --bit-order ORDER       msb | lsb, default msb\n"
		"      --hp-hz HZ              high-pass cutoff, default 10\n"
		"      --lp-hz HZ              low-pass cutoff, default pcm-rate/2\n"
		"      --no-hpf                disable high-pass filter\n"
		"      --no-lpf                disable low-pass filter\n"
		"      --lpf-stages N          cascaded LPF stages 0..3, default 1\n"
		"      --voice-smooth N        post-decimation FIR smoothing 0..3, default 0\n"
		"      --gate-open N           gate opens at envelope >= N, default 0/off\n"
		"      --gate-close N          gate closes below N after hold, default 0\n"
		"      --noise-gate N          compatibility alias: open=N close=N/2\n"
		"      --gate-min-gain N       closed-gate floor gain 0..256, default 64\n"
		"      --gate-hold-ms N        keep gate open after speech, default 150\n"
		"      --gate-attack-ms N      gate fade-in time selector, default 5\n"
		"      --gate-release-ms N     gate fade-out time selector, default 200\n"
		"      --discard-ms N          discard first N ms of output PCM, default 100\n"
		"      --diag                  print first-block diagnostics\n"
		"      --stats                 print PCM statistics at exit\n"
		"  -h, --help                  show help\n"
		"\n"
		"Recommended real-time playback:\n"
		"  arecord -q -D hw:1,0 -f S16_LE -r 192000 -c 1 -t raw \\\n"
		"    | %s \\\n"
		"    | aplay -q -D plughw:0,0 -f S16_LE -r 47348 -c 1 -t raw\n"
		"\n"
		"Recommended file conversion:\n"
		"  arecord -q -D hw:1,0 -f S16_LE -r 192000 -c 1 -d 5 -t raw /tmp/pdm.raw\n"
		"  %s /tmp/pdm.raw /tmp/out.pcm\n"
		"  aplay -q -D plughw:0,0 -f S16_LE -r 47348 -c 1 -t raw /tmp/out.pcm\n",
		prog, prog, prog);
}

int main(int argc, char **argv)
{
	static const struct option long_opts[] = {
		{ "pcm-rate", required_argument, NULL, 'r' },
		{ "rate", required_argument, NULL, 'r' },
		{ "decimation", required_argument, NULL, 'd' },
		{ "volume", required_argument, NULL, 'v' },
		{ "input-format", required_argument, NULL, 1000 },
		{ "bit-order", required_argument, NULL, 1001 },
		{ "hp-hz", required_argument, NULL, 1002 },
		{ "lp-hz", required_argument, NULL, 1003 },
		{ "no-hpf", no_argument, NULL, 1004 },
		{ "no-lpf", no_argument, NULL, 1005 },
		{ "discard-ms", required_argument, NULL, 1006 },
		{ "diag", no_argument, NULL, 1007 },
		{ "noise-gate", required_argument, NULL, 1008 },
		{ "lpf-stages", required_argument, NULL, 1009 },
		{ "stats", no_argument, NULL, 1010 },
		{ "gate-min-gain", required_argument, NULL, 1011 },
		{ "gate-open", required_argument, NULL, 1012 },
		{ "gate-close", required_argument, NULL, 1013 },
		{ "gate-hold-ms", required_argument, NULL, 1014 },
		{ "gate-attack-ms", required_argument, NULL, 1015 },
		{ "gate-release-ms", required_argument, NULL, 1016 },
		{ "voice-smooth", required_argument, NULL, 1017 },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};

	unsigned int pcm_rate = DEFAULT_PCM_RATE;
	unsigned int decimation = 64;
	unsigned int volume = DEFAULT_VOLUME;
	unsigned int hp_hz = DEFAULT_HP_HZ;
	unsigned int lp_hz = 0;
	unsigned int discard_ms = DEFAULT_DISCARD_MS;
	unsigned int gate_open = 0;
	unsigned int gate_close = 0;
	unsigned int gate_min_gain = 64;
	unsigned int gate_hold_ms = 150;
	unsigned int gate_attack_ms = 5;
	unsigned int gate_release_ms = 200;
	unsigned int lpf_stages = 1;
	unsigned int voice_smooth = 0;
	bool enable_hpf = true;
	bool enable_lpf = true;
	bool diag = false;
	bool print_stats = false;
	/*
	 * MA35D1 captures the PDM stream through the I2S RX path, but for
	 * ST OpenPDMFilter compatibility the data must be interpreted as a
	 * continuous byte stream, MSB-first. Do not use s16le-packed as the
	 * default because it swaps byte order within each 16-bit ALSA sample
	 * and causes scratchy speech artifacts.
	 */
	enum input_format input_format = INPUT_PACKED_BYTES;
	enum bit_order bit_order = BIT_ORDER_MSB;
	const char *in_path = "-";
	const char *out_path = "-";
	FILE *in = stdin;
	FILE *out = stdout;
	struct st_pdm_filter filter;
	uint8_t *inbuf = NULL;
	int16_t *outbuf = NULL;
	size_t chunk_pcm_samples = 4096;
	size_t chunk_bits;
	size_t chunk_bytes;
	size_t pending_bytes = 0;
	uint64_t total_out_samples = 0;
	uint64_t discard_samples;
	struct pcm_stats stats = {0};
	int opt;


	while ((opt = getopt_long(argc, argv, "r:d:v:h", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'r': pcm_rate = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 'd': decimation = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 'v': volume = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1000:
			if (parse_format(optarg, &input_format)) {
				fprintf(stderr, "invalid input format: %s\n", optarg);
				return 1;
			}
			break;
		case 1001:
			if (parse_bit_order(optarg, &bit_order)) {
				fprintf(stderr, "invalid bit order: %s\n", optarg);
				return 1;
			}
			break;
		case 1002: hp_hz = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1003: lp_hz = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1004: enable_hpf = false; break;
		case 1005: enable_lpf = false; break;
		case 1006: discard_ms = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1007: diag = true; break;
		case 1008:
			gate_open = (unsigned int)strtoul(optarg, NULL, 0);
			gate_close = gate_open / 2;
			break;
		case 1009: lpf_stages = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1010: print_stats = true; break;
		case 1011: gate_min_gain = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1012: gate_open = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1013: gate_close = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1014: gate_hold_ms = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1015: gate_attack_ms = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1016: gate_release_ms = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 1017: voice_smooth = (unsigned int)strtoul(optarg, NULL, 0); break;
		case 'h': usage(argv[0]); return 0;
		default: usage(argv[0]); return 1;
		}
	}

	if (optind < argc)
		in_path = argv[optind++];
	if (optind < argc)
		out_path = argv[optind++];
	if (optind < argc) {
		usage(argv[0]);
		return 1;
	}

	if (gate_open && !gate_close)
		gate_close = gate_open / 2;

	if (st_pdm_filter_init(&filter, pcm_rate, decimation, volume,
			       hp_hz, lp_hz, gate_open, gate_close,
			       gate_min_gain, gate_hold_ms, gate_attack_ms,
			       gate_release_ms, lpf_stages, voice_smooth,
			       enable_hpf, enable_lpf))
		return 1;

	if (strcmp(in_path, "-") != 0) {
		in = fopen(in_path, "rb");
		if (!in) {
			fprintf(stderr, "failed to open %s: %s\n", in_path, strerror(errno));
			return 1;
		}
	}
	if (strcmp(out_path, "-") != 0) {
		out = fopen(out_path, "wb");
		if (!out) {
			fprintf(stderr, "failed to open %s: %s\n", out_path, strerror(errno));
			return 1;
		}
	}

	chunk_bits = chunk_pcm_samples * decimation;
	switch (input_format) {
	case INPUT_PACKED_BYTES:
		chunk_bytes = (chunk_bits + 7) / 8;
		break;
	case INPUT_S16LE_PACKED:
		chunk_bytes = ((chunk_bits + 15) / 16) * 2;
		break;
	case INPUT_S16LE_MSB:
	case INPUT_S16LE_LSB:
		chunk_bytes = chunk_bits * 2;
		break;
	default:
		return 1;
	}

	inbuf = malloc(chunk_bytes + 4096);
	outbuf = malloc(chunk_pcm_samples * sizeof(*outbuf));
	if (!inbuf || !outbuf) {
		fprintf(stderr, "out of memory\n");
		free(inbuf);
		free(outbuf);
		return 1;
	}

	discard_samples = (uint64_t)pcm_rate * discard_ms / 1000;

	fprintf(stderr,
		"ma35d1_st_pdm2pcm: pcm_rate=%u decimation=%u expected_pdm_clock=%u "
		"volume=%u hp_hz=%u lp_hz=%u "
		"input_format=%s bit_order=%s "
		"hpf=%s lpf=%s lpf_stages=%u voice_smooth=%u "
		"gate_open=%u gate_close=%u gate_min_gain=%u "
		"gate_hold_ms=%u gate_attack_ms=%u gate_release_ms=%u "
		"discard_ms=%u stats=%s\n",
		pcm_rate, decimation, pcm_rate * decimation, volume,
		hp_hz, lp_hz ? lp_hz : pcm_rate / 2,
		format_name(input_format), bit_order == BIT_ORDER_MSB ? "msb" : "lsb",
		enable_hpf ? "on" : "off",
		filter.enable_lpf ? "on" : "off", filter.lpf_stages,
		filter.voice_smooth, gate_open, gate_close, gate_min_gain,
		gate_hold_ms, gate_attack_ms, gate_release_ms, discard_ms,
		print_stats ? "on" : "off");

	for (;;) {
		size_t got;
		size_t available_bytes;
		size_t bits;
		size_t groups;
		size_t i;
		size_t bytes_used;

		got = fread(inbuf + pending_bytes, 1, chunk_bytes, in);
		available_bytes = pending_bytes + got;

		if (diag && available_bytes > 0) {
			print_diagnostics(inbuf, available_bytes, input_format, bit_order,
					  pcm_rate, decimation);
			diag = false;
		}

		bits = bits_available_for_format(input_format, available_bytes);
		groups = bits / decimation;
		if (groups > chunk_pcm_samples)
			groups = chunk_pcm_samples;

		if (groups) {
			struct bit_reader br = {
				.buf = inbuf,
				.size = available_bytes,
				.format = input_format,
				.order = bit_order,
				.bit_pos = 0,
			};
			size_t produced = 0;

			for (i = 0; i < groups; i++) {
				int16_t sample;
				bool gate_active = false;
				bool clipped = false;

				br.bit_pos = i * decimation;
				sample = st_pdm_filter_sample(&filter, &br,
							       &gate_active, &clipped);
				if (total_out_samples >= discard_samples) {
					outbuf[produced++] = sample;
					pcm_stats_update(&stats, sample);
					if (gate_active)
						stats.gate_active_count++;
					if (clipped)
						stats.clipping_count++;
				}
				total_out_samples++;
			}

			if (produced && fwrite(outbuf, sizeof(*outbuf), produced, out) != produced) {
				fprintf(stderr, "write failed: %s\n", strerror(errno));
				free(inbuf);
				free(outbuf);
				return 1;
			}
		}

		switch (input_format) {
		case INPUT_PACKED_BYTES:
			bytes_used = (groups * decimation) / 8;
			break;
		case INPUT_S16LE_PACKED:
			bytes_used = ((groups * decimation) / 16) * 2;
			break;
		case INPUT_S16LE_MSB:
		case INPUT_S16LE_LSB:
			bytes_used = groups * decimation * 2;
			break;
		default:
			bytes_used = available_bytes;
			break;
		}

		pending_bytes = available_bytes - bytes_used;
		if (pending_bytes && bytes_used)
			memmove(inbuf, inbuf + bytes_used, pending_bytes);

		if (got != chunk_bytes) {
			if (ferror(in)) {
				fprintf(stderr, "read failed: %s\n", strerror(errno));
				free(inbuf);
				free(outbuf);
				return 1;
			}
			break;
		}
	}

	if (print_stats)
		pcm_stats_print(&stats);

	free(inbuf);
	free(outbuf);
	if (in != stdin)
		fclose(in);
	if (out != stdout)
		fclose(out);
	return 0;
}
