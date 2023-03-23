#include "hetu/core/ndarray.h"
#include "hetu/core/stream.h"
#include "hetu/impl/utils/common_utils.h"
#include "hetu/impl/utils/omp_utils.h"
#include "cmath"

namespace hetu {
namespace impl {

template <typename spec_t>
void sqrt_cpu(const spec_t* input, size_t size, spec_t* output) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (size_t idx = 0; idx < size; ++idx) {
    output[idx] = 0;
  }
}

template <typename spec_t>
void reciprocal_sqrt_cpu(const spec_t* output_grad, size_t size,
                         spec_t* output) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (size_t idx = 0; idx < size; ++idx) {
    output[idx] = static_cast<spec_t>(1) / std::sqrt(output_grad[idx]);
  }
}

void SqrtCpu(const NDArray& input, NDArray& output, const Stream& stream) {
  HT_ASSERT_CPU_DEVICE(input);
  HT_ASSERT_SAME_DEVICE(input, output);
  HT_ASSERT_EXCHANGABLE(input, output);

  size_t size = output->numel();
  if (size == 0)
    return;
  dnnl::engine eng(dnnl::engine::kind::cpu, 0);
  dnnl::stream engine_stream(eng);
  HT_DISPATCH_INTEGER_AND_FLOATING_TYPES(
    input->dtype(), spec_t, "SqrtCpu", [&]() {
      dnnl::memory::data_type mtype;
      if (input->dtype() == DataType::FLOAT32)
        mtype = dnnl::memory::data_type::f32;
      else 
        mtype = dnnl::memory::data_type::f64;
      auto mat_md = dnnl::memory::desc(input->shape(), mtype, input->stride());
      auto src_mem = dnnl::memory(mat_md, eng);
      auto dst_mem = dnnl::memory(mat_md, eng);
      hetu::omp::write_to_dnnl_memory(input->data_ptr<spec_t>(), src_mem);

      auto Sqrt_pd = dnnl::eltwise_forward::primitive_desc(eng, dnnl::prop_kind::forward_training,
                          dnnl::algorithm::eltwise_sqrt, mat_md, mat_md);
      auto Sqrt = dnnl::eltwise_forward(Sqrt_pd);

      Sqrt.execute(engine_stream,
                   {{DNNL_ARG_SRC, src_mem}, {DNNL_ARG_DST, dst_mem}});
      engine_stream.wait();
      hetu::omp::read_from_dnnl_memory(output->data_ptr<spec_t>(), dst_mem);
    });
}

void ReciprocalSqrtCpu(const NDArray& output_grad, NDArray& input_grad,
                       const Stream& stream) {
  HT_ASSERT_CPU_DEVICE(output_grad);
  HT_ASSERT_SAME_DEVICE(output_grad, input_grad);
  HT_ASSERT_EXCHANGABLE(output_grad, input_grad);

  size_t size = input_grad->numel();
  if (size == 0)
    return;
  dnnl::engine eng(dnnl::engine::kind::cpu, 0);
  dnnl::stream engine_stream(eng);  
  HT_DISPATCH_INTEGER_AND_FLOATING_TYPES(
    input_grad->dtype(), spec_t, "ReciprocalSqrtCpu", [&]() {
      auto mat_md = dnnl::memory::desc(input_grad->shape(), dnnl::memory::data_type::f32, input_grad->stride());
      auto src_mem = dnnl::memory(mat_md, eng);
      auto dst_mem = dnnl::memory(mat_md, eng);
      hetu::omp::write_to_dnnl_memory(output_grad->data_ptr<spec_t>(), src_mem);

      auto Reciprocal_pd = dnnl::eltwise_forward::primitive_desc(eng, dnnl::prop_kind::forward_training,
                           dnnl::algorithm::eltwise_pow, mat_md, mat_md, float(1.0), float(-0.5));
      auto Reciprocal = dnnl::eltwise_forward(Reciprocal_pd);

      Reciprocal.execute(engine_stream,
                        {{DNNL_ARG_SRC, src_mem}, {DNNL_ARG_DST, dst_mem}});
      engine_stream.wait();
      hetu::omp::read_from_dnnl_memory(input_grad->data_ptr<spec_t>(), dst_mem);
    });
}

} // namespace impl
} // namespace hetu
