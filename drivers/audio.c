/**
 * @file    audio.c
 * @author  shadcy
 * @brief   PL041 AACI (Advanced Audio CODEC Interface) Driver & Sound Subsystem
 *
 * Implements hardware control for the ARM PrimeCell PL041 AACI controller,
 * LM4549 AC'97 codec configuration, non-blocking PCM ring-buffer playback,
 * frequency tone synthesis, retro sound effects, and WAV parsing.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "audio.h"
#include "console.h"
#include "heap.h"
#include "string.h"
#include "fatfs/ff.h"

/* ============================================================================
 * Configuration & Internal State
 * ============================================================================ */
#define AUDIO_RING_BUFFER_SIZE  (64 * 1024)   /* 64 KB circular PCM buffer */
#define DEFAULT_SAMPLE_RATE     22050
#define DEFAULT_CHANNELS        2
#define DEFAULT_BITS            16

static uint8_t *g_audio_buf = NULL;
static volatile uint32_t g_ring_head = 0;
static volatile uint32_t g_ring_tail = 0;
static volatile int g_audio_initialized = 0;
static volatile uint32_t g_current_rate = DEFAULT_SAMPLE_RATE;
static uint8_t g_current_volume = 90;

/* ============================================================================
 * AC'97 Codec Low-Level Register Access (via AACI Slot 1 & Slot 2)
 * ============================================================================ */

static void ac97_write(uint8_t reg, uint16_t val)
{
    /* Wait for Slot 1 and Slot 2 TX not busy */
    int timeout = 10000;
    while ((AACI_SLFR & (AACI_SLFR_1TXB | AACI_SLFR_2TXB)) && --timeout > 0);

    /* Write Slot 1 (register address) and Slot 2 (16-bit register value) */
    AACI_SL1TX = (uint32_t)(reg & 0x7F) << 12;
    AACI_SL2TX = (uint32_t)val << 4;
}

static uint16_t ac97_read(uint8_t reg)
{
    int timeout = 10000;
    while ((AACI_SLFR & AACI_SLFR_1TXB) && --timeout > 0);

    /* Slot 1 read command: Bit 19 is Read/Write bit (1 = Read) */
    AACI_SL1TX = ((uint32_t)(reg & 0x7F) << 12) | (1U << 19);

    timeout = 10000;
    while (!(AACI_SLFR & AACI_SLFR_2RXV) && --timeout > 0);

    if (AACI_SLFR & AACI_SLFR_2RXV) {
        return (uint16_t)(AACI_SL2RX >> 4);
    }
    return 0;
}

/* ============================================================================
 * Hardware Initialization & Volume Controls
 * ============================================================================ */

int audio_set_sample_rate(uint32_t rate)
{
    if (rate == 0) rate = DEFAULT_SAMPLE_RATE;
    g_current_rate = rate;

    /* Enable Variable Rate Audio (VRA) in AC'97 Extended Audio Status register */
    ac97_write(AC97_REG_EXT_AUDIO_STAT, 0x0001);

    /* Set front DAC sample rate */
    ac97_write(AC97_REG_PCM_FRONT_DAC, (uint16_t)(rate & 0xFFFF));
    return 0;
}

void audio_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    g_current_volume = percent;

    /* AC'97 Volume attenuation: 0 = 0dB (loudest), 31 = -46.5dB (quietest), 0x8000 = mute */
    if (percent == 0) {
        ac97_write(AC97_REG_MASTER_VOL, 0x8000);
        ac97_write(AC97_REG_PCMOUT_VOL, 0x8000);
        return;
    }

    uint8_t atten = (uint8_t)(31 - (percent * 31 / 100));
    uint16_t vol_val = ((uint16_t)atten << 8) | (uint16_t)atten;

    ac97_write(AC97_REG_MASTER_VOL, vol_val);
    ac97_write(AC97_REG_PCMOUT_VOL, vol_val);
    ac97_write(AC97_REG_HEADPHONE_VOL, vol_val);
}

