#include <Arduino.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "esp_heap_caps.h"

#include "t5_model.h"
#include "t5_profile_vectors.h"

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

constexpr int kTensorArenaBytes = 96 * 1024;   // currently testing 96 KB
constexpr int kFailureRamBytes = 96 * 1024;
constexpr int kProfileRepeats = 20;
constexpr int kMaxChunks = 128;
constexpr int kSmallHeaderProxyBytes = 16;

static uint8_t* tensor_arena = nullptr;
static bool g_finished = false;

static tflite::MicroErrorReporter micro_error_reporter;
static tflite::MicroMutableOpResolver<12> resolver;
static tflite::MicroInterpreter* interpreter = nullptr;

bool ModelUsesOp(const tflite::Model* model, tflite::BuiltinOperator target_op) {
  if (model == nullptr || model->operator_codes() == nullptr ||
      model->subgraphs() == nullptr || model->subgraphs()->size() == 0) {
    return false;
  }

  const auto* opcodes = model->operator_codes();
  const auto* subgraph = model->subgraphs()->Get(0);

  if (subgraph == nullptr || subgraph->operators() == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < subgraph->operators()->size(); ++i) {
    const auto* op = subgraph->operators()->Get(i);
    if (op == nullptr) {
      continue;
    }

    const uint32_t opcode_index = op->opcode_index();
    if (opcode_index >= opcodes->size()) {
      continue;
    }

    const auto* opcode = opcodes->Get(opcode_index);
    if (opcode == nullptr) {
      continue;
    }

    const int32_t builtin_code =
        static_cast<int32_t>(opcode->builtin_code());

    const int32_t deprecated_builtin_code =
        static_cast<int32_t>(opcode->deprecated_builtin_code());

    const int32_t target =
        static_cast<int32_t>(target_op);

    if (builtin_code == target || deprecated_builtin_code == target) {
      return true;
    }
  }

  return false;
}

int PositiveElementCount(const TfLiteTensor* tensor) {
  if (tensor == nullptr || tensor->dims == nullptr) {
    return 0;
  }

  int count = 1;

  for (int i = 0; i < tensor->dims->size; ++i) {
    const int d = tensor->dims->data[i];

    if (d <= 0) {
      return 0;
    }

    count *= d;
  }

  return count;
}

uint32_t Percentile95(std::vector<uint32_t>& samples) {
  if (samples.empty()) {
    return 0;
  }

  std::sort(samples.begin(), samples.end());

  const size_t n = samples.size();
  size_t idx = (n * 95 + 99) / 100;

  if (idx == 0) {
    idx = 1;
  }

  idx -= 1;

  if (idx >= n) {
    idx = n - 1;
  }

  return samples[idx];
}

int8_t QuantizeInt16ToInt8(int16_t input, float scale, int zero_point) {
  if (scale <= 0.0f) {
    return 0;
  }

  const float transformed =
      static_cast<float>(input) / scale + static_cast<float>(zero_point);

  int32_t q = static_cast<int32_t>(std::lround(transformed));

  if (q < -128) {
    q = -128;
  } else if (q > 127) {
    q = 127;
  }

  return static_cast<int8_t>(q);
}

void PrintFailureRow(const char* note) {
  Serial.print("T5PROFILE,esp32,tflite_micro_encoder_int8,16,0,0,0,0,0,0,0,0,");
  Serial.print(kFailureRamBytes);
  Serial.print(",flash_size,failed,");
  Serial.println(note);
  Serial.flush();
}

