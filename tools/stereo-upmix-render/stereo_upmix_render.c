/*
 * Host-side audition tool for hook/audio/stereo_upmix.c.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "stereo_upmix.h"
#define RENDER_SURROUND_MAX 0.5

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    WAV_FORMAT_PCM = 0x0001,
    WAV_FORMAT_EXTENSIBLE = 0xfffe,
    WAV_INPUT_CHANNELS = 2,
    WAV_OUTPUT_CHANNELS = 6,
    WAV_SAMPLE_RATE = 48000,
    WAV_BITS_PER_SAMPLE = 16,
    WAV_INPUT_BLOCK_ALIGN = WAV_INPUT_CHANNELS * (WAV_BITS_PER_SAMPLE / 8),
    WAV_OUTPUT_BLOCK_ALIGN = WAV_OUTPUT_CHANNELS * (WAV_BITS_PER_SAMPLE / 8),
    WAV_OUTPUT_FMT_SIZE = 40,
    WAV_OUTPUT_HEADER_SIZE = 68,

    /* FL | FR | FC | LFE | SL | SR. */
    WAV_OUTPUT_CHANNEL_MASK = 0x0000060f
};

struct wav_input {
    uint16_t format_tag;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint16_t valid_bits_per_sample;
    uint32_t channel_mask;
    uint32_t data_bytes;
    fpos_t data_position;
};

enum cli_lfe_mode {
    CLI_LFE_OFF = 0,
    CLI_LFE_ASSIST = 1
};

struct options {
    const char *input_path;
    const char *output_path;
    enum spa_stereo_upmix_backend backend;
    enum cli_lfe_mode lfe_mode;
    double surround_gain;
    double front_bass_db;
    double front_retention;
    int bed_delays;
    int bed_attack;
    double attack_collapse;
    double rear_body_db;
    double vocal_gate;
    double rear_air_db;              /* <= -100 = off */
    double front_treble_db;
    const char *protect_mask_path;   /* HSSRMSK0 file, see mask_net.py export */
    double mask_drive;               /* > 0: the mask drives the rear object (test build) */
    long retention_hf_lo_hz;
    long retention_hf_hi_hz;
    double lfe_gain_db;            /* re SPA_STEREO_UPMIX_LFE_Q15_DEFAULT */
    const char *model_path;
    const char *guard_model_path;
};

static uint16_t load_u16le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

static uint32_t load_u32le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void store_u16le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void store_u32le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static int read_exact(FILE *stream, void *buffer, size_t bytes)
{
    return bytes == 0 || fread(buffer, 1, bytes, stream) == bytes;
}

static int write_exact(FILE *stream, const void *buffer, size_t bytes)
{
    return bytes == 0 || fwrite(buffer, 1, bytes, stream) == bytes;
}

static int skip_bytes(FILE *stream, uint64_t bytes)
{
    while (bytes != 0) {
        long step = bytes > (uint64_t)LONG_MAX ? LONG_MAX : (long)bytes;

        if (fseek(stream, step, SEEK_CUR) != 0) return 0;
        bytes -= (uint64_t)step;
    }
    return 1;
}

static int guid_is_pcm(const uint8_t *guid)
{
    static const uint8_t pcm_guid[16] = {
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00,
        0x10, 0x00,
        0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
    };

    return memcmp(guid, pcm_guid, sizeof(pcm_guid)) == 0;
}

static int parse_fmt_chunk(FILE *stream, uint32_t chunk_size,
                           struct wav_input *input)
{
    uint8_t bytes[40];
    size_t wanted;

    if (chunk_size < 16) {
        fprintf(stderr, "error: WAV fmt chunk is only %u bytes\n",
                (unsigned)chunk_size);
        return 0;
    }

    memset(bytes, 0, sizeof(bytes));
    wanted = chunk_size < sizeof(bytes) ? (size_t)chunk_size : sizeof(bytes);
    if (!read_exact(stream, bytes, wanted)) {
        fprintf(stderr, "error: truncated WAV fmt chunk\n");
        return 0;
    }
    if (!skip_bytes(stream, (uint64_t)chunk_size - wanted)) {
        fprintf(stderr, "error: cannot skip WAV fmt extension\n");
        return 0;
    }

    input->format_tag = load_u16le(bytes + 0);
    input->channels = load_u16le(bytes + 2);
    input->sample_rate = load_u32le(bytes + 4);
    input->byte_rate = load_u32le(bytes + 8);
    input->block_align = load_u16le(bytes + 12);
    input->bits_per_sample = load_u16le(bytes + 14);
    input->valid_bits_per_sample = input->bits_per_sample;
    input->channel_mask = 0;