int audio_init(void)
{
    if (g_audio_initialized) return 0;

    /* Allocate ring buffer */
    if (!g_audio_buf) {
        g_audio_buf = (uint8_t *)kmalloc(AUDIO_RING_BUFFER_SIZE);
        if (!g_audio_buf) {
            kputs("AUDIO: Failed to allocate ring buffer\n");
            return -1;
        }
    }
    g_ring_head = 0;
    g_ring_tail = 0;

    /* Reset and enable PL041 AACI Controller */
    AACI_RESET = 0;
    AACI_MAINCR = AACI_MAINCR_IE;

    /* Reset AC'97 Codec */
    ac97_write(AC97_REG_RESET, 0x0000);

    /* Un-mute and configure default master volume */
    audio_set_volume(90);

    /* Set default sample rate (22.05 kHz) */
    audio_set_sample_rate(DEFAULT_SAMPLE_RATE);

    /* Configure Transmit Channel 1:
     * - TXEN: Transmit enable
     * - COMPACT: Compact stereo (two 16-bit samples per 32-bit word)
     * - TSLOT3 | TSLOT4: Assign to left/right slots
     * - FEN: Enable FIFO */
    AACI_TXCR1 = AACI_TXCR_TXEN | AACI_TXCR_COMPACT | AACI_TXCR_TSLOT3 | AACI_TXCR_TSLOT4 | AACI_TXCR_FEN;

    /* Query codec vendor ID */
    uint16_t vid1 = ac97_read(AC97_REG_VENDOR_ID1);
    uint16_t vid2 = ac97_read(AC97_REG_VENDOR_ID2);
    (void)vid1; (void)vid2;

    g_audio_initialized = 1;
    kputs("AUDIO: ARM PrimeCell PL041 AACI & AC'97 driver online (Stereo 16-bit, 22.05 kHz).\n");
    return 0;
}

/* ============================================================================
 * Ring Buffer & FIFO Stream Pump
 * ============================================================================ */

void audio_poll(void)
{
    if (!g_audio_initialized || !g_audio_buf) return;

    /* Feed hardware FIFO while TX FIFO is not full and buffer has data */
    int max_words = 256; /* PL041 FIFO depth per poll */
    while (max_words-- > 0 && g_ring_head != g_ring_tail) {
        if (AACI_SR1 & AACI_SR_TXFF) {
            break; /* Hardware FIFO is full */
        }

        /* Extract 4 bytes (32-bit word = Left 16-bit + Right 16-bit) */
        uint32_t tail = g_ring_tail;
        uint32_t avail = (g_ring_head >= tail) ? (g_ring_head - tail) : (AUDIO_RING_BUFFER_SIZE - tail + g_ring_head);
        if (avail < 4) break;

        uint32_t sample_word = 0;
        sample_word |= (uint32_t)g_audio_buf[tail];
        sample_word |= ((uint32_t)g_audio_buf[(tail + 1) % AUDIO_RING_BUFFER_SIZE]) << 8;
        sample_word |= ((uint32_t)g_audio_buf[(tail + 2) % AUDIO_RING_BUFFER_SIZE]) << 16;
        sample_word |= ((uint32_t)g_audio_buf[(tail + 3) % AUDIO_RING_BUFFER_SIZE]) << 24;

        AACI_DR1 = sample_word;
        g_ring_tail = (tail + 4) % AUDIO_RING_BUFFER_SIZE;
    }
}

int32_t audio_write_pcm(const void *data, size_t num_bytes)
{
    if (!g_audio_initialized) audio_init();
    if (!data || num_bytes == 0 || !g_audio_buf) return 0;

    const uint8_t *src = (const uint8_t *)data;
    size_t written = 0;

    while (written < num_bytes) {
        uint32_t next_head = (g_ring_head + 1) % AUDIO_RING_BUFFER_SIZE;
        if (next_head == g_ring_tail) {
            /* Ring buffer full: try to drain to hardware */
            audio_poll();
            if (next_head == g_ring_tail) {
                break; /* Still full */
            }
        }
        g_audio_buf[g_ring_head] = src[written++];
        g_ring_head = next_head;
    }

    audio_poll();
    return (int32_t)written;
}