bool AllocateTensorArena() {
  if (tensor_arena != nullptr) {
    return true;
  }

  Serial.print("FREE_HEAP=");
  Serial.print(ESP.getFreeHeap());
  Serial.print(",MAX_ALLOC=");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.flush();

  tensor_arena = static_cast<uint8_t*>(
      heap_caps_malloc(kTensorArenaBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

  if (tensor_arena == nullptr) {
    tensor_arena = static_cast<uint8_t*>(
        heap_caps_malloc(kTensorArenaBytes, MALLOC_CAP_8BIT));
  }

  return tensor_arena != nullptr;
}

void RunProfile() {
  if (!AllocateTensorArena()) {
    PrintFailureRow("tflm_allocation_failed");
    return;
  }

  const tflite::Model* model =
      tflite::GetModel(t5_encoder_w16_z8_int8_tflite);

  if (model == nullptr || model->version() != TFLITE_SCHEMA_VERSION) {
    PrintFailureRow("tflm_model_schema_failed");
    return;
  }

  bool resolver_ok = true;

  resolver_ok = resolver_ok && (resolver.AddFullyConnected() == kTfLiteOk);
  resolver_ok = resolver_ok && (resolver.AddReshape() == kTfLiteOk);
  resolver_ok = resolver_ok && (resolver.AddRelu() == kTfLiteOk);

  if (ModelUsesOp(model, tflite::BuiltinOperator_SHAPE)) {
    resolver_ok = resolver_ok && (resolver.AddShape() == kTfLiteOk);
  }

  if (ModelUsesOp(model, tflite::BuiltinOperator_STRIDED_SLICE)) {
    resolver_ok = resolver_ok && (resolver.AddStridedSlice() == kTfLiteOk);
  
  }

  if (ModelUsesOp(model, tflite::BuiltinOperator_PACK)) {
    resolver_ok = resolver_ok && (resolver.AddPack() == kTfLiteOk);
  }

  if (ModelUsesOp(model, tflite::BuiltinOperator_QUANTIZE)) {
    resolver_ok = resolver_ok && (resolver.AddQuantize() == kTfLiteOk);
  }

  if (ModelUsesOp(model, tflite::BuiltinOperator_DEQUANTIZE)) {
    resolver_ok = resolver_ok && (resolver.AddDequantize() == kTfLiteOk);
  }

  if (!resolver_ok) {
    PrintFailureRow("tflm_resolver_failed");
    return;
  }

  interpreter = new tflite::MicroInterpreter(
      model,
      resolver,
      tensor_arena,
      kTensorArenaBytes,
      &micro_error_reporter,
      nullptr,
      nullptr);

  if (interpreter == nullptr) {
    PrintFailureRow("tflm_interpreter_create_failed");
    return;
  }

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    PrintFailureRow("tflm_allocation_failed");
    return;
  }

  TfLiteTensor* input = interpreter->input(0);
  TfLiteTensor* output = interpreter->output(0);

  if (input == nullptr || output == nullptr || input->type != kTfLiteInt8) {
    PrintFailureRow("tflm_input_output_failed");
    return;
  }

  const int input_element_count = PositiveElementCount(input);
  const int expected_input_elements =
      T5_PROFILE_W * T5_PROFILE_N_CHANNELS;

  if (input_element_count != expected_input_elements ||
      input->data.int8 == nullptr) {
    PrintFailureRow("tflm_input_shape_failed");
    return;
  }

  const float input_scale = input->params.scale;
  const int input_zero_point = input->params.zero_point;

  if (input_scale <= 0.0f) {
    PrintFailureRow("tflm_input_quant_failed");
    return;
  }

  const int n_chunks =
      (T5_PROFILE_N_CHUNKS < kMaxChunks) ? T5_PROFILE_N_CHUNKS : kMaxChunks;

  const size_t total_invocations =
      static_cast<size_t>(kProfileRepeats) * static_cast<size_t>(n_chunks);

  std::vector<uint32_t> timings;
  timings.reserve(total_invocations);

  uint64_t sum_us = 0;

  for (int r = 0; r < kProfileRepeats; ++r) {
    for (int chunk = 0; chunk < n_chunks; ++chunk) {
      const int base = chunk * expected_input_elements;

      for (int i = 0; i < expected_input_elements; ++i) {
        const int16_t raw_value = T5_PROFILE_DATA[base + i];
        input->data.int8[i] =
            QuantizeInt16ToInt8(raw_value, input_scale, input_zero_point);
      }

      const uint32_t t0 = micros();

      if (interpreter->Invoke() != kTfLiteOk) {
        PrintFailureRow("tflm_invoke_failed");
        return;
      }

      const uint32_t dt = micros() - t0;

      timings.push_back(dt);
      sum_us += dt;
    }
  }

  if (timings.empty()) {
    PrintFailureRow("tflm_no_timings");
    return;
  }

  const float encode_us_mean =
      static_cast<float>(sum_us) / static_cast<float>(timings.size());

  const uint32_t encode_us_p95 = Percentile95(timings);

  if (output->type != kTfLiteInt8) {
    PrintFailureRow("tflm_output_type_failed");
    return;
  }

  const int latent_dim = PositiveElementCount(output);

  if (latent_dim <= 0) {
    PrintFailureRow("tflm_output_shape_failed");
    return;
  }

  const uint32_t bytes_in =
      static_cast<uint32_t>(T5_PROFILE_W) *
      static_cast<uint32_t>(T5_PROFILE_N_CHANNELS) *
      static_cast<uint32_t>(sizeof(int16_t)) *
      static_cast<uint32_t>(n_chunks);

  const uint32_t bytes_out =
      static_cast<uint32_t>(latent_dim) *
      static_cast<uint32_t>(sizeof(int8_t)) *
      static_cast<uint32_t>(n_chunks) +
      static_cast<uint32_t>(kSmallHeaderProxyBytes);

  const float compression_ratio =
      (bytes_out == 0)
          ? 0.0f
          : static_cast<float>(bytes_in) / static_cast<float>(bytes_out);

  Serial.print("T5PROFILE,esp32,tflite_micro_encoder_int8,");
  Serial.print(T5_PROFILE_W);
  Serial.print(",");
  Serial.print(n_chunks);
  Serial.print(",");
  Serial.print(bytes_in);
  Serial.print(",");
  Serial.print(bytes_out);
  Serial.print(",");
  Serial.print(compression_ratio, 6);
  Serial.print(",");
  Serial.print(encode_us_mean, 2);
  Serial.print(",");
  Serial.print(encode_us_p95);
  Serial.print(",0,0,");
  Serial.print(kTensorArenaBytes);
  Serial.print(",");
  Serial.print(t5_encoder_w16_z8_int8_tflite_len);
  Serial.print(",ok,energy_status=not_measured_proxy_only;encoder_only=true;tensor_arena_bytes=");
  Serial.println(kTensorArenaBytes);
  Serial.flush();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  RunProfile();

  g_finished = true;
}

void loop() {
  delay(1000);
}