    if (input->format_tag == WAV_FORMAT_EXTENSIBLE) {
        uint16_t extension_size;

        if (chunk_size < 40) {
            fprintf(stderr,
                    "error: truncated WAVE_FORMAT_EXTENSIBLE fmt chunk\n");
            return 0;
        }
        extension_size = load_u16le(bytes + 16);
        if (extension_size < 22 || !guid_is_pcm(bytes + 24)) {
            fprintf(stderr,
                    "error: WAVE_FORMAT_EXTENSIBLE input is not PCM\n");
            return 0;
        }
        input->valid_bits_per_sample = load_u16le(bytes + 18);
        input->channel_mask = load_u32le(bytes + 20);
    } else if (input->format_tag != WAV_FORMAT_PCM) {
        fprintf(stderr, "error: WAV format 0x%04x is not integer PCM\n",
                input->format_tag);
        return 0;
    }

    return 1;
}

static int parse_wav(FILE *stream, struct wav_input *input)
{
    uint8_t riff[12];
    uint64_t remaining;
    int have_fmt = 0;
    int have_data = 0;

    memset(input, 0, sizeof(*input));
    if (!read_exact(stream, riff, sizeof(riff))) {
        fprintf(stderr, "error: input is shorter than a WAV header\n");
        return 0;
    }
    if (memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "error: input is not a little-endian RIFF/WAVE file\n");
        return 0;
    }
    if (load_u32le(riff + 4) < 4) {
        fprintf(stderr, "error: invalid RIFF size\n");
        return 0;
    }
    remaining = (uint64_t)load_u32le(riff + 4) - 4;

    while (remaining >= 8) {
        uint8_t header[8];
        uint32_t chunk_size;
        uint64_t padded_size;

        if (!read_exact(stream, header, sizeof(header))) {
            fprintf(stderr, "error: truncated WAV chunk header\n");
            return 0;
        }
        remaining -= 8;
        chunk_size = load_u32le(header + 4);
        padded_size = (uint64_t)chunk_size + (chunk_size & 1u);
        if (padded_size > remaining) {
            fprintf(stderr, "error: WAV chunk exceeds declared RIFF size\n");
            return 0;
        }

        if (memcmp(header, "fmt ", 4) == 0 && !have_fmt) {
            if (!parse_fmt_chunk(stream, chunk_size, input)) return 0;
            if ((chunk_size & 1u) != 0 && !skip_bytes(stream, 1)) {
                fprintf(stderr, "error: truncated WAV chunk padding\n");
                return 0;
            }
            have_fmt = 1;
        } else if (memcmp(header, "data", 4) == 0 && !have_data) {
            if (fgetpos(stream, &input->data_position) != 0) {
                fprintf(stderr, "error: cannot record WAV data position\n");
                return 0;
            }
            input->data_bytes = chunk_size;
            have_data = 1;

            /* The normal fmt-before-data case can begin streaming now. */
            if (have_fmt) break;
            if (!skip_bytes(stream, padded_size)) {
                fprintf(stderr, "error: cannot skip WAV data before fmt\n");
                return 0;
            }
        } else if (!skip_bytes(stream, padded_size)) {
            fprintf(stderr, "error: cannot skip WAV chunk\n");
            return 0;
        }

        remaining -= padded_size;
        if (have_fmt && have_data) break;
    }

    if (!have_fmt || !have_data) {
        fprintf(stderr, "error: WAV is missing its %s chunk\n",
                !have_fmt ? "fmt" : "data");
        return 0;
    }
    if (fsetpos(stream, &input->data_position) != 0) {
        fprintf(stderr, "error: cannot seek to WAV sample data\n");
        return 0;
    }
    return 1;
}

