#include "hetu/core/ndarray.h"
#include "hetu/impl/stream/CUDAStream.h"
#include "hetu/impl/utils/common_utils.h"
#include "hetu/impl/utils/cuda_utils.h"

namespace hetu {
namespace impl {

template <typename spec_t>
__global__ void embedding_lookup_kernel(const spec_t* input, const spec_t* ids,
                                        size_t size, size_t length,
                                        size_t input_row, spec_t* output) {
  auto idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= size)
    return;
  int id = ids[idx];
  spec_t* output_ptr = output + length * idx;
  if (id < 0 || id >= input_row) {
    for (int i = 0; i < length; i++)
      output_ptr[i] = 0;
  } else {
    const spec_t* input_ptr = input + length * id;
    for (int i = 0; i < length; i++)
      output_ptr[i] = input_ptr[i];
  }
}

template <typename spec_t>
__global__ void array_zero_set_kernel(spec_t* input, size_t size) {
  auto idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= size)
    return;
  input[idx] = 0;
}

template <typename spec_t>
__global__ void embedding_lookup_gradient_kernel(const float* output_grad,
                                                 const float* ids, size_t size,
                                                 size_t length,
                                                 float* input_grad) {
  auto idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= size)
    return;
  int id = ids[idx];
  const float* output_grad_ptr = output_grad + length * idx;
  float* input_grad_ptr = input_grad + length * id;
  for (int i = 0; i < length; i++)
    atomicAdd(input_grad_ptr + i, *(output_grad_ptr + i));
}

void EmbeddingLookupCuda(const NDArray& input, const NDArray& id,
                         NDArray& output, const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, id);
  HT_ASSERT_SAME_DEVICE(input, output);
  HT_ASSERT(input->ndim() == 2)
    << "input_dim is invalid.Expect 2,but get " << input->ndim();

  for (int i = 0; i < output->ndim(); i++) {
    if (i < output->ndim() - 1) {
      HT_ASSERT(id->shape(i) == output->shape(i));
    } else if (i == output->ndim() - 1) {
      HT_ASSERT(input->shape(1) == output->shape(i));
    }
  }
  size_t input_row = input->shape(0);
  size_t length = input->shape(1);
  size_t size = id->numel();
  if (size == 0 || input_row == 0 || length == 0)
    return;
  dim3 blocks, threads;
  threads.x = MIN(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  blocks.x = DIVUP(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  CUDAStream cuda_stream(stream);
  hetu::cuda::CUDADeviceGuard guard(cuda_stream.device_id());
  HT_DISPATCH_INTEGER_AND_FLOATING_TYPES(
    input->dtype(), spec_t, "EmbbedingLookupCuda", [&]() {
      embedding_lookup_kernel<spec_t><<<blocks, threads, 0, cuda_stream>>>(
        input->data_ptr<spec_t>(), id->data_ptr<spec_t>(), size, length,
        input_row, output->data_ptr<spec_t>());
    });
}

void EmbeddingLookupGradientCuda(const NDArray& output_grad, const NDArray& id,
                                 NDArray& input_grad, const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(output_grad);
  HT_ASSERT_SAME_DEVICE(output_grad, id);
  HT_ASSERT_SAME_DEVICE(output_grad, input_grad);
  HT_ASSERT(input_grad->ndim() == 2)
    << "input_dim is invalid.Expect 2,but get " << input_grad->ndim();

  for (int i = 0; i < output_grad->ndim(); i++) {
    if (i < output_grad->ndim() - 1) {
      HT_ASSERT(id->shape(i) == output_grad->shape(i));
    } else if (i == output_grad->ndim() - 1) {
      HT_ASSERT(input_grad->shape(1) == output_grad->shape(i));
    }
  }
  size_t length = input_grad->shape(1);
  size_t size = input_grad->numel();
  if (size == 0 || length == 0)
    return;
  dim3 blocks, threads;
  threads.x = MIN(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  blocks.x = DIVUP(size, HT_DEFAULT_NUM_THREADS_PER_BLOCK);
  CUDAStream cuda_stream(stream);
  hetu::cuda::CUDADeviceGuard guard(cuda_stream.device_id());
  HT_DISPATCH_INTEGER_AND_FLOATING_TYPES(
    input_grad->dtype(), spec_t, "ArrayZeroSet", [&]() {
      array_zero_set_kernel<spec_t><<<blocks, threads, 0, cuda_stream>>>(
        input_grad->data_ptr<spec_t>(), size);
    });
  HT_DISPATCH_INTEGER_AND_FLOATING_TYPES(
    input_grad->dtype(), spec_t, "EmbeddingLookupGradientCuda", [&]() {
      embedding_lookup_gradient_kernel<float>
        <<<blocks, threads, 0, cuda_stream>>>(
          output_grad->data_ptr<float>(), id->data_ptr<float>(), size, length,
          input_grad->data_ptr<float>());
    });
}

} // namespace impl
} // namespace hetu