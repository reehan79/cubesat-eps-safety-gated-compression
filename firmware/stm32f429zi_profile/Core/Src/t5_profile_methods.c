/*
 * Classical encode/decode timing proxies for T5 profile vectors (STM32).
 * Mirrors firmware/esp32_t5_profile/src/main.cpp — not CCSDS-124, no on-device AI inference.
 */

#include "t5_profile_vectors.h"
#include "t5_profile_methods.h"

#include "stm32f4xx_hal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifndef T5_PROFILE_W
#error "T5_PROFILE_W not defined in t5_profile_vectors.h"
#endif
#ifndef T5_PROFILE_N_CHUNKS
#error "T5_PROFILE_N_CHUNKS not defined in t5_profile_vectors.h"
#endif
#ifndef T5_PROFILE_N_CHANNELS
#error "T5_PROFILE_N_CHANNELS not defined in t5_profile_vectors.h"
#endif

#if defined(STM32F429xx)
#define T5_PROFILE_BOARD_TOKEN "stm32f429zi"
#elif defined(STM32F413xx)
#define T5_PROFILE_BOARD_TOKEN "stm32f413zh"
#else
#define T5_PROFILE_BOARD_TOKEN "stm32_unknown"
#endif

/* Linker symbols (GCC STM32Cube) */
extern uint32_t _etext;
extern uint32_t _sdata;
extern uint32_t _edata;

static const int REPEATS = 30;

volatile int32_t t5_decode_checksum = 0;
volatile size_t t5_bytes_sink = 0;

static int32_t raw_buffer[T5_PROFILE_W * T5_PROFILE_N_CHANNELS];
static int32_t delta_buffer[T5_PROFILE_W * T5_PROFILE_N_CHANNELS];
static int32_t recon_buffer[T5_PROFILE_W * T5_PROFILE_N_CHANNELS];

static uint8_t g_use_dwt = 1;
static uint32_t g_timer_mark;

#define NOINLINE __attribute__((noinline))

static NOINLINE int t5_try_enable_dwt(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  uint32_t a = DWT->CYCCNT;
  volatile uint32_t spin = 0;
  for (int i = 0; i < 256; i++) {
    spin += (uint32_t)i;
  }
  (void)spin;
  uint32_t b = DWT->CYCCNT;
  return (b > a) ? 1 : 0;
}

static inline void t5_timer_start(void)
{
  if (g_use_dwt) {
    g_timer_mark = DWT->CYCCNT;
  } else {
    g_timer_mark = HAL_GetTick();
  }
}

static inline uint32_t t5_timer_elapsed_since_mark(void)
{
  uint32_t now;
  if (g_use_dwt) {
    now = DWT->CYCCNT;
  } else {
    now = HAL_GetTick();
  }
  if (now >= g_timer_mark) {
    return now - g_timer_mark;
  }
  return (UINT32_MAX - g_timer_mark + 1U) + now;
}

static float t5_raw_to_total_us(uint32_t raw)
{
  if (g_use_dwt) {
    if (SystemCoreClock == 0U) {
      return 0.0f;
    }
    return (float)((uint64_t)raw * 1000000ULL / (uint64_t)SystemCoreClock);
  }
  /* HAL_GetTick: ms resolution */
  return (float)raw * 1000.0f;
}

static uint32_t t5_flash_estimate_bytes(void)
{
  uint32_t text_ro = (uint32_t)(uintptr_t)&_etext - (uint32_t)FLASH_BASE;
  uint32_t init_data = (uint32_t)(uintptr_t)&_edata - (uint32_t)(uintptr_t)&_sdata;
  return text_ro + init_data;
}

static inline int data_index(int chunk, int t, int c)
{
  return (chunk * T5_PROFILE_W * T5_PROFILE_N_CHANNELS) + (t * T5_PROFILE_N_CHANNELS) + c;
}

static NOINLINE int32_t consume_recon_checksum(void)
{
  int32_t s = 0;
  const int n = T5_PROFILE_W * T5_PROFILE_N_CHANNELS;

  for (int i = 0; i < n; i++) {
    s ^= (recon_buffer[i] + (i * 131));
  }

  t5_decode_checksum ^= s;
  return s;
}

static NOINLINE void load_chunk_raw_copy(int chunk)
{
  int idx = 0;
  for (int t = 0; t < T5_PROFILE_W; t++) {
    for (int c = 0; c < T5_PROFILE_N_CHANNELS; c++) {
      raw_buffer[idx++] = (int32_t)T5_PROFILE_DATA[data_index(chunk, t, c)];
    }
  }
}

