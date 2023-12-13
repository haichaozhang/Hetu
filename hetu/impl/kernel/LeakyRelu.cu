#include "hetu/core/ndarray.h"
#include "hetu/impl/stream/CUDAStream.h"
#include "hetu/impl/utils/common_utils.h"
#include "hetu/impl/utils/cuda_utils.h"
#include "hetu/impl/utils/offset_calculator.cuh"

namespace hetu {
namespace impl {

template <typename spec_t>
__global__ void leaky_relu_kernel(const spec_t* input, spec_t alpha,
                                  size_t size, spec_t* output,
                                  const OffsetCalculator* in_offset_calculator,
                                  const OffsetCalculator* out_offset_calculator) {
  auto idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= size)
    return;
  auto in_offset = in_offset_calculator->get(idx);
  auto out_offset = out_offset_calculator->get(idx);
  output[out_offset] = input[in_offset];
  if (double(input[in_offset]) < 0)
    output[out_offset] *= alpha;
}

template <typename spec_t>
__global__ void leaky_relu_gradient_kernel(const spec_t* input, const spec_t* output_grad,
                                           spec_t alpha_reciprocal, size_t size, spec_t* output,
                                           const OffsetCalculator* in_offset_calculator,
                                           const OffsetCalculator* out_grad_offset_calculator,
                                           const OffsetCalculator* out_offset_calculator) {
  auto idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= size)
    return;
  auto in_offset = in_offset_calculator->get(idx);
  auto out_grad_offset = out_grad_offset_calculator->get(idx);
  auto out_offset = out_offset_calculator->get(idx);
  output[out_offset] = output_grad[out_grad_offset];
  if (double(input[in_offset]) < 0)
    output[out_offset] *= alpha_reciprocal;
}

void LeakyReluCuda(const NDArray& input, double alpha, NDArray& output,
                   const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, output);
  HT_ASSERT_SAME_SHAPE(input, output);

  size_t size = output->numel();
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
  HT_DISPATCH_INTEGER_AND_FLOATING_TYPES(
    input->dtype(), spec_t, "LeakyReluCuda", [&]() {
      leaky_relu_kernel<spec_t><<<blocks, threads, 0, cuda_stream>>>(
        input->data_ptr<spec_t>(), alpha, size, output->data_ptr<spec_t>(),
        in_offset_calculator, out_offset_calculator);
    });
  NDArray::MarkUsedBy({input, output, in_offset_calculator_arr,
                      out_offset_calculator_arr}, stream);
}

void LeakyReluGradientCuda(const NDArray& input, const NDArray& output_grad,
                           double alpha, NDArray& input_grad,
                           const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, output_grad);
  HT_ASSERT_SAME_DEVICE(input, input_grad);

  size_t size = input_grad->numel();
  if (size == 0)
    return;
  dim3 blocks, threads;
  threads.x = MIN(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  blocks.x = DIVUP(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  CUDAStream cuda_stream(stream);
  hetu::cuda::CUDADeviceGuard guard(cuda_stream.device_id());
  NDArray in_offset_calculator_arr, out_grad_offset_calculator_arr,
          in_grad_offset_calculator_arr;
  OffsetCalculator *in_offset_calculator, *out_grad_offset_calculator,
                   *in_grad_offset_calculator;
  std::tie(in_offset_calculator_arr, in_offset_calculator) =
    AllocOffsetCalculator(input, stream);
  std::tie(out_grad_offset_calculator_arr, out_grad_offset_calculator) = 
    AllocOffsetCalculator(output_grad, stream);
  std::tie(in_grad_offset_calculator_arr, in_grad_offset_calculator) = 
    AllocOffsetCalculator(input_grad, stream);
  HT_DISPATCH_INTEGER_AND_FLOATING_TYPES(
    input->dtype(), spec_t, "LeakyReluGradientCuda", [&]() {
      leaky_relu_gradient_kernel<spec_t><<<blocks, threads, 0, cuda_stream>>>(
        input->data_ptr<spec_t>(), output_grad->data_ptr<spec_t>(),
        static_cast<spec_t>(alpha), size, input_grad->data_ptr<spec_t>(),
        in_offset_calculator, out_grad_offset_calculator, in_grad_offset_calculator);
    });
  NDArray::MarkUsedBy({input, output_grad, input_grad, in_offset_calculator_arr,
                      out_grad_offset_calculator_arr, in_grad_offset_calculator_arr}, stream);
}

} // namespace impl
} // namespace hetu
