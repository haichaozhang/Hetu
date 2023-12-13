#include "hetu/core/ndarray.h"
#include "hetu/impl/stream/CUDAStream.h"
#include "hetu/impl/utils/common_utils.h"
#include "hetu/impl/utils/cuda_utils.h"
#include "hetu/impl/utils/cuda_math.h"
#include "hetu/impl/utils/offset_calculator.cuh"

namespace hetu {
namespace impl {

template <typename spec_t>
__global__ void floor_kernel(const spec_t* input, size_t size, spec_t* output,
                             const OffsetCalculator* in_offset_calculator,
                             const OffsetCalculator* out_offset_calculator) {
  auto idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < size) {
    auto in_offset = in_offset_calculator->get(idx);
    auto out_offset = out_offset_calculator->get(idx);
    output[out_offset] = hetu::cuda::cuda_floor(input[in_offset]);
  }
}

void FloorCuda(const NDArray& input, NDArray& output, const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, output);
  HT_ASSERT_SAME_SHAPE(input, output);

  size_t size = input->numel();
  if (size == 0)
    return;
  dim3 blocks, threads;
  threads.x = MIN(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  blocks.x = DIVUP(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  CUDAStream cuda_stream(stream);
  hetu::cuda::CUDADeviceGuard guard(cuda_stream.device_id());
  NDArray in_offset_calculator_arr, out_offset_calculator_arr;
  OffsetCalculator *in_offset_calculator, *out_offset_calculator;
  std::tie(in_offset_calculator_arr, in_offset_calculator) =
    AllocOffsetCalculator(input, stream);
  std::tie(out_offset_calculator_arr, out_offset_calculator) = 
    AllocOffsetCalculator(output, stream);
  HT_DISPATCH_FLOATING_TYPES(input->dtype(), spec_t, "FloorCuda", [&]() {
    floor_kernel<spec_t><<<blocks, threads, 0, cuda_stream>>>(
      input->data_ptr<spec_t>(), size, output->data_ptr<spec_t>(),
      in_offset_calculator, out_offset_calculator);
  });
  NDArray::MarkUsedBy({input, output, in_offset_calculator_arr,
                      out_offset_calculator_arr}, stream);
}

template <typename spec_t>
__global__ void ceil_kernel(const spec_t* input, size_t size, spec_t* output,
                            const OffsetCalculator* in_offset_calculator,
                            const OffsetCalculator* out_offset_calculator) {
  auto idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < size) {
    auto in_offset = in_offset_calculator->get(idx);
    auto out_offset = out_offset_calculator->get(idx);
    output[out_offset] = hetu::cuda::cuda_ceil(input[in_offset]);
  }
}

void CeilCuda(const NDArray& input, NDArray& output, const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, output);
  HT_ASSERT_SAME_SHAPE(input, output);

  size_t size = input->numel();
  if (size == 0)
    return;
  dim3 blocks, threads;
  threads.x = MIN(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  blocks.x = DIVUP(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  CUDAStream cuda_stream(stream);
  hetu::cuda::CUDADeviceGuard guard(cuda_stream.device_id());
  NDArray in_offset_calculator_arr, out_offset_calculator_arr;
  OffsetCalculator *in_offset_calculator, *out_offset_calculator;
  std::tie(in_offset_calculator_arr, in_offset_calculator) =
    AllocOffsetCalculator(input, stream);
  std::tie(out_offset_calculator_arr, out_offset_calculator) = 
    AllocOffsetCalculator(output, stream);
  HT_DISPATCH_FLOATING_TYPES(input->dtype(), spec_t, "CeilCuda", [&]() {
    ceil_kernel<spec_t><<<blocks, threads, 0, cuda_stream>>>(
      input->data_ptr<spec_t>(), size, output->data_ptr<spec_t>(),
      in_offset_calculator, out_offset_calculator);
  });
  NDArray::MarkUsedBy({input, output, in_offset_calculator_arr,
                      out_offset_calculator_arr}, stream);
}

template <typename spec_t>
__global__ void round_kernel(const spec_t* input, size_t size, spec_t* output,
                             const OffsetCalculator* in_offset_calculator,
                             const OffsetCalculator* out_offset_calculator) {
  auto idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < size) {
    auto in_offset = in_offset_calculator->get(idx);
    auto out_offset = out_offset_calculator->get(idx);
    output[out_offset] = hetu::cuda::cuda_round(input[in_offset]);
  }
}

void RoundCuda(const NDArray& input, NDArray& output, const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, output);
  HT_ASSERT_SAME_SHAPE(input, output);

  size_t size = input->numel();
  if (size == 0)
    return;
  dim3 blocks, threads;
  threads.x = MIN(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  blocks.x = DIVUP(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  CUDAStream cuda_stream(stream);
  hetu::cuda::CUDADeviceGuard guard(cuda_stream.device_id());
  NDArray in_offset_calculator_arr, out_offset_calculator_arr;
  OffsetCalculator *in_offset_calculator, *out_offset_calculator;
  std::tie(in_offset_calculator_arr, in_offset_calculator) =
    AllocOffsetCalculator(input, stream);
  std::tie(out_offset_calculator_arr, out_offset_calculator) = 
    AllocOffsetCalculator(output, stream);
  HT_DISPATCH_FLOATING_TYPES(input->dtype(), spec_t, "RoundCuda", [&]() {
    round_kernel<spec_t><<<blocks, threads, 0, cuda_stream>>>(
      input->data_ptr<spec_t>(), size, output->data_ptr<spec_t>(),
      in_offset_calculator, out_offset_calculator);
  });
  NDArray::MarkUsedBy({input, output, in_offset_calculator_arr,
                      out_offset_calculator_arr}, stream);
}

} // namespace impl
} // namespace hetu