static NOINLINE size_t encode_raw_copy(int chunk)
{
  load_chunk_raw_copy(chunk);
  return (size_t)(T5_PROFILE_W * T5_PROFILE_N_CHANNELS * sizeof(int16_t));
}

static NOINLINE size_t decode_raw_copy(void)
{
  const int n = T5_PROFILE_W * T5_PROFILE_N_CHANNELS;

  for (int i = 0; i < n; i++) {
    recon_buffer[i] = raw_buffer[i];
  }

  consume_recon_checksum();
  return (size_t)(n * sizeof(int16_t));
}

static NOINLINE size_t encode_delta_int16(int chunk)
{
  int idx = 0;

  for (int c = 0; c < T5_PROFILE_N_CHANNELS; c++) {
    int16_t prev = 0;

    for (int t = 0; t < T5_PROFILE_W; t++) {
      int16_t x = (int16_t)T5_PROFILE_DATA[data_index(chunk, t, c)];

      if (t == 0) {
        delta_buffer[idx++] = x;
      } else {
        delta_buffer[idx++] = (int32_t)x - (int32_t)prev;
      }

      prev = x;
    }
  }

  return (size_t)(T5_PROFILE_W * T5_PROFILE_N_CHANNELS * sizeof(int16_t));
}

static NOINLINE size_t decode_delta_int16(void)
{
  int idx = 0;

  for (int c = 0; c < T5_PROFILE_N_CHANNELS; c++) {
    int32_t prev = 0;

    for (int t = 0; t < T5_PROFILE_W; t++) {
      int32_t d = delta_buffer[idx];
      int32_t x;

      if (t == 0) {
        x = d;
      } else {
        x = prev + d;
      }

      recon_buffer[idx] = x;
      prev = x;
      idx++;
    }
  }

  consume_recon_checksum();
  return (size_t)(T5_PROFILE_W * T5_PROFILE_N_CHANNELS * sizeof(int16_t));
}

static NOINLINE size_t encode_simple_predictor_pack(int chunk)
{
  int idx = 0;
  size_t estimated_bytes = 0;

  for (int c = 0; c < T5_PROFILE_N_CHANNELS; c++) {
    int16_t prev = 0;

    for (int t = 0; t < T5_PROFILE_W; t++) {
      int16_t x = (int16_t)T5_PROFILE_DATA[data_index(chunk, t, c)];
      int32_t residual = (t == 0) ? x : ((int32_t)x - (int32_t)prev);

      delta_buffer[idx++] = residual;

      if (residual >= -127 && residual <= 127) {
        estimated_bytes += 1;
      } else {
        estimated_bytes += 2;
      }

      prev = x;
    }
  }

  estimated_bytes += 4;
  return estimated_bytes;
}

static NOINLINE size_t decode_simple_predictor_pack(void) { return decode_delta_int16(); }

static float mean_u32(const uint32_t *vals, int n)
{
  uint64_t s = 0;
  for (int i = 0; i < n; i++) {
    s += vals[i];
  }
  return (float)s / (float)n;
}

static uint32_t p95_u32(uint32_t *vals, int n)
{
  for (int i = 1; i < n; i++) {
    uint32_t key = vals[i];
    int j = i - 1;
    while (j >= 0 && vals[j] > key) {
      vals[j + 1] = vals[j];
      j--;
    }
    vals[j + 1] = key;
  }

  int idx = (int)(0.95f * (float)(n - 1));
  if (idx < 0) {
    idx = 0;
  }
  if (idx >= n) {
    idx = n - 1;
  }
  return vals[idx];
}

static void copy_u32(uint32_t *dst, const uint32_t *src, int n)
{
  for (int i = 0; i < n; i++) {
    dst[i] = src[i];
  }
}

static size_t encode_one(const char *method, int chunk)
{
  if (strcmp(method, "raw_copy") == 0) {
    return encode_raw_copy(chunk);
  }
  if (strcmp(method, "delta_int16") == 0) {
    return encode_delta_int16(chunk);
  }
  if (strcmp(method, "simple_predictor_pack") == 0) {
    return encode_simple_predictor_pack(chunk);
  }
  return 0;
}

static size_t decode_one(const char *method)
{
  if (strcmp(method, "raw_copy") == 0) {
    return decode_raw_copy();
  }
  if (strcmp(method, "delta_int16") == 0) {
    return decode_delta_int16();
  }
  if (strcmp(method, "simple_predictor_pack") == 0) {
    return decode_simple_predictor_pack();
  }
  return 0;
}

