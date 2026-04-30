#include <Arduino.h>
#include <string.h>
#include "t5_profile_vectors.h"

#ifndef T5_PROFILE_W
#error "T5_PROFILE_W not defined in t5_profile_vectors.h"
#endif

#ifndef T5_PROFILE_N_CHUNKS
#error "T5_PROFILE_N_CHUNKS not defined in t5_profile_vectors.h"
#endif

#ifndef T5_PROFILE_N_CHANNELS
#error "T5_PROFILE_N_CHANNELS not defined in t5_profile_vectors.h"
#endif

static const int REPEATS = 30;

// Volatile checksum prevents compiler from removing decode/reconstruction work.
volatile int32_t t5_decode_checksum = 0;
volatile size_t t5_bytes_sink = 0;

static int32_t raw_buffer[T5_PROFILE_W * T5_PROFILE_N_CHANNELS];
static int32_t delta_buffer[T5_PROFILE_W * T5_PROFILE_N_CHANNELS];
static int32_t recon_buffer[T5_PROFILE_W * T5_PROFILE_N_CHANNELS];

#define NOINLINE __attribute__((noinline))

static inline int data_index(int chunk, int t, int c) {
  return (chunk * T5_PROFILE_W * T5_PROFILE_N_CHANNELS) + (t * T5_PROFILE_N_CHANNELS) + c;
}

static NOINLINE int32_t consume_recon_checksum() {
  int32_t s = 0;
  const int n = T5_PROFILE_W * T5_PROFILE_N_CHANNELS;

  for (int i = 0; i < n; i++) {
    // Mix values so compiler cannot simplify to one read.
    s ^= (recon_buffer[i] + (i * 131));
  }

  t5_decode_checksum ^= s;
  return s;
}

static NOINLINE void load_chunk_raw_copy(int chunk) {
  int idx = 0;
  for (int t = 0; t < T5_PROFILE_W; t++) {
    for (int c = 0; c < T5_PROFILE_N_CHANNELS; c++) {
      raw_buffer[idx++] = (int32_t)T5_PROFILE_DATA[data_index(chunk, t, c)];
    }
  }
}

static NOINLINE size_t encode_raw_copy(int chunk) {
  load_chunk_raw_copy(chunk);
  return (size_t)(T5_PROFILE_W * T5_PROFILE_N_CHANNELS * sizeof(int16_t));
}

static NOINLINE size_t decode_raw_copy() {
  const int n = T5_PROFILE_W * T5_PROFILE_N_CHANNELS;

  for (int i = 0; i < n; i++) {
    recon_buffer[i] = raw_buffer[i];
  }

  consume_recon_checksum();
  return (size_t)(n * sizeof(int16_t));
}

static NOINLINE size_t encode_delta_int16(int chunk) {
  int idx = 0;

  // Store layout as channel-major residuals:
  // c0:t0,t1,..., c1:t0,t1,...
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

static NOINLINE size_t decode_delta_int16() {
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

      // Store reconstructed output in the same channel-major order used by residuals.
      recon_buffer[idx] = x;
      prev = x;
      idx++;
    }
  }

  consume_recon_checksum();
  return (size_t)(T5_PROFILE_W * T5_PROFILE_N_CHANNELS * sizeof(int16_t));
}

static NOINLINE size_t encode_simple_predictor_pack(int chunk) {
  int idx = 0;
  size_t estimated_bytes = 0;

  for (int c = 0; c < T5_PROFILE_N_CHANNELS; c++) {
    int16_t prev = 0;

    for (int t = 0; t < T5_PROFILE_W; t++) {
      int16_t x = (int16_t)T5_PROFILE_DATA[data_index(chunk, t, c)];
      int32_t residual = (t == 0) ? x : ((int32_t)x - (int32_t)prev);

      delta_buffer[idx++] = residual;

      // Simple byte-cost proxy:
      // small residual fits 1 byte, otherwise 2 bytes.
      // This is not a real entropy-coded bitstream. It is a lightweight packed-size proxy.
      if (residual >= -127 && residual <= 127) {
        estimated_bytes += 1;
      } else {
        estimated_bytes += 2;
      }

      prev = x;
    }
  }

  // Small fixed header overhead per chunk.
  estimated_bytes += 4;
  return estimated_bytes;
}

static NOINLINE size_t decode_simple_predictor_pack() {
  // Reconstruction path is the same as delta residual reconstruction.
  // This measures predictor reconstruction cost, not actual entropy unpacking cost.
  return decode_delta_int16();
}

static float mean_u32(const uint32_t *vals, int n) {
  uint64_t s = 0;
  for (int i = 0; i < n; i++) s += vals[i];
  return (float)s / (float)n;
}

static uint32_t p95_u32(uint32_t *vals, int n) {
  // Sort copy in-place. Caller passes temporary array when needed.
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
  if (idx < 0) idx = 0;
  if (idx >= n) idx = n - 1;
  return vals[idx];
}

static void copy_u32(uint32_t *dst, const uint32_t *src, int n) {
  for (int i = 0; i < n; i++) dst[i] = src[i];
}