static int validate_wav(const struct wav_input *input, uint32_t *frames)
{
    uint64_t expected_byte_rate;
    uint64_t output_bytes;

    if (input->channels != WAV_INPUT_CHANNELS ||
        input->sample_rate != WAV_SAMPLE_RATE ||
        input->bits_per_sample != WAV_BITS_PER_SAMPLE ||
        input->valid_bits_per_sample != WAV_BITS_PER_SAMPLE) {
        fprintf(stderr,
                "error: input must be 48 kHz, 16-bit, stereo PCM "
                "(got %u Hz, %u/%u bits, %u channels)\n",
                (unsigned)input->sample_rate,
                (unsigned)input->valid_bits_per_sample,
                (unsigned)input->bits_per_sample,
                (unsigned)input->channels);
        return 0;
    }
    expected_byte_rate = (uint64_t)WAV_SAMPLE_RATE * WAV_INPUT_BLOCK_ALIGN;
    if (input->block_align != WAV_INPUT_BLOCK_ALIGN ||
        input->byte_rate != expected_byte_rate) {
        fprintf(stderr,
                "error: malformed stereo PCM geometry "
                "(block_align=%u byte_rate=%u)\n",
                (unsigned)input->block_align,
                (unsigned)input->byte_rate);
        return 0;
    }
    if (input->format_tag == WAV_FORMAT_EXTENSIBLE &&
        input->channel_mask != 0 && input->channel_mask != 0x00000003u) {
        fprintf(stderr,
                "error: extensible stereo channel mask 0x%08x is not FL|FR\n",
                (unsigned)input->channel_mask);
        return 0;
    }
    if ((input->data_bytes % WAV_INPUT_BLOCK_ALIGN) != 0) {
        fprintf(stderr, "error: WAV data ends in a partial stereo frame\n");
        return 0;
    }

    *frames = input->data_bytes / WAV_INPUT_BLOCK_ALIGN;
    output_bytes = (uint64_t)*frames * WAV_OUTPUT_BLOCK_ALIGN;
    if (output_bytes > UINT32_MAX - 60u) {
        fprintf(stderr,
                "error: output is too large for a canonical RIFF/WAVE file\n");
        return 0;
    }
    return 1;
}

static int write_wav_header(FILE *stream, uint32_t frames)
{
    uint8_t header[WAV_OUTPUT_HEADER_SIZE];
    uint32_t data_bytes = frames * WAV_OUTPUT_BLOCK_ALIGN;

    memset(header, 0, sizeof(header));
    memcpy(header + 0, "RIFF", 4);
    store_u32le(header + 4, data_bytes + 60u);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    store_u32le(header + 16, WAV_OUTPUT_FMT_SIZE);
    store_u16le(header + 20, WAV_FORMAT_EXTENSIBLE);
    store_u16le(header + 22, WAV_OUTPUT_CHANNELS);
    store_u32le(header + 24, WAV_SAMPLE_RATE);
    store_u32le(header + 28, WAV_SAMPLE_RATE * WAV_OUTPUT_BLOCK_ALIGN);
    store_u16le(header + 32, WAV_OUTPUT_BLOCK_ALIGN);
    store_u16le(header + 34, WAV_BITS_PER_SAMPLE);
    store_u16le(header + 36, 22); /* cbSize */
    store_u16le(header + 38, WAV_BITS_PER_SAMPLE);
    store_u32le(header + 40, WAV_OUTPUT_CHANNEL_MASK);
    store_u32le(header + 44, WAV_FORMAT_PCM); /* PCM sub-format GUID */
    store_u16le(header + 48, 0x0000);
    store_u16le(header + 50, 0x0010);
    header[52] = 0x80;
    header[53] = 0x00;
    header[54] = 0x00;
    header[55] = 0xaa;
    header[56] = 0x00;
    header[57] = 0x38;
    header[58] = 0x9b;
    header[59] = 0x71;
    memcpy(header + 60, "data", 4);
    store_u32le(header + 64, data_bytes);
    return write_exact(stream, header, sizeof(header));
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--mode off|hssr-universal] "
            "[--model HSSR.bin] "
            "[--lfe off|assist] [--lfe-gain -24..12] [--surround 0.0..0.5] [--front-bass -12..0] "
            "[--front-retention 0..1] [--bed-delays] [--bed-attack off|owner|front] [--attack-collapse 0..1] [--retention-hf LO:HI] [--rear-body -12..0] [--vocal-gate 0..1] [--rear-air -40..0] [--front-treble -12..0] [--protect-mask FILE] [--mask-drive 0..2] "
            "INPUT.wav OUTPUT.wav\n"
            "  --surround controls OUR rear gain, not the MMI Surround setting.\n",
            program);
}

static int parse_mode(const char *text, enum spa_stereo_upmix_backend *backend)
{
    if (strcmp(text, "off") == 0) {
        *backend = SPA_STEREO_UPMIX_OFF;
        return 1;
    }
    if (strcmp(text, "hssr-universal") == 0) {
        *backend = SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
        return 1;
    }
    return 0;
}

static const char *backend_mode_name(enum spa_stereo_upmix_backend backend)
{
    switch (backend) {
    case SPA_STEREO_UPMIX_OFF:           return "off";
    case SPA_STEREO_UPMIX_HSSR_UNIVERSAL:return "hssr-universal";
    default:                             return "unsupported";
    }
}

static int parse_lfe_mode(const char *text, enum cli_lfe_mode *mode)
{
    if (strcmp(text, "off") == 0) {
        *mode = CLI_LFE_OFF;
        return 1;
    }
    if (strcmp(text, "assist") == 0) {
        *mode = CLI_LFE_ASSIST;
        return 1;
    }
    return 0;
}

static int parse_gain(const char *text, double *gain)
{
    char *end = NULL;
    double parsed;

    errno = 0;
    parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' ||
        !isfinite(parsed) || parsed < 0.0 || parsed > RENDER_SURROUND_MAX) return 0;
    *gain = parsed;
    return 1;
}

static int parse_options(int argc, char **argv, struct options *options)
{
    int index;

    options->input_path = NULL;
    options->output_path = NULL;
    options->backend = SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
    options->lfe_mode = CLI_LFE_OFF;
    options->surround_gain =
        (double)SPA_STEREO_UPMIX_REAR_Q15_DEFAULT / 32768.0;
    options->front_bass_db = 0.0;
    options->front_retention = 0.0;
    options->bed_delays = 0;
    options->bed_attack = 0;
    options->attack_collapse = 1.0;
    options->rear_body_db = -4.0;
    options->vocal_gate = SPA_STEREO_UPMIX_VOCAL_GATE_Q15_DEFAULT / 32768.0;
    options->rear_air_db = -1000.0;
    options->front_treble_db = 0.0;
    options->protect_mask_path = NULL;
    options->mask_drive = 0.0;
    options->retention_hf_lo_hz = SPA_STEREO_UPMIX_RETENTION_HF_LO_HZ;
    options->retention_hf_hi_hz = SPA_STEREO_UPMIX_RETENTION_HF_HI_HZ;
    options->lfe_gain_db = 0.0;
    options->model_path = NULL;
    options->guard_model_path = NULL;

    for (index = 1; index < argc; index++) {
        const char *argument = argv[index];

        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0) {
            usage(argv[0]);
            exit(0);
        }
        if (strcmp(argument, "--mode") == 0) {
            if (++index >= argc || !parse_mode(argv[index], &options->backend)) {
                fprintf(stderr,
                        "error: invalid --mode (see --help; hssr-universal is the current vehicle profile)\n");
                return 0;
            }
            continue;
        }
        if (strncmp(argument, "--mode=", 7) == 0) {
            if (!parse_mode(argument + 7, &options->backend)) {
                fprintf(stderr,
                        "error: invalid --mode (see --help; hssr-universal is the current vehicle profile)\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--model") == 0) {
            if (++index >= argc || !argv[index][0]) {
                fprintf(stderr, "error: --model requires a path\n");
                return 0;
            }
            options->model_path = argv[index];
            continue;
        }
        if (strcmp(argument, "--guard-model") == 0) {
            if (++index >= argc || !argv[index][0]) {
                fprintf(stderr, "error: --guard-model requires a path\n");
                return 0;
            }
            options->guard_model_path = argv[index];
            continue;
        }
        if (strncmp(argument, "--model=", 8) == 0) {
            if (!argument[8]) {
                fprintf(stderr, "error: --model requires a path\n");
                return 0;
            }
            options->model_path = argument + 8;
            continue;
        }
        if (strcmp(argument, "--lfe") == 0) {
            if (++index >= argc ||
                !parse_lfe_mode(argv[index], &options->lfe_mode)) {
                fprintf(stderr, "error: --lfe must be off or assist\n");
                return 0;
            }
            continue;
        }
        if (strncmp(argument, "--lfe=", 6) == 0) {
            if (!parse_lfe_mode(argument + 6, &options->lfe_mode)) {
                fprintf(stderr, "error: --lfe must be off or assist\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--surround") == 0) {
            if (++index >= argc ||
                !parse_gain(argv[index], &options->surround_gain)) {
                fprintf(stderr,
                        "error: --surround must be between 0.0 and %g\n", RENDER_SURROUND_MAX);
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--front-retention") == 0) {
            char *end;
            if (++index >= argc) return 0;
            options->front_retention = strtod(argv[index], &end);
            if (end == argv[index] || *end || !isfinite(options->front_retention) ||
                options->front_retention < 0.0 || options->front_retention > 1.0) {
                fprintf(stderr, "error: --front-retention must be finite 0..1\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--bed-delays") == 0) {
            options->bed_delays = 1;
            continue;
        }
        if (strcmp(argument, "--bed-attack") == 0) {
            if (++index >= argc) return 0;
            if (strcmp(argv[index], "off") == 0) options->bed_attack = 0;
            else if (strcmp(argv[index], "owner") == 0) options->bed_attack = 1;
            else if (strcmp(argv[index], "front") == 0) options->bed_attack = 2;
            else { fprintf(stderr, "error: --bed-attack must be off|owner|front\n"); return 0; }
            continue;
        }
        if (strcmp(argument, "--attack-collapse") == 0) {
            char *end;
            if (++index >= argc) return 0;
            options->attack_collapse = strtod(argv[index], &end);
            if (end == argv[index] || *end || !isfinite(options->attack_collapse) ||
                options->attack_collapse < 0.0 || options->attack_collapse > 1.0) {
                fprintf(stderr, "error: --attack-collapse must be finite 0..1\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--vocal-gate") == 0) {
            char *end;
            if (++index >= argc) return 0;
            options->vocal_gate = strtod(argv[index], &end);
            if (end == argv[index] || *end || !isfinite(options->vocal_gate) ||
                options->vocal_gate < 0.0 || options->vocal_gate > 1.0) {
                fprintf(stderr, "error: --vocal-gate must be 0..1\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--protect-mask") == 0) {
            if (++index >= argc) return 0;
            options->protect_mask_path = argv[index];
            continue;
        }
        if (strcmp(argument, "--mask-drive") == 0) {
            char *end;
            if (++index >= argc) return 0;
            options->mask_drive = strtod(argv[index], &end);
            if (end == argv[index] || *end || !isfinite(options->mask_drive) ||
                options->mask_drive < 0.0 || options->mask_drive > 2.0) {
                fprintf(stderr, "error: --mask-drive must be 0..2\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--front-treble") == 0) {
            char *end;
            if (++index >= argc) return 0;
            options->front_treble_db = strtod(argv[index], &end);
            if (end == argv[index] || *end || !isfinite(options->front_treble_db) ||
                options->front_treble_db < -12.0 || options->front_treble_db > 0.0) {
                fprintf(stderr, "error: --front-treble must be -12..0 dB\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--rear-air") == 0) {
            char *end;
            if (++index >= argc) return 0;
            options->rear_air_db = strtod(argv[index], &end);
            if (end == argv[index] || *end || !isfinite(options->rear_air_db) ||
                options->rear_air_db < -40.0 || options->rear_air_db > 0.0) {
                fprintf(stderr, "error: --rear-air must be -40..0 dB\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--rear-body") == 0) {
            char *end;
            if (++index >= argc) return 0;
            options->rear_body_db = strtod(argv[index], &end);
            if (end == argv[index] || *end || !isfinite(options->rear_body_db) ||
                options->rear_body_db < -12.0 || options->rear_body_db > 0.0) {
                fprintf(stderr, "error: --rear-body must be -12..0 dB\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--retention-hf") == 0) {
            char *end;
            long lo, hi;
            if (++index >= argc) return 0;
            lo = strtol(argv[index], &end, 10);
            if (end == argv[index] || *end != ':') {
                fprintf(stderr, "error: --retention-hf wants LO:HI in Hz\n");
                return 0;
            }
            hi = strtol(end + 1, &end, 10);
            if (*end || lo < 0 || hi < 0 || lo > 24000 || hi > 24000) {
                fprintf(stderr, "error: --retention-hf wants 0..24000 Hz\n");
                return 0;
            }
            options->retention_hf_lo_hz = lo;
            options->retention_hf_hi_hz = hi;
            continue;
        }
        if (strcmp(argument, "--front-bass") == 0) {
            char *end = NULL;
            if (++index >= argc) { fprintf(stderr, "error: --front-bass requires dB\n"); return 0; }
            options->front_bass_db = strtod(argv[index], &end);
            if (!end || *end || options->front_bass_db > 0.0 || options->front_bass_db < -12.0) {
                fprintf(stderr, "error: --front-bass must be -12.0..0 dB\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argument, "--lfe-gain") == 0) {
            char *end = NULL;
            if (++index >= argc) { fprintf(stderr, "error: --lfe-gain requires dB\n"); return 0; }
            options->lfe_gain_db = strtod(argv[index], &end);
            if (!end || *end || options->lfe_gain_db > 12.0 || options->lfe_gain_db < -24.0) {
                fprintf(stderr, "error: --lfe-gain must be -24.0..12.0 dB re the 4096 assist\n");
                return 0;
            }
            continue;
        }
        if (strncmp(argument, "--surround=", 11) == 0) {
            if (!parse_gain(argument + 11, &options->surround_gain)) {
                fprintf(stderr,
                        "error: --surround must be between 0.0 and %g\n", RENDER_SURROUND_MAX);
                return 0;
            }
            continue;
        }
        if (argument[0] == '-') {
            fprintf(stderr, "error: unknown option: %s\n", argument);
            return 0;
        }
        if (!options->input_path)
            options->input_path = argument;
        else if (!options->output_path)
            options->output_path = argument;
        else {
            fprintf(stderr, "error: unexpected argument: %s\n", argument);
            return 0;
        }
    }

    if (!options->input_path || !options->output_path) {
        usage(argv[0]);
        return 0;
    }
    return 1;
}

static int paths_are_same_file(const char *left, const char *right)
{
    struct stat left_stat;
    struct stat right_stat;

    if (strcmp(left, right) == 0) return 1;
    if (stat(left, &left_stat) != 0 || stat(right, &right_stat) != 0) return 0;
    return left_stat.st_dev == right_stat.st_dev &&
           left_stat.st_ino == right_stat.st_ino;
}

static void make_upmix_config(const struct options *options,
                              struct spa_stereo_upmix_config *config)
{
    spa_stereo_upmix_default_config(config);
    config->backend = options->backend;
    config->rear_gain_q15 =
        (uint32_t)(options->surround_gain * 32768.0 + 0.5);
    config->front_bass_db10 = (int32_t)lround(options->front_bass_db * 10.0);
    config->front_retention_q15 = (uint32_t)lround(options->front_retention * 32768.0);
    config->bed_delays = options->bed_delays ? 1u : 0u;
    config->bed_attack = (uint32_t)options->bed_attack;
    config->attack_collapse_q15 = (uint32_t)lround(options->attack_collapse * 32768.0);
    config->retention_hf_lo_hz = (uint32_t)options->retention_hf_lo_hz;
    config->retention_hf_hi_hz = (uint32_t)options->retention_hf_hi_hz;
    config->rear_body_db10 = (int32_t)lround(options->rear_body_db * 10.0);
    config->vocal_gate_q15 = (uint32_t)lround(options->vocal_gate * 32768.0);
    config->front_treble_db10 = (int32_t)lround(options->front_treble_db * 10.0);
    config->rear_air_q15 = options->rear_air_db > -100.0
        ? (uint32_t)lround(32768.0 * pow(10.0, options->rear_air_db / 20.0)) : 0u;
    config->lfe_gain_q15 = (uint32_t)lround(
        SPA_STEREO_UPMIX_LFE_Q15_DEFAULT * pow(10.0, options->lfe_gain_db / 20.0));
    if (config->lfe_gain_q15 < 1) config->lfe_gain_q15 = 1;
    if (config->lfe_gain_q15 > SPA_STEREO_UPMIX_LFE_Q15_MAX) config->lfe_gain_q15 = SPA_STEREO_UPMIX_LFE_Q15_MAX;
    spa_stereo_upmix_config_set_lfe(
        config, options->backend != SPA_STEREO_UPMIX_OFF &&
                options->lfe_mode == CLI_LFE_ASSIST);

    /* OFF is the bit-exact compatibility path.  Make the public config match
     * the module's effective OFF behaviour so diagnostics cannot imply a trim
     * or synthesized channel that is not actually rendered. */
    if (config->backend == SPA_STEREO_UPMIX_OFF) {
        config->front_gain_q15 = 32768u;
        config->rear_gain_q15 = 0;
        config->lfe_gain_q15 = 0;
        config->lfe_enabled = 0;
    }
}

#ifdef SPA_HSSR_TEST_MASK
static float *g_protect_mask;
static uint32_t g_protect_mask_controls;
static float g_mask_drive;

static int load_protect_mask(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char header[16];
    uint32_t controls;
    uint32_t bins;
    size_t count;

    if (!f) { fprintf(stderr, "error: cannot open %s\n", path); return 0; }
    if (fread(header, 1, 16, f) != 16 || memcmp(header, "HSSRMSK0", 8) != 0) {
        fprintf(stderr, "error: %s is not an HSSRMSK0 file\n", path); fclose(f); return 0;
    }
    controls = load_u32le(header + 8);
    bins = load_u32le(header + 12);
    if (bins != SPA_HSSR_BINS || controls == 0) {
        fprintf(stderr, "error: protect mask wants %u bins, %u controls\n", (unsigned)bins, (unsigned)controls);
        fclose(f); return 0;
    }
    count = (size_t)controls * SPA_HSSR_BINS;
    g_protect_mask = (float *)malloc(count * sizeof(float));
    if (!g_protect_mask || fread(g_protect_mask, sizeof(float), count, f) != count) {
        fprintf(stderr, "error: short protect mask\n"); fclose(f); return 0;
    }
    fclose(f);
    g_protect_mask_controls = controls;
    fprintf(stderr, "protect mask: %u controls x %u bins from %s\n", (unsigned)controls, (unsigned)bins, path);
    return 1;
}
#endif

static int render(FILE *input_file, FILE *output_file, uint32_t total_frames,
                  const struct spa_stereo_upmix_config *config)
{
    /* Keep the complete FFT/WOLA state out of the host thread stack. */
    static struct spa_stereo_upmix state;
    uint8_t input_bytes[SPA_STEREO_UPMIX_MAX_FRAMES *
                        WAV_INPUT_BLOCK_ALIGN];
    uint8_t output_bytes[SPA_STEREO_UPMIX_MAX_FRAMES *
                         WAV_OUTPUT_BLOCK_ALIGN];
    int16_t pcm2[SPA_STEREO_UPMIX_MAX_FRAMES * WAV_INPUT_CHANNELS];
    int16_t pcm6[SPA_STEREO_UPMIX_MAX_FRAMES * WAV_OUTPUT_CHANNELS];
    uint32_t remaining = total_frames;
    int status;


    status = spa_stereo_upmix_init(&state, config);
    if (status != SPA_STEREO_UPMIX_OK) {
        fprintf(stderr, "error: upmixer initialisation failed (%d)\n", status);
        return 0;
    }
#ifdef SPA_HSSR_TEST_MASK
    state._runtime._hssr.test_mask = g_protect_mask;
    state._runtime._hssr.test_mask_controls = g_protect_mask_controls;
    state._runtime._hssr.test_mask_drive = g_mask_drive;
#endif
    if (!write_wav_header(output_file, total_frames)) {
        fprintf(stderr, "error: cannot write output WAV header\n");
        return 0;
    }

    memset(input_bytes, 0, sizeof(input_bytes));
    while (remaining != 0) {
        size_t frames = remaining > SPA_STEREO_UPMIX_MAX_FRAMES
                            ? SPA_STEREO_UPMIX_MAX_FRAMES
                            : (size_t)remaining;
        size_t input_sample_count = frames * WAV_INPUT_CHANNELS;
        size_t output_sample_count = frames * WAV_OUTPUT_CHANNELS;
        size_t sample;

        if (!read_exact(input_file, input_bytes,
                        frames * WAV_INPUT_BLOCK_ALIGN)) {
            fprintf(stderr, "error: WAV data is truncated\n");
            return 0;
        }
        for (sample = 0; sample < input_sample_count; sample++)
            pcm2[sample] = (int16_t)load_u16le(input_bytes + sample * 2);

        status = spa_stereo_upmix_process_s16(&state, pcm2, frames, pcm6, frames);
        if (status != SPA_STEREO_UPMIX_OK) {
            fprintf(stderr, "error: upmixer failed (%d)\n", status);
            return 0;
        }

        for (sample = 0; sample < output_sample_count; sample++)
            store_u16le(output_bytes + sample * 2, (uint16_t)pcm6[sample]);
        if (!write_exact(output_file, output_bytes,
                         frames * WAV_OUTPUT_BLOCK_ALIGN)) {
            fprintf(stderr, "error: cannot write output WAV samples\n");
            return 0;
        }
        remaining -= (uint32_t)frames;
    }

    return 1;
}

int main(int argc, char **argv)
{
    struct options options;
    struct wav_input input;
    struct spa_stereo_upmix_config config;
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    uint32_t frames = 0;
    int success = 0;
    int output_created = 0;
    const char *mode_name;
    const char *lfe_name;
    uint32_t effective_lfe_gain_q15;
    struct spa_hssr_model hssr_model;

    memset(&hssr_model, 0, sizeof(hssr_model));
    if (!parse_options(argc, argv, &options)) return 2;
    make_upmix_config(&options, &config);
#ifdef SPA_HSSR_TEST_MASK
    if (options.protect_mask_path && !load_protect_mask(options.protect_mask_path)) return 2;
    g_mask_drive = (float)options.mask_drive;
#else
    if (options.protect_mask_path) {
        fprintf(stderr, "error: --protect-mask needs a build with -DSPA_HSSR_TEST_MASK\n");
        return 2;
    }
#endif
    if (options.backend == SPA_STEREO_UPMIX_HSSR_UNIVERSAL) {
        int model_status;

        if (!options.model_path) {
            fprintf(stderr, "error: selected HSSR mode requires --model HSSR.bin\n");
            return 2;
        }
        model_status = spa_hssr_model_load_file(
            &hssr_model, options.model_path);
        if (model_status != SPA_HSSR_OK) {
            fprintf(stderr, "error: cannot load HSSR model %s (status=%d)\n",
                    options.model_path, model_status);
            return 2;
        }
        spa_stereo_upmix_config_set_hssr_model(&config, &hssr_model);
        if (options.guard_model_path) {
            static struct spa_hssr_model hssr_guard;
            if (spa_hssr_model_load_file(&hssr_guard, options.guard_model_path) != SPA_HSSR_OK) {
                fprintf(stderr, "error: cannot load guard model %s\n", options.guard_model_path);
                return 2;
            }
            spa_stereo_upmix_config_set_hssr_guard_model(&config, &hssr_guard);
        }
    }
    if (paths_are_same_file(options.input_path, options.output_path)) {
        fprintf(stderr, "error: input and output resolve to the same file\n");
        return 2;
    }

    input_file = fopen(options.input_path, "rb");
    if (!input_file) {
        fprintf(stderr, "error: cannot open %s: %s\n",
                options.input_path, strerror(errno));
        goto done;
    }
    if (!parse_wav(input_file, &input) || !validate_wav(&input, &frames))
        goto done;

    output_file = fopen(options.output_path, "wb");
    if (!output_file) {
        fprintf(stderr, "error: cannot create %s: %s\n",
                options.output_path, strerror(errno));
        goto done;
    }
    output_created = 1;
    if (!render(input_file, output_file, frames, &config)) goto done;
    if (fclose(output_file) != 0) {
        output_file = NULL;
        fprintf(stderr, "error: cannot finish %s: %s\n",
                options.output_path, strerror(errno));
        goto done;
    }
    output_file = NULL;
    success = 1;

done:
    if (output_file && fclose(output_file) != 0)
        fprintf(stderr, "warning: failed to close partial output\n");
    if (input_file && fclose(input_file) != 0)
        fprintf(stderr, "warning: failed to close input\n");
    if (!success) {
        if (output_created) (void)remove(options.output_path);
        spa_hssr_model_unload(&hssr_model);
        return 1;
    }

    mode_name = backend_mode_name(options.backend);
    fprintf(stderr, "front-retention=%.5f (q15=%u; independent of rear gain)\n",
            (double)config.front_retention_q15 / 32768.0,
            (unsigned)config.front_retention_q15);
    fprintf(stderr, "bed-delays=%u bed-attack=%u attack-collapse=%.5f "
            "retention-hf=%u:%u Hz\n",
            (unsigned)config.bed_delays, (unsigned)config.bed_attack,
            (double)config.attack_collapse_q15 / 32768.0,
            (unsigned)config.retention_hf_lo_hz,
            (unsigned)config.retention_hf_hi_hz);
    lfe_name = config.lfe_enabled ? "assist" : "off";
    effective_lfe_gain_q15 = config.lfe_enabled ? config.lfe_gain_q15 : 0;
    fprintf(stderr,
            "rendered %u frames (%.3f s): PCM2 -> PCM6 "
            "L,R,C,LFE,Ls,Rs\n"
            "mode=%s surround=%.5f (q15=%u) "
            "front=%.5f (q15=%u) lfe=%s gain=%.5f (q15=%u)%s\n%s\n",
            (unsigned)frames, (double)frames / WAV_SAMPLE_RATE,
            mode_name,
            (double)config.rear_gain_q15 / 32768.0,
            (unsigned)config.rear_gain_q15,
            (double)config.front_gain_q15 / 32768.0,
            (unsigned)config.front_gain_q15,
            lfe_name,
            (double)effective_lfe_gain_q15 / 32768.0,
            (unsigned)effective_lfe_gain_q15,
            options.backend == SPA_STEREO_UPMIX_OFF &&
                    options.lfe_mode == CLI_LFE_ASSIST
                ? " (forced off by mode=off)" : "",
            options.output_path);
    spa_hssr_model_unload(&hssr_model);
    return 0;
}
