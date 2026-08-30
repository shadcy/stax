/**
 * @file    audio.h
 * @author  shadcy
 * @brief   Audio Subsystem & ARM PrimeCell PL041 AACI Audio Driver for STAX OS
 *
 * Provides hardware control for the PL041 Advanced Audio CODEC Interface (AACI)
 * and LM4549 AC'97 audio codec on the ARM VersatilePB architecture.
 * Features PCM audio playback, ring buffering, sound synthesis, and /dev/dsp support.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * PL041 AACI Hardware Register Map (ARM VersatilePB)
 * ============================================================================ */
#define PL041_AACI_BASE         0x10004000UL

/* AACI Registers (Offsets from PL041_AACI_BASE) */
#define AACI_RXCR1              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x000))
#define AACI_TXCR1              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x004))
#define AACI_SR1                (*(volatile uint32_t *)(PL041_AACI_BASE + 0x008))
#define AACI_ISR1               (*(volatile uint32_t *)(PL041_AACI_BASE + 0x00C))
#define AACI_IE1                (*(volatile uint32_t *)(PL041_AACI_BASE + 0x010))
#define AACI_INT1               (*(volatile uint32_t *)(PL041_AACI_BASE + 0x014))

#define AACI_RXCR2              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x018))
#define AACI_TXCR2              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x01C))
#define AACI_SR2                (*(volatile uint32_t *)(PL041_AACI_BASE + 0x020))

#define AACI_SL1RX              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x060))
#define AACI_SL1TX              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x064))
#define AACI_SL2RX              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x068))
#define AACI_SL2TX              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x06C))
#define AACI_SL12RX             (*(volatile uint32_t *)(PL041_AACI_BASE + 0x070))
#define AACI_SL12TX             (*(volatile uint32_t *)(PL041_AACI_BASE + 0x074))
#define AACI_SLFR               (*(volatile uint32_t *)(PL041_AACI_BASE + 0x078))
#define AACI_SLISTAT            (*(volatile uint32_t *)(PL041_AACI_BASE + 0x07C))
#define AACI_SLIEN              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x080))
#define AACI_INTCLR             (*(volatile uint32_t *)(PL041_AACI_BASE + 0x084))
#define AACI_MAINCR             (*(volatile uint32_t *)(PL041_AACI_BASE + 0x088))
#define AACI_RESET              (*(volatile uint32_t *)(PL041_AACI_BASE + 0x08C))
#define AACI_SYNC               (*(volatile uint32_t *)(PL041_AACI_BASE + 0x090))
#define AACI_ALLINTS            (*(volatile uint32_t *)(PL041_AACI_BASE + 0x094))
#define AACI_MAINFR             (*(volatile uint32_t *)(PL041_AACI_BASE + 0x098))

/* FIFO Data Registers (Channel 1-4) */
#define AACI_DR1                (*(volatile uint32_t *)(PL041_AACI_BASE + 0x900))
#define AACI_DR2                (*(volatile uint32_t *)(PL041_AACI_BASE + 0xA00))
#define AACI_DR3                (*(volatile uint32_t *)(PL041_AACI_BASE + 0xB00))
#define AACI_DR4                (*(volatile uint32_t *)(PL041_AACI_BASE + 0xC00))

/* ============================================================================
 * Register Bit Definitions
 * ============================================================================ */
/* TXCR1 bits */
#define AACI_TXCR_TXEN          (1U << 0)   /* Transmit Enable */
#define AACI_TXCR_COMPACT       (1U << 15)  /* Compact 16-bit Stereo in 32-bit word */
#define AACI_TXCR_SIZE_16       (1U << 1)   /* 16-bit sample size */
#define AACI_TXCR_TSLOT3        (1U << 5)   /* Left channel (AC'97 slot 3) */
#define AACI_TXCR_TSLOT4        (1U << 6)   /* Right channel (AC'97 slot 4) */
#define AACI_TXCR_FEN           (1U << 16)  /* FIFO Enable */

/* Status Register bits (SR1) */
#define AACI_SR_TXB             (1U << 0)   /* TX Busy */
#define AACI_SR_TXFF            (1U << 1)   /* TX FIFO Full */
#define AACI_SR_TXHE            (1U << 2)   /* TX FIFO Half Empty */
#define AACI_SR_TXE             (1U << 3)   /* TX FIFO Empty */
#define AACI_SR_RXB             (1U << 4)   /* RX Busy */
#define AACI_SR_RXFF            (1U << 5)   /* RX FIFO Full */
#define AACI_SR_RXHF            (1U << 6)   /* RX FIFO Half Full */
#define AACI_SR_RXNE            (1U << 7)   /* RX FIFO Not Empty */

/* Slot Flag Register bits (SLFR) */
#define AACI_SLFR_1TXB          (1U << 0)   /* Slot 1 TX Busy */
#define AACI_SLFR_2TXB          (1U << 1)   /* Slot 2 TX Busy */
#define AACI_SLFR_12TXB         (1U << 2)   /* Slot 1&2 TX Busy */
#define AACI_SLFR_1RXV          (1U << 3)   /* Slot 1 RX Valid */
#define AACI_SLFR_2RXV          (1U << 4)   /* Slot 2 RX Valid */
#define AACI_SLFR_12RXV         (1U << 5)   /* Slot 1&2 RX Valid */

/* Main Control Register (MAINCR) */
#define AACI_MAINCR_IE          (1U << 0)   /* Interface Enable */

/* ============================================================================
 * AC'97 Codec Registers (LM4549)
 * ============================================================================ */