int audio_play_pcm(const void *samples, size_t num_bytes, uint32_t sample_rate, uint8_t channels, uint8_t bits)
{
    if (!samples || num_bytes == 0) return -1;
    if (!g_audio_initialized) audio_init();

    audio_set_sample_rate(sample_rate);

    /* If standard 16-bit stereo, write directly */
    if (channels == 2 && bits == 16) {
        audio_write_pcm(samples, num_bytes);
        return 0;
    }

    /* Convert mono or 8-bit to 16-bit stereo for PL041 compact mode */
    if (channels == 1 && bits == 16) {
        /* Mono 16-bit -> Stereo 16-bit */
        const int16_t *src = (const int16_t *)samples;
        size_t count = num_bytes / 2;
        int16_t stereo_buf[128];
        size_t idx = 0;
        while (idx < count) {
            size_t chunk = count - idx;
            if (chunk > 64) chunk = 64;
            for (size_t i = 0; i < chunk; i++) {
                stereo_buf[i * 2]     = src[idx + i];
                stereo_buf[i * 2 + 1] = src[idx + i];
            }
            audio_write_pcm(stereo_buf, chunk * 4);
            idx += chunk;
        }
        return 0;
    }

    if (channels == 1 && bits == 8) {
        /* Mono 8-bit unsigned -> Stereo 16-bit signed */
        const uint8_t *src = (const uint8_t *)samples;
        int16_t stereo_buf[128];
        size_t idx = 0;
        while (idx < num_bytes) {
            size_t chunk = num_bytes - idx;
            if (chunk > 64) chunk = 64;
            for (size_t i = 0; i < chunk; i++) {
                int16_t s16 = (int16_t)(((int32_t)src[idx + i] - 128) << 8);
                stereo_buf[i * 2]     = s16;
                stereo_buf[i * 2 + 1] = s16;
            }
            audio_write_pcm(stereo_buf, chunk * 4);
            idx += chunk;
        }
        return 0;
    }

    return audio_write_pcm(samples, num_bytes);
}

void audio_stop(void)
{
    g_ring_head = 0;
    g_ring_tail = 0;
}

int audio_is_playing(void)
{
    return (g_ring_head != g_ring_tail) || (AACI_SR1 & AACI_SR_TXB);
}

/* ============================================================================
 * Tone & Sound Effect Synthesis
 * ============================================================================ */

int audio_play_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume)
{
    if (freq_hz == 0 || duration_ms == 0) return 0;
    if (!g_audio_initialized) audio_init();

    uint32_t rate = 22050;
    audio_set_sample_rate(rate);

    uint32_t total_samples = (rate * duration_ms) / 1000;
    uint32_t period = rate / freq_hz;
    if (period < 2) period = 2;

    int16_t amplitude = (int16_t)(volume * 300); /* Max ~30,000 */
    int16_t buf[256]; /* 128 stereo pairs */

    uint32_t samples_generated = 0;
    while (samples_generated < total_samples) {
        size_t chunk = total_samples - samples_generated;
        if (chunk > 128) chunk = 128;

        for (size_t i = 0; i < chunk; i++) {
            uint32_t pos = (samples_generated + i) % period;
            /* Square wave */
            int16_t sample = (pos < period / 2) ? amplitude : -amplitude;
            buf[i * 2]     = sample;
            buf[i * 2 + 1] = sample;
        }

        audio_write_pcm(buf, chunk * 4);
        samples_generated += chunk;
    }
    return 0;
}

int audio_beep(uint32_t freq_hz, uint32_t duration_ms)
{
    return audio_play_tone(freq_hz, duration_ms, 80);
}

