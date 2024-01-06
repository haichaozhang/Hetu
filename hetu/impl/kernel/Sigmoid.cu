#include "hetu/core/ndarray.h"
#include "hetu/impl/stream/CUDAStream.h"
#include "hetu/impl/utils/common_utils.h"
#include "hetu/impl/utils/cuda_utils.h"
#include "hetu/impl/utils/cuda_math.h"
#include "hetu/impl/utils/offset_calculator.cuh"
#include "hetu/impl/kernel/Vectorized.cuh"

namespace hetu {
namespace impl {

void SigmoidCuda(const NDArray& input, NDArray& output, const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, output);
  HT_ASSERT_SAME_SHAPE(input, output);

  size_t size = output->numel();
  if (size == 0)
    return;
  HT_DISPATCH_FLOATING_TYPES(
    input->dtype(), spec_t, "SigmoidCuda", [&]() {
      launch_loop_kernel<spec_t, spec_t>(input, output, size, stream,
                                         [] __device__ (spec_t x) -> spec_t {
                                           return 1.0 / (1.0 + hetu::cuda::cuda_exp(-x));
                                         });
    });
  NDArray::MarkUsedBy({input, output}, stream);
}

void SigmoidGradientCuda(const NDArray& out_grad, const NDArray& output, NDArray& in_grad, const Stream& stream) {
  HT_ASSERT_CUDA_DEVICE(out_grad);
  HT_ASSERT_SAME_DEVICE(out_grad, output);
  HT_ASSERT_SAME_DEVICE(out_grad, in_grad);
  HT_ASSERT_SAME_SHAPE(out_grad, output);
  HT_ASSERT_SAME_SHAPE(out_grad, in_grad);

  size_t size = output->numel();
  if (size == 0)
    return;
  HT_DISPATCH_FLOATING_TYPES(
    out_grad->dtype(), spec_t, "SigmoidGradientCuda", [&]() {
      launch_loop_kernel<spec_t, spec_t, spec_t>(out_grad, output, in_grad, size, stream,
                                                 [] __device__ (spec_t out_grad, spec_t output) -> spec_t {
                                                   return out_grad * output * (1 - output);
                                                });
  });
  NDArray::MarkUsedBy({out_grad, output, in_grad}, stream);
}

} // namespace impl
} // namespace hetu