static size_t encode_one(const char *method, int chunk) {
  if (strcmp(method, "raw_copy") == 0) {
    return encode_raw_copy(chunk);
  } else if (strcmp(method, "delta_int16") == 0) {
    return encode_delta_int16(chunk);
  } else if (strcmp(method, "simple_predictor_pack") == 0) {
    return encode_simple_predictor_pack(chunk);
  }

  return 0;
}

static size_t decode_one(const char *method) {
  if (strcmp(method, "raw_copy") == 0) {
    return decode_raw_copy();
  } else if (strcmp(method, "delta_int16") == 0) {
    return decode_delta_int16();
  } else if (strcmp(method, "simple_predictor_pack") == 0) {
    return decode_simple_predictor_pack();
  }

  return 0;
}

static void profile_method(const char *method) {
  uint32_t enc_times[REPEATS];
  uint32_t dec_times[REPEATS];
  uint32_t enc_times_sorted[REPEATS];
  uint32_t dec_times_sorted[REPEATS];

  const size_t bytes_in =
      (size_t)T5_PROFILE_N_CHUNKS *
      T5_PROFILE_W *
      T5_PROFILE_N_CHANNELS *
      sizeof(int16_t);

  size_t bytes_out_total = 0;

  // Warmup.
  for (int chunk = 0; chunk < T5_PROFILE_N_CHUNKS; chunk++) {
    t5_bytes_sink += encode_one(method, chunk);
    t5_bytes_sink += decode_one(method);
  }

  for (int r = 0; r < REPEATS; r++) {
    size_t bytes_out_run = 0;

    // Encode timing: every chunk is encoded.
    uint32_t enc_start = micros();
    for (int chunk = 0; chunk < T5_PROFILE_N_CHUNKS; chunk++) {
      bytes_out_run += encode_one(method, chunk);
    }
    uint32_t enc_end = micros();

    // Decode timing: for each chunk, prepare the encoded representation outside
    // the timed decode region, then time only decode/reconstruction.
    uint32_t dec_total = 0;
    for (int chunk = 0; chunk < T5_PROFILE_N_CHUNKS; chunk++) {
      t5_bytes_sink += encode_one(method, chunk);

      uint32_t dec_start = micros();
      t5_bytes_sink += decode_one(method);
      uint32_t dec_end = micros();

      dec_total += (dec_end - dec_start);
    }

    enc_times[r] = enc_end - enc_start;
    dec_times[r] = dec_total;

    if (r == 0) {
      bytes_out_total = bytes_out_run;
    }
  }

  copy_u32(enc_times_sorted, enc_times, REPEATS);
  copy_u32(dec_times_sorted, dec_times, REPEATS);

  float enc_mean_total = mean_u32(enc_times, REPEATS);
  uint32_t enc_p95_total = p95_u32(enc_times_sorted, REPEATS);

  float dec_mean_total = mean_u32(dec_times, REPEATS);
  uint32_t dec_p95_total = p95_u32(dec_times_sorted, REPEATS);

  float enc_mean_per_chunk = enc_mean_total / (float)T5_PROFILE_N_CHUNKS;
  float enc_p95_per_chunk = (float)enc_p95_total / (float)T5_PROFILE_N_CHUNKS;

  float dec_mean_per_chunk = dec_mean_total / (float)T5_PROFILE_N_CHUNKS;
  float dec_p95_per_chunk = (float)dec_p95_total / (float)T5_PROFILE_N_CHUNKS;

  float cr = 0.0f;
  if (bytes_out_total > 0) {
    cr = (float)bytes_in / (float)bytes_out_total;
  }

  size_t ram_bytes = sizeof(raw_buffer) + sizeof(delta_buffer) + sizeof(recon_buffer);

  Serial.print("T5PROFILE,");
  Serial.print("esp32,");
  Serial.print(method);
  Serial.print(",");
  Serial.print(T5_PROFILE_W);
  Serial.print(",");
  Serial.print(T5_PROFILE_N_CHUNKS);
  Serial.print(",");
  Serial.print(bytes_in);
  Serial.print(",");
  Serial.print(bytes_out_total);
  Serial.print(",");
  Serial.print(cr, 6);
  Serial.print(",");
  Serial.print(enc_mean_per_chunk, 3);
  Serial.print(",");
  Serial.print(enc_p95_per_chunk, 3);
  Serial.print(",");
  Serial.print(dec_mean_per_chunk, 3);
  Serial.print(",");
  Serial.print(dec_p95_per_chunk, 3);
  Serial.print(",");
  Serial.print(ram_bytes);
  Serial.print(",");
  Serial.print(ESP.getSketchSize());
  Serial.print(",");
  Serial.print("ok,");
  Serial.print("energy_status=not_measured_proxy_only");
  Serial.print(";checksum=");
  Serial.print((int32_t)t5_decode_checksum);
  Serial.print(";bytes_sink=");
  Serial.println((size_t)t5_bytes_sink);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("T5 ESP32 profiling start");
  Serial.print("W=");
  Serial.print(T5_PROFILE_W);
  Serial.print(" chunks=");
  Serial.print(T5_PROFILE_N_CHUNKS);
  Serial.print(" channels=");
  Serial.println(T5_PROFILE_N_CHANNELS);

  profile_method("raw_copy");
  profile_method("delta_int16");
  profile_method("simple_predictor_pack");

  Serial.println("T5 ESP32 profiling done");
}

void loop() {
  delay(1000);
}