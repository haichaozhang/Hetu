#include "hetu/core/ndarray.h"
#include "hetu/core/stream.h"
#include "hetu/impl/utils/common_utils.h"
#include "hetu/impl/utils/omp_utils.h"

namespace hetu {
namespace impl {

template <typename spec_t>
void abs_cpu(const spec_t* input, size_t size, spec_t* output) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (size_t idx = 0; idx < size; idx++)
    output[idx] = std::abs(input[idx]);
}

void AbsCpu(const NDArray& input, NDArray& output, const Stream& stream) {
  HT_ASSERT_CPU_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, output);
  HT_ASSERT_EXCHANGABLE(input, output);

  size_t size = input->numel();
  if (size == 0)
    return;
  HT_DISPATCH_INTEGER_AND_FLOATING_TYPES(
    input->dtype(), spec_t, "AbsCpu", [&]() {
      abs_cpu<spec_t>(input->data_ptr<spec_t>(), size,
                      output->data_ptr<spec_t>());
    });
}

} // namespace impl
} // namespace hetu