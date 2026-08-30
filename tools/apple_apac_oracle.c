/*
 * Decode raw Apple Positional Audio Codec packets with macOS AudioToolbox.
 *
 * This is a bench/oracle tool for comparing captured CarPlay APAC with the
 * decoder built into macOS 26.  It is not intended for deployment on the HU.
 */

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    kAudioFormatAPACLocal = 'apac',
    kFramesPerAPACPacket = 1024,
    kMaximumInputSize = 64 * 1024 * 1024,
};

typedef struct {
    const uint8_t *bytes;
    UInt32 size;
    UInt32 channels;
    AudioStreamPacketDescription description;
    int supplied;
} InputPacket;

static void fourcc(UInt32 value, char output[5])
{
    output[0] = (char)(value >> 24);
    output[1] = (char)(value >> 16);
    output[2] = (char)(value >> 8);
    output[3] = (char)value;
    output[4] = '\0';
    for (int index = 0; index < 4; ++index) {
        if ((unsigned char)output[index] < 0x20 ||
            (unsigned char)output[index] > 0x7e) {
            output[index] = '.';
        }
    }
}

static void print_status(const char *operation, OSStatus status)
{
    char text[5];
    fourcc((UInt32)status, text);
    fprintf(stderr, "%s: status=%" PRId32 " (0x%08" PRIx32 ", '%s')\n",
        operation, (int32_t)status, (uint32_t)status, text);
}

static int read_file(const char *path, uint8_t **data_out, size_t *size_out)
{
    FILE *stream;
    uint8_t *data;
    long length;

    stream = fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) <= 0 ||
        length > kMaximumInputSize || fseek(stream, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: invalid input size\n", path);
        fclose(stream);
        return -1;
    }
    data = malloc((size_t)length);
    if (data == NULL || fread(data, 1, (size_t)length, stream) != (size_t)length) {
        fprintf(stderr, "%s: read failed\n", path);
        free(data);
        fclose(stream);
        return -1;
    }
    fclose(stream);
    *data_out = data;
    *size_out = (size_t)length;
    return 0;
}

static void print_asbd(const AudioStreamBasicDescription *format)
{
    char format_id[5];
    fourcc(format->mFormatID, format_id);
    printf("format='%s' rate=%.0f flags=0x%08" PRIx32
           " bytes/packet=%" PRIu32 " frames/packet=%" PRIu32
           " bytes/frame=%" PRIu32 " channels=%" PRIu32
           " bits=%" PRIu32 "\n",
        format_id,
        format->mSampleRate,
        format->mFormatFlags,
        format->mBytesPerPacket,
        format->mFramesPerPacket,
        format->mBytesPerFrame,
        format->mChannelsPerFrame,
        format->mBitsPerChannel);
}

static int probe_decoder(void)
{
    AudioStreamBasicDescription apac = {0};
    UInt32 size = 0;
    UInt32 *identifiers = NULL;
    OSStatus status;
    int found = 0;

    status = AudioFormatGetPropertyInfo(
        kAudioFormatProperty_DecodeFormatIDs, 0, NULL, &size);
    if (status != noErr) {
        print_status("DecodeFormatIDs/info", status);
        return 1;
    }
    identifiers = malloc(size);
    if (identifiers == NULL) {
        fputs("decoder-list allocation failed\n", stderr);
        return 1;
    }
    status = AudioFormatGetProperty(
        kAudioFormatProperty_DecodeFormatIDs, 0, NULL, &size, identifiers);
    if (status != noErr) {
        print_status("DecodeFormatIDs", status);
        free(identifiers);
        return 1;
    }
    for (UInt32 index = 0; index < size / sizeof(*identifiers); ++index) {
        if (identifiers[index] == kAudioFormatAPACLocal) {
            found = 1;
            break;
        }
    }
    printf("macOS AudioToolbox APAC decoder: %s (decode formats=%zu)\n",
        found ? "AVAILABLE" : "NOT FOUND",
        (size_t)(size / sizeof(*identifiers)));
    free(identifiers);
    if (!found) {
        return 1;
    }

    size = 0;
    status = AudioFormatGetPropertyInfo(
        kAudioFormatProperty_EncodeFormatIDs, 0, NULL, &size);
    if (status == noErr && size >= sizeof(UInt32)) {
        int encoder_found = 0;
        identifiers = malloc(size);
        if (identifiers == NULL) {
            fputs("encoder-list allocation failed\n", stderr);
            return 1;
        }
        status = AudioFormatGetProperty(
            kAudioFormatProperty_EncodeFormatIDs, 0, NULL, &size, identifiers);
        if (status == noErr) {
            for (UInt32 index = 0; index < size / sizeof(*identifiers); ++index) {
                encoder_found |= identifiers[index] == kAudioFormatAPACLocal;
            }
            printf("macOS AudioToolbox APAC encoder: %s\n",
                encoder_found ? "AVAILABLE" : "NOT FOUND");
        } else {
            print_status("EncodeFormatIDs", status);
        }
        free(identifiers);
        identifiers = NULL;
    }

    {
        UInt32 format_id = kAudioFormatAPACLocal;
        UInt32 decoder_size = 0;
        status = AudioFormatGetPropertyInfo(
            kAudioFormatProperty_Decoders,
            sizeof(format_id),
            &format_id,
            &decoder_size);
        if (status == noErr && decoder_size >= sizeof(AudioClassDescription)) {
            AudioClassDescription *decoders = malloc(decoder_size);
            if (decoders == NULL) {
                fputs("APAC decoder-description allocation failed\n", stderr);
                return 1;
            }
            status = AudioFormatGetProperty(
                kAudioFormatProperty_Decoders,
                sizeof(format_id),
                &format_id,
                &decoder_size,
                decoders);
            if (status == noErr) {
                for (UInt32 index = 0;
                     index < decoder_size / sizeof(*decoders);
                     ++index) {
                    char type[5], subtype[5], manufacturer[5];
                    fourcc(decoders[index].mType, type);
                    fourcc(decoders[index].mSubType, subtype);
                    fourcc(decoders[index].mManufacturer, manufacturer);
                    printf("decoder[%" PRIu32 "]: type='%s' subtype='%s' manufacturer='%s'\n",
                        index, type, subtype, manufacturer);
                }
            } else {
                print_status("APAC Decoders", status);
            }
            free(decoders);
        } else if (status != noErr) {
            print_status("APAC Decoders/info", status);
        }
    }

    apac.mFormatID = kAudioFormatAPACLocal;
    size = 0;
    status = AudioFormatGetPropertyInfo(
        kAudioFormatProperty_AvailableDecodeNumberChannels,
        sizeof(apac),
        &apac,
        &size);
    if (status == noErr && size >= sizeof(UInt32)) {
        UInt32 *channel_counts = malloc(size);
        if (channel_counts == NULL) {
            fputs("channel-list allocation failed\n", stderr);
            return 1;
        }
        status = AudioFormatGetProperty(
            kAudioFormatProperty_AvailableDecodeNumberChannels,
            sizeof(apac),
            &apac,
            &size,
            channel_counts);
        if (status == noErr) {
            UInt32 count = size / sizeof(*channel_counts);
            int sequential = count > 1 && channel_counts[0] != UINT32_MAX;
            for (UInt32 index = 1; index < count && sequential; ++index) {
                sequential = channel_counts[index] == channel_counts[index - 1] + 1;
            }
            if (sequential) {
                printf("reported APAC decode channel counts: %" PRIu32 "-%" PRIu32 "\n",
                    channel_counts[0], channel_counts[count - 1]);
            } else {
                fputs("reported APAC decode channel counts:", stdout);
                for (UInt32 index = 0; index < count; ++index) {
                    if (channel_counts[index] == UINT32_MAX) {
                        fputs(" any", stdout);
                    } else {
                        printf(" %" PRIu32, channel_counts[index]);
                    }
                }
                fputc('\n', stdout);
            }
        } else {
            print_status("AvailableDecodeNumberChannels", status);
        }
        free(channel_counts);
    } else if (status != noErr) {
        print_status("AvailableDecodeNumberChannels/info", status);
    }
    return 0;
}

static int describe_cookie(
    const uint8_t *cookie,
    size_t cookie_size,
    AudioStreamBasicDescription *source)
{
    UInt32 output_size = sizeof(*source);
    OSStatus status;

    memset(source, 0, sizeof(*source));
    source->mFormatID = kAudioFormatAPACLocal;
    status = AudioFormatGetProperty(
        kAudioFormatProperty_FormatInfo,
        (UInt32)cookie_size,
        cookie,
        &output_size,
        source);
    if (status != noErr) {
        print_status("APAC FormatInfo(cookie)", status);
        return -1;
    }
    print_asbd(source);
    if (source->mSampleRate <= 0 || source->mChannelsPerFrame == 0 ||
        source->mChannelsPerFrame > 32) {
        fputs("APAC cookie returned invalid stream metadata\n", stderr);
        return -1;
    }
    return 0;
}

static OSStatus supply_packet(
    AudioConverterRef converter,
    UInt32 *packet_count,
    AudioBufferList *data,
    AudioStreamPacketDescription **descriptions,
    void *user_data)
{
    InputPacket *packet = user_data;
    (void)converter;

    if (packet->supplied || *packet_count == 0) {
        *packet_count = 0;
        return noErr;
    }
    packet->description.mStartOffset = 0;
    packet->description.mVariableFramesInPacket = kFramesPerAPACPacket;
    packet->description.mDataByteSize = packet->size;
    data->mNumberBuffers = 1;
    data->mBuffers[0].mNumberChannels = packet->channels;
    data->mBuffers[0].mDataByteSize = packet->size;
    data->mBuffers[0].mData = (void *)packet->bytes;
    if (descriptions != NULL) {
        *descriptions = &packet->description;
    }
    *packet_count = 1;
    packet->supplied = 1;
    return noErr;
}

