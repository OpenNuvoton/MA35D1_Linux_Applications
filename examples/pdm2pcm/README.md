# PDM-to-PCM Converter

This example converts raw MP34DT01-M PDM into mono S16_LE PCM in userspace. The confirmed MA35D1 release mode matches the ST OpenPDMFilter byte input order: treat ALSA capture as a continuous byte stream and decode each byte MSB-first.

## Release Defaults

The converter defaults are tuned for the MA35D1 I2S1 MP34DT01-M path:

- PCM output sample rate: `47348` Hz.
- Decimation: `64` PDM bits per PCM sample.
- Input format: `packed-bytes`, MSB-first.
- Volume: `4`.
- HPF: `10` Hz.
- LPF: `pcm-rate / 2`.
- Startup discard: `100` ms.
- Gate: disabled by default.
- LPF stages: `1`; voice smoothing: `0`.

Do not use `s16le-packed` as the normal MA35D1 release mode. It reverses byte order inside each 16-bit ALSA sample compared with the ST library input order and can cause scratchy speech artifacts.

## Build

```sh
make
```

## Real-Time Playback

```sh
arecord -q -D hw:1,0 -f S16_LE -r 192000 -c 1 -t raw \
  | /mnt/pdm2pcm \
  | aplay -q -D plughw:0,0 -f S16_LE -r 47348 -c 1 -t raw
```

## File Conversion

```sh
arecord -q -D hw:1,0 -f S16_LE -r 192000 -c 1 -d 5 -t raw /tmp/pdm.raw
/mnt/pdm2pcm /tmp/pdm.raw /tmp/out.pcm
aplay -q -D plughw:0,0 -f S16_LE -r 47348 -c 1 -t raw /tmp/out.pcm
```

## Debug Options

All defaults can still be overridden for bring-up and comparison:

```sh
./pdm2pcm --diag --input-format packed-bytes --bit-order msb /tmp/pdm.raw /tmp/out.pcm
./pdm2pcm --diag --input-format s16le-packed --bit-order msb /tmp/pdm.raw /tmp/out_s16le.pcm
./pdm2pcm --no-hpf --no-lpf /tmp/pdm.raw /tmp/sinc_only.pcm
./pdm2pcm --stats /tmp/pdm.raw /tmp/out.pcm
```

Useful knobs:

- `--pcm-rate HZ`, `--decimation N`, and `--volume N` control the OpenPDMFilter rate model and output level.
- `--input-format` and `--bit-order` are for packing experiments; release mode is `packed-bytes` + `msb`.
- `--lpf-stages N` and `--voice-smooth N` can reduce high-frequency artifacts if needed.
- `--gate-open`, `--gate-close`, and related gate options remain available but are off by default.
- `--stats` prints `avg_abs`, `max_abs`, `rms`, `near_clipping_count`, `clipping_count`, and `gate_active_count`.
