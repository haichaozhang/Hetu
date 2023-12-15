#include "hetu/graph/ops/matmul.h"
#include "hetu/graph/headers.h"
#include "hetu/graph/ops/kernel_links.h"
#include <numeric>

namespace hetu {
namespace graph {

TensorList MatMulOpImpl::DoGradient(Operator& op,
                                    const TensorList& grad_outputs) const {
  const Tensor& grad_c = grad_outputs.at(0);
  Tensor& a = op->input(0);
  Tensor& b = op->input(1);
  Tensor grad_a;
  Tensor grad_b;
  auto grad_a_op_meta = op->grad_op_meta().set_name(op->grad_name(0));
  auto grad_b_op_meta = op->grad_op_meta().set_name(op->grad_name(1));
  if (!trans_a() && !trans_b()) {
    // case 1: c = MatMul(a, b)
    // grad_a = MatMul(grad_c, b^T), grad_b = MatMul(a^T, grad_c)
    grad_a = op->requires_grad(0) ? MakeMatMulGradientOp(grad_c, b, a, 0, false, true, std::move(grad_a_op_meta))
                                 : Tensor();
    grad_b = op->requires_grad(1) ? MakeMatMulGradientOp(a, grad_c, b, 1, true, false, std::move(grad_b_op_meta))
                                 : Tensor();
  } else if (trans_a() && !trans_b()) {
    // case 2: c = MatMul(a^T, b)
    // grad_a = MatMul(b, grad_c^T), grad_b = MatMul(a, grad_c)
    grad_a = op->requires_grad(0) ? MakeMatMulGradientOp(b, grad_c, a, 1, false, true, std::move(grad_a_op_meta))
                                 : Tensor();
    grad_b = op->requires_grad(1) ? MakeMatMulGradientOp(a, grad_c, b, 1, false, false, std::move(grad_b_op_meta))
                                 : Tensor();
  } else if (!trans_a() && trans_b()) {
    // case 3: c = MatMul(a, b^T)
    // grad_a = MatMul(grad_c, b), grad_b = MatMul(grad_c^T, a)
    grad_a = op->requires_grad(0) ? MakeMatMulGradientOp(grad_c, b, a, 0, false, false, std::move(grad_a_op_meta))
                                 : Tensor();
    grad_b = op->requires_grad(1) ? MakeMatMulGradientOp(grad_c, a, b, 0, true, false, std::move(grad_b_op_meta))
                                 : Tensor();
  } else {
    // case 4: c = MatMul(a^T, b^T)
    // grad_a = MatMul(b^T, grad_c^T), grad_b = MatMul(grad_c^T, a^T)
    grad_a = op->requires_grad(0) ? MakeMatMulGradientOp(b, grad_c, a, 1, true, true, std::move(grad_a_op_meta))
                                 : Tensor();
    grad_b = op->requires_grad(1) ? MakeMatMulGradientOp(grad_c, a, b, 0, true, true, std::move(grad_b_op_meta))
                                 : Tensor();
  }
  return {grad_a, grad_b};
}

// TODO: support states deduce for different input shape
DistributedStates MatMulDeduceStates(Tensor a, Tensor b, bool trans_a, bool trans_b) {
  HT_ASSERT(a->ndim() == 2 && b->ndim() == 2)
    << "Now only support 2-dimensional distributed matmul! "
    << "got a.ndim = " << a->ndim() << ", b.ndim = " << b->ndim();
  const DistributedStates& ds_a = a->get_distributed_states();
  const DistributedStates& ds_b = b->get_distributed_states();
  int32_t device_num = ds_a.get_device_num();
  HT_ASSERT(ds_a.is_valid() && ds_b.is_valid() && ds_a.get_device_num() == ds_b.get_device_num())
            << "MatMul: cannot convert src distributed states to unpaired dst distributed states!"
            << "got " << a << ": ds_a = " << ds_a.ds_info() << "; " << b << ": ds_b = " << ds_b.ds_info();
  std::vector<std::unordered_map<int32_t, int32_t>> l2res_case({
    {{-1, 1}, {0, 0}, {1, -2}}, // no trans
    {{-1, 1}, {1, 0}, {0, -2}}  // trans A
  });
  auto& l2res_map = l2res_case[trans_a];
  std::vector<std::unordered_map<int32_t, int32_t>> r2res_case({
    {{-1, 0}, {0, -2}, {1, 1}}, // no trans
    {{-1, 0}, {0, 1}, {1, -2}}  // trans A
  });
  auto& r2res_map = r2res_case[trans_b];
  // deduce states
  int32_t lrow = ds_a.get_dim(trans_a);
  int32_t lcol = ds_a.get_dim(1-trans_a);
  int32_t rrow = ds_b.get_dim(trans_b);
  int32_t rcol = ds_b.get_dim(1-trans_b);
  HT_ASSERT(lcol == rrow) << "MatMul: tensor a.dimension[1] " << lcol 
                << " must be equal to tensor b.dimension[0] " << rrow;
  std::unordered_map<int32_t, int32_t> res_states({
    {-2, lcol}, {-1, device_num/(lcol*lrow*rcol)}, {0, lrow}, {1, rcol}
  });
  // deduce order
  std::vector<int32_t> lorder = ds_a.get_order();
  std::vector<int32_t> rorder = ds_b.get_order();
  auto get_new_order = [](std::unordered_map<int32_t, int32_t>& _map,
  std::vector<int32_t>& _order) -> std::vector<int32_t> {
    std::vector<int32_t> new_order;
    for (int32_t x : _order) {
      new_order.push_back(_map[x]);
    }
    return new_order;
  };
  auto get_index = [](std::vector<int32_t>& _order, int32_t val) -> int32_t {
    auto it = std::find(_order.begin(), _order.end(), val);
    HT_ASSERT(it != _order.end()) << "dimension " << val << " is not in order!";
    return it - _order.begin();
  };
  auto new_lorder = get_new_order(l2res_map, lorder);
  auto new_rorder = get_new_order(r2res_map, rorder);
  if (new_lorder != new_rorder) {
    new_lorder[get_index(new_lorder, 1)] = -1;
    new_rorder[get_index(new_rorder, 0)] = -1;
    HT_ASSERT(new_lorder == new_rorder) << "new_lorder is not equal to new_rorder!";
  } else if (std::find(new_lorder.begin(), new_lorder.end(), 0) != new_lorder.end()
             && ds_a.get_dim(-1) > 1) {
    int32_t ind0 = get_index(new_lorder, 0);
    int32_t ind1 = get_index(new_lorder, 1);
    if (ind0 > ind1) {
      int32_t tmp = ind0;
      ind0 = ind1;
      ind1 = tmp;
    }
    HT_ASSERT(ind0 + 1 == ind1) << "ind0 + 1 != ind1";
    new_lorder.insert(new_lorder.begin() + ind1, -1);
  }
  std::vector<int32_t> res_order(new_lorder);
  // set distributed states for result c
  DistributedStates ds_c({device_num, res_states, res_order});
  return ds_c;
}

void MatMulOpImpl::DoDeduceStates(const TensorList& inputs, TensorList& outputs, 
                                  const OpMeta& op_meta) const {
  const Tensor& a = inputs.at(0);
  const Tensor& b = inputs.at(1);
  DistributedStates ds_c = MatMulDeduceStates(a, b, trans_a(), trans_b());
  outputs.at(0)->set_distributed_states(ds_c);
}

void MatMulGradientOpImpl::DoCompute(Operator& op, const NDArrayList& inputs, NDArrayList& outputs,
                                     RuntimeContext& runtime_ctx) const {
  const auto a = inputs.at(0);
  const auto b = inputs.at(1);
  const auto dim_a = a->ndim();
  const auto dim_b = b->ndim();
  if (dim_a == 0 || dim_b == 0) {
    outputs.at(0) = a * b;
  } else if (dim_b == 1 && trans_b()) {
    auto a_shape = a->shape();
    auto a_ = a;
    if (dim_a >= 2 && trans_a()) {
      std::iter_swap(a_shape.end() - 2, a_shape.end() - 1);
      a_ = NDArray::empty(a_shape, a->device(), a->dtype(), op->instantiation_ctx().stream_index);
      auto ndims_a_ = HTAxes(dim_a);
      std::iota(ndims_a_.begin(), ndims_a_.end(), 0);
      std::iter_swap(ndims_a_.end() - 2, ndims_a_.end() - 1);
      a_ = NDArray::permute(a, ndims_a_, op->instantiation_ctx().stream_index);
    }
    NDArray::matmul(NDArray::unsqueeze(a_, dim_a), NDArray::unsqueeze(b, 0), false, false,
                    op->instantiation_ctx().stream_index, outputs.front());
  } else {
    NDArray unreduced;
    unreduced = NDArray::matmul(a, b, trans_a(), trans_b(),
                    op->instantiation_ctx().stream_index, unreduced);
    const auto grad = inputs.at(grad_idx());
    const auto dst = inputs.at(2);
    const auto dim_grad = grad->ndim();
    const auto dim_dst = dst->ndim();
    if (dim_grad > dim_dst) {
      auto reduce_dims = HTAxes(dim_grad - dim_dst);
      std::iota(reduce_dims.begin(), reduce_dims.end(), 0);
      HT_DISPATCH_KERNEL_CUDA_ONLY(op->instantiation_ctx().placement.type(), type(),
                                      hetu::impl::ReduceSum, unreduced, outputs.front(),
                                      reduce_dims.data(), reduce_dims.size(),
                                      op->instantiation_ctx().stream());
    } else {
      NDArray::copy(unreduced, op->instantiation_ctx().stream_index, outputs.front());
    }
  }
}

void MatMulGradientOpImpl::DoDeduceStates(const TensorList& inputs, TensorList& outputs, 
                                          const OpMeta& op_meta) const {
  const Tensor& a = inputs.at(0);
  const Tensor& b = inputs.at(1);
  DistributedStates ds_c = MatMulDeduceStates(a, b, trans_a(), trans_b());
  outputs.at(0)->set_distributed_states(ds_c);
}

Tensor MakeMatMulOp(Tensor a, Tensor b, bool trans_a, bool trans_b,
                    OpMeta op_meta) {
  TensorList inputs = {std::move(a), std::move(b)};
  return Graph::MakeOp(std::make_shared<MatMulOpImpl>(trans_a, trans_b),
                       std::move(inputs), std::move(op_meta))
    ->output(0);
}

Tensor MakeMatMulGradientOp(Tensor a, Tensor b, Tensor dst, int grad_idx,
                            bool trans_a, bool trans_b, OpMeta op_meta) {
  TensorList inputs = {std::move(a), std::move(b), std::move(dst)};
  return Graph::MakeOp(std::make_shared<MatMulGradientOpImpl>(trans_a, trans_b, grad_idx),
                      std::move(inputs), std::move(op_meta))
    ->output(0);
}

} // namespace graph
} // namespace hetu