static int decode_frames(
    const uint8_t *cookie,
    size_t cookie_size,
    const AudioStreamBasicDescription *source,
    const char *output_path,
    int frame_count,
    char **frame_paths)
{
    AudioStreamBasicDescription destination = {0};
    AudioConverterRef converter = NULL;
    FILE *output = NULL;
    uint8_t *pcm = NULL;
    size_t pcm_capacity;
    uint64_t total_frames = 0;
    OSStatus status;
    int result = 1;

    destination.mSampleRate = source->mSampleRate;
    destination.mFormatID = kAudioFormatLinearPCM;
    destination.mFormatFlags =
        kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    destination.mBytesPerPacket = source->mChannelsPerFrame * 2;
    destination.mFramesPerPacket = 1;
    destination.mBytesPerFrame = source->mChannelsPerFrame * 2;
    destination.mChannelsPerFrame = source->mChannelsPerFrame;
    destination.mBitsPerChannel = 16;

    status = AudioConverterNew(source, &destination, &converter);
    if (status != noErr) {
        print_status("AudioConverterNew(APAC->PCM)", status);
        goto cleanup;
    }
    status = AudioConverterSetProperty(
        converter,
        kAudioConverterDecompressionMagicCookie,
        (UInt32)cookie_size,
        cookie);
    if (status != noErr) {
        print_status("AudioConverterSetProperty(dmgc)", status);
        goto cleanup;
    }

    pcm_capacity = (size_t)destination.mBytesPerFrame * kFramesPerAPACPacket;
    pcm = malloc(pcm_capacity);
    output = fopen(output_path, "wb");
    if (pcm == NULL || output == NULL) {
        fprintf(stderr, "%s: output allocation/open failed: %s\n",
            output_path, strerror(errno));
        goto cleanup;
    }

    for (int index = 0; index < frame_count; ++index) {
        uint8_t *compressed = NULL;
        size_t compressed_size = 0;
        UInt32 output_packets = kFramesPerAPACPacket;
        struct {
            UInt32 mNumberBuffers;
            AudioBuffer mBuffers[1];
        } output_list = {
            .mNumberBuffers = 1,
            .mBuffers = {{
                .mNumberChannels = destination.mChannelsPerFrame,
                .mDataByteSize = (UInt32)pcm_capacity,
                .mData = pcm,
            }},
        };
        InputPacket input;

        if (read_file(frame_paths[index], &compressed, &compressed_size) != 0) {
            goto cleanup;
        }
        if (compressed_size > UINT32_MAX) {
            free(compressed);
            fputs("compressed APAC frame is too large\n", stderr);
            goto cleanup;
        }
        memset(&input, 0, sizeof(input));
        input.bytes = compressed;
        input.size = (UInt32)compressed_size;
        input.channels = source->mChannelsPerFrame;
        status = AudioConverterFillComplexBuffer(
            converter,
            supply_packet,
            &input,
            &output_packets,
            (AudioBufferList *)&output_list,
            NULL);
        free(compressed);
        if (status != noErr) {
            fprintf(stderr, "frame %d (%s): ", index, frame_paths[index]);
            print_status("AudioConverterFillComplexBuffer", status);
            goto cleanup;
        }
        if (fwrite(pcm, 1, output_list.mBuffers[0].mDataByteSize, output) !=
            output_list.mBuffers[0].mDataByteSize) {
            fputs("PCM output write failed\n", stderr);
            goto cleanup;
        }
        total_frames += output_packets;
        printf("frame[%d] compressed=%zu pcm_frames=%" PRIu32
               " pcm_bytes=%" PRIu32 "\n",
            index,
            compressed_size,
            output_packets,
            output_list.mBuffers[0].mDataByteSize);
    }
    printf("wrote raw s16le interleaved PCM: %s (%" PRIu64
           " frames, %" PRIu32 " channels, %.0f Hz)\n",
        output_path, total_frames, destination.mChannelsPerFrame,
        destination.mSampleRate);
    result = 0;

cleanup:
    if (output != NULL && fclose(output) != 0) {
        result = 1;
    }
    free(pcm);
    if (converter != NULL) {
        AudioConverterDispose(converter);
    }
    return result;
}

static void usage(const char *program)
{
    fprintf(stderr,
        "usage: %s --probe\n"
        "       %s --inspect COOKIE\n"
        "       %s COOKIE PCM_OUT APAC_FRAME [APAC_FRAME ...]\n",
        program, program, program);
}

int main(int argc, char **argv)
{
    uint8_t *cookie = NULL;
    size_t cookie_size = 0;
    AudioStreamBasicDescription source;
    int result = 1;

    if (argc == 2 && strcmp(argv[1], "--probe") == 0) {
        return probe_decoder();
    }
    if (argc != 3 && argc < 4) {
        usage(argv[0]);
        return 2;
    }
    if (read_file(argc == 3 ? argv[2] : argv[1], &cookie, &cookie_size) != 0) {
        return 1;
    }
    if (describe_cookie(cookie, cookie_size, &source) != 0) {
        goto cleanup;
    }
    if (argc == 3) {
        if (strcmp(argv[1], "--inspect") != 0) {
            usage(argv[0]);
            result = 2;
            goto cleanup;
        }
        result = 0;
        goto cleanup;
    }
    result = decode_frames(
        cookie, cookie_size, &source, argv[2], argc - 3, &argv[3]);

cleanup:
    free(cookie);
    return result;
}