#define AC97_REG_RESET          0x00
#define AC97_REG_MASTER_VOL     0x02
#define AC97_REG_HEADPHONE_VOL  0x04
#define AC97_REG_MASTER_MONO    0x06
#define AC97_REG_PCBEEP_VOL     0x0A
#define AC97_REG_PHONE_VOL      0x0C
#define AC97_REG_MIC_VOL        0x0E
#define AC97_REG_LINEIN_VOL     0x10
#define AC97_REG_CD_VOL         0x12
#define AC97_REG_VIDEO_VOL      0x14
#define AC97_REG_AUX_VOL        0x16
#define AC97_REG_PCMOUT_VOL     0x18
#define AC97_REG_REC_SELECT     0x1A
#define AC97_REG_REC_GAIN       0x1C
#define AC97_REG_GENERAL_PURP   0x20
#define AC97_REG_3D_CONTROL     0x22
#define AC97_REG_POWERDOWN      0x26
#define AC97_REG_EXT_AUDIO_ID   0x28
#define AC97_REG_EXT_AUDIO_STAT 0x2A
#define AC97_REG_PCM_FRONT_DAC  0x2C
#define AC97_REG_PCM_LR_ADC     0x32
#define AC97_REG_VENDOR_ID1     0x7C
#define AC97_REG_VENDOR_ID2     0x7E

/* ============================================================================
 * Sound Effects & Presets
 * ============================================================================ */
typedef enum {
    AUDIO_FX_BEEP = 0,
    AUDIO_FX_CLICK,
    AUDIO_FX_POPUP,
    AUDIO_FX_ERROR,
    AUDIO_FX_COIN,
    AUDIO_FX_LASER,
    AUDIO_FX_EXPLODE,
    AUDIO_FX_BOOT
} audio_fx_t;

/* ============================================================================
 * OSS Audio IOCTL Definitions (for /dev/dsp compatibility)
 * ============================================================================ */
#define SNDCTL_DSP_RESET        0x5000
#define SNDCTL_DSP_SYNC         0x5001
#define SNDCTL_DSP_SPEED        0x5002
#define SNDCTL_DSP_STEREO       0x5003
#define SNDCTL_DSP_GETBLKSIZE   0x5004
#define SNDCTL_DSP_SETFMT       0x5005
#define SNDCTL_DSP_CHANNELS     0x5006

#define AFMT_U8                 0x00000008
#define AFMT_S16_LE             0x00000010

/* ============================================================================
 * Audio Driver Public API
 * ============================================================================ */

/**
 * audio_init — Initialize PL041 AACI audio hardware, configure AC'97 codec,
 * un-mute outputs, and allocate the playback ring buffer.
 * @return 0 on success, negative on error
 */
int audio_init(void);

/**
 * audio_set_sample_rate — Configure AC'97 hardware DAC sample rate.
 * @param rate  Sample rate in Hz (e.g. 11025, 22050, 44100, 48000)
 * @return 0 on success, negative on error
 */
int audio_set_sample_rate(uint32_t rate);

/**
 * audio_set_volume — Set output master volume (0 to 100%).
 * @param percent  Volume level 0 (silent) to 100 (loudest)
 */
void audio_set_volume(uint8_t percent);

/**
 * audio_write_pcm — Queue raw PCM audio bytes into the playback ring buffer.
 * Non-blocking if buffer has space.
 * @param data      Pointer to PCM data (16-bit signed stereo or mono)
 * @param num_bytes Size of data in bytes
 * @return Number of bytes accepted into ring buffer
 */
int32_t audio_write_pcm(const void *data, size_t num_bytes);

/**
 * audio_play_pcm — Play PCM buffer with specified format parameters.
 * @param samples       Buffer of PCM samples
 * @param num_bytes     Size in bytes
 * @param sample_rate   Sample rate in Hz (e.g. 22050, 44100)
 * @param channels      1 (mono) or 2 (stereo)
 * @param bits          8 or 16
 * @return 0 on success, negative on error
 */
int audio_play_pcm(const void *samples, size_t num_bytes, uint32_t sample_rate, uint8_t channels, uint8_t bits);

/**
 * audio_play_tone — Generate and play a square/sine wave frequency tone.
 * @param freq_hz       Tone frequency (e.g. 440 for A4, 1000 for high beep)
 * @param duration_ms   Duration in milliseconds
 * @param volume        Volume level 0-100%
 * @return 0 on success
 */
int audio_play_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume);

/**
 * audio_beep — Quick utility beep helper.
 * @param freq_hz       Frequency in Hz
 * @param duration_ms   Duration in milliseconds
 * @return 0 on success
 */
int audio_beep(uint32_t freq_hz, uint32_t duration_ms);

/**
 * audio_play_fx — Play a synthesized retro sound effect preset.
 * @param fx  Sound effect type (AUDIO_FX_*)
 * @return 0 on success
 */
int audio_play_fx(audio_fx_t fx);

/**
 * audio_play_wav — Load and play a standard .wav audio file from the filesystem.
 * @param fat_path  Path on FAT filesystem (e.g. "/SOUNDS/BOOT.WAV")
 * @return 0 on success, negative on error
 */
int audio_play_wav(const char *fat_path);

/**
 * audio_poll — Feed pending audio samples from ring buffer into PL041 hardware FIFO.
 * Should be called regularly (e.g. from timer tick / background loop).
 */
void audio_poll(void);

/**
 * audio_stop — Stop playback and clear all active buffers.
 */
void audio_stop(void);

/**
 * audio_is_playing — Check if audio is currently streaming.
 * @return 1 if audio active, 0 if idle
 */
int audio_is_playing(void);

#endif /* AUDIO_H */