int audio_play_fx(audio_fx_t fx)
{
    if (!g_audio_initialized) audio_init();

    switch (fx) {
        case AUDIO_FX_CLICK:
            audio_play_tone(2400, 15, 60);
            break;

        case AUDIO_FX_POPUP:
            audio_play_tone(880, 40, 70);
            audio_play_tone(1760, 60, 75);
            break;

        case AUDIO_FX_ERROR:
            audio_play_tone(300, 80, 85);
            audio_play_tone(220, 120, 85);
            break;

        case AUDIO_FX_COIN:
            audio_play_tone(988, 70, 80);  /* B5 */
            audio_play_tone(1319, 200, 85); /* E6 */
            break;

        case AUDIO_FX_LASER:
            for (uint32_t f = 1600; f >= 400; f -= 120) {
                audio_play_tone(f, 10, 80);
            }
            break;

        case AUDIO_FX_EXPLODE:
            for (uint32_t f = 300; f >= 60; f -= 20) {
                audio_play_tone(f, 25, 90);
            }
            break;

        case AUDIO_FX_BOOT:
            /* Retro modern startup chord: C5 -> E5 -> G5 -> C6 */
            audio_play_tone(523, 70, 75);
            audio_play_tone(659, 70, 80);
            audio_play_tone(784, 70, 85);
            audio_play_tone(1046, 180, 90);
            break;

        case AUDIO_FX_BEEP:
        default:
            audio_play_tone(880, 80, 80);
            break;
    }
    return 0;
}

/* ============================================================================
 * WAV File Parser & Player
 * ============================================================================ */

typedef struct __attribute__((packed)) {
    char     riff_id[4];     /* "RIFF" */
    uint32_t riff_size;
    char     wave_id[4];     /* "WAVE" */
    char     fmt_id[4];      /* "fmt " */
    uint32_t fmt_size;
    uint16_t audio_format;   /* 1 = PCM */
    uint16_t num_channels;   /* 1 = Mono, 2 = Stereo */
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_header_t;

int audio_play_wav(const char *fat_path)
{
    if (!fat_path) return -1;
    if (!g_audio_initialized) audio_init();

    FIL fil;
    UINT br;
    if (f_open(&fil, fat_path, FA_READ) != FR_OK) {
        kprintf("[AUDIO] Cannot open WAV file: %s\n", fat_path);
        return -1;
    }

    wav_header_t hdr;
    if (f_read(&fil, &hdr, sizeof(wav_header_t), &br) != FR_OK || br < sizeof(wav_header_t)) {
        kprintf("[AUDIO] Error reading WAV header.\n");
        f_close(&fil);
        return -2;
    }

    if (memcmp(hdr.riff_id, "RIFF", 4) != 0 || memcmp(hdr.wave_id, "WAVE", 4) != 0) {
        kprintf("[AUDIO] Not a valid RIFF/WAVE file.\n");
        f_close(&fil);
        return -3;
    }

    /* Find "data" chunk */
    char chunk_id[4];
    uint32_t chunk_size = 0;
    int found_data = 0;

    while (f_read(&fil, chunk_id, 4, &br) == FR_OK && br == 4) {
        if (f_read(&fil, &chunk_size, 4, &br) != FR_OK || br != 4) break;
        if (memcmp(chunk_id, "data", 4) == 0) {
            found_data = 1;
            break;
        }
        f_lseek(&fil, fil.fptr + chunk_size);
    }

    if (!found_data) {
        kprintf("[AUDIO] No data chunk found in WAV.\n");
        f_close(&fil);
        return -4;
    }

    kprintf("[AUDIO] Playing '%s' (%u Hz, %d-ch, %d-bit, %u bytes)...\n",
            fat_path, hdr.sample_rate, hdr.num_channels, hdr.bits_per_sample, chunk_size);

    audio_set_sample_rate(hdr.sample_rate);

    uint8_t buffer[1024];
    uint32_t bytes_remaining = chunk_size;

    while (bytes_remaining > 0) {
        UINT to_read = sizeof(buffer);
        if (to_read > bytes_remaining) to_read = bytes_remaining;

        if (f_read(&fil, buffer, to_read, &br) != FR_OK || br == 0) break;
        audio_play_pcm(buffer, br, hdr.sample_rate, (uint8_t)hdr.num_channels, (uint8_t)hdr.bits_per_sample);
        bytes_remaining -= br;
    }

    f_close(&fil);
    return 0;
}