static void profile_method(const char *method)
{
  uint32_t enc_times[REPEATS];
  uint32_t dec_times[REPEATS];
  uint32_t enc_times_sorted[REPEATS];
  uint32_t dec_times_sorted[REPEATS];

  const uint32_t bytes_in =
      (uint32_t)T5_PROFILE_N_CHUNKS * (uint32_t)T5_PROFILE_W * (uint32_t)T5_PROFILE_N_CHANNELS *
      (uint32_t)sizeof(int16_t);

  size_t bytes_out_total = 0;

  for (int chunk = 0; chunk < T5_PROFILE_N_CHUNKS; chunk++) {
    t5_bytes_sink += encode_one(method, chunk);
    t5_bytes_sink += decode_one(method, chunk);
  }

  for (int r = 0; r < REPEATS; r++) {
    size_t bytes_out_run = 0;

    t5_timer_start();
    for (int chunk = 0; chunk < T5_PROFILE_N_CHUNKS; chunk++) {
      bytes_out_run += encode_one(method, chunk);
    }
    uint32_t enc_us =
        (uint32_t)(t5_raw_to_total_us(t5_timer_elapsed_since_mark()) + 0.5f);

    uint32_t dec_total_raw = 0;
    for (int chunk = 0; chunk < T5_PROFILE_N_CHUNKS; chunk++) {
      t5_bytes_sink += encode_one(method, chunk);

      t5_timer_start();
      t5_bytes_sink += decode_one(method, chunk);
      dec_total_raw += t5_timer_elapsed_since_mark();
    }
    uint32_t dec_us = (uint32_t)(t5_raw_to_total_us(dec_total_raw) + 0.5f);

    enc_times[r] = enc_us;
    dec_times[r] = dec_us;

    if (r == 0) {
      bytes_out_total = bytes_out_run;
    }
  }

  copy_u32(enc_times_sorted, enc_times, REPEATS);
  copy_u32(dec_times_sorted, dec_times, REPEATS);

  float enc_mean_total_us = mean_u32(enc_times, REPEATS);
  uint32_t enc_p95_us = p95_u32(enc_times_sorted, REPEATS);

  float dec_mean_total_us = mean_u32(dec_times, REPEATS);
  uint32_t dec_p95_us = p95_u32(dec_times_sorted, REPEATS);

  float enc_mean_per_chunk = enc_mean_total_us / (float)T5_PROFILE_N_CHUNKS;
  float enc_p95_per_chunk = (float)enc_p95_us / (float)T5_PROFILE_N_CHUNKS;

  float dec_mean_per_chunk = dec_mean_total_us / (float)T5_PROFILE_N_CHUNKS;
  float dec_p95_per_chunk = (float)dec_p95_us / (float)T5_PROFILE_N_CHUNKS;

  float cr = 0.0f;
  if (bytes_out_total > 0) {
    cr = (float)bytes_in / (float)bytes_out_total;
  }

  size_t ram_bytes = sizeof(raw_buffer) + sizeof(delta_buffer) + sizeof(recon_buffer);
  uint32_t flash_bytes = t5_flash_estimate_bytes();

  const char *timing_note = g_use_dwt ? "dwt_cycles" : "hal_tick_fallback";

  printf(
      "T5PROFILE,%s,%s,%d,%d,%" PRIu32 ",%zu,%.6f,%.3f,%.3f,%.3f,%.3f,%zu,%" PRIu32
      ",ok,energy_status=not_measured_proxy_only;checksum=%" PRId32 ";bytes_sink=%zu;timing_source=%s\r\n",
      T5_PROFILE_BOARD_TOKEN, method, T5_PROFILE_W, T5_PROFILE_N_CHUNKS, bytes_in, bytes_out_total,
      cr, enc_mean_per_chunk, enc_p95_per_chunk, dec_mean_per_chunk, dec_p95_per_chunk, ram_bytes,
      flash_bytes, (int32_t)t5_decode_checksum, (size_t)t5_bytes_sink, timing_note);
}

void T5_Profile_RunAll(void)
{
  g_use_dwt = t5_try_enable_dwt() ? 1U : 0U;

  printf("T5 STM32 profiling start (%s)\r\n", T5_PROFILE_BOARD_TOKEN);
  printf("W=%d chunks=%d channels=%d timing=%s\r\n", T5_PROFILE_W, T5_PROFILE_N_CHUNKS,
         T5_PROFILE_N_CHANNELS, g_use_dwt ? "dwt_cycles" : "hal_tick_fallback");

  profile_method("raw_copy");
  profile_method("delta_int16");
  profile_method("simple_predictor_pack");

  printf("T5 STM32 profiling done\r\n");
}
