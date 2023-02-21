#include "hetu/autograd/ops/MatMul.h"
#include "hetu/autograd/ops/kernel_links.h"
#include "hetu/autograd/distributed_states.h"

namespace hetu {
namespace autograd {

void MatMulOpDef::DoCompute(const NDArrayList& inputs, NDArrayList& outputs,
                            RuntimeContext& ctx) {
  HT_DISPATCH_KERNEL_CUDA_ONLY(placement().type(), type(), hetu::impl::MatMul,
                               inputs.at(0), trans_a(), inputs.at(1), trans_b(),
                               outputs.at(0), stream());
}

TensorList MatMulOpDef::DoGradient(const TensorList& grad_outputs) {
  const Tensor& grad_c = grad_outputs.at(0);
  Tensor& a = _inputs[0];
  Tensor& b = _inputs[1];
  Tensor grad_a;
  Tensor grad_b;
  auto g_op_meta = grad_op_meta();
  if (!trans_a() && !trans_b()) {
    // case 1: c = MatMul(a, b)
    // grad_a = MatMul(grad_c, b^T), grad_b = MatMul(a^T, grad_c)
    grad_a = MatMulOp(grad_c, b, false, true, g_op_meta.set_name(grad_name(0)))
               ->output(0);
    grad_b = MatMulOp(a, grad_c, true, false, g_op_meta.set_name(grad_name(1)))
               ->output(0);
  } else if (trans_a() && !trans_b()) {
    // case 2: c = MatMul(a^T, b)
    // grad_a = MatMul(b, grad_c^T), grad_b = MatMul(a, grad_c)
    grad_a = MatMulOp(b, grad_c, false, true, g_op_meta.set_name(grad_name(0)))
               ->output(0);
    grad_b = MatMulOp(a, grad_c, false, false, g_op_meta.set_name(grad_name(1)))
               ->output(0);
  } else if (!trans_a() && trans_b()) {
    // case 3: c = MatMul(a, b^T)
    // grad_a = MatMul(grad_c, b), grad_b = MatMul(grad_c^T, a)
    grad_a = MatMulOp(grad_c, b, false, false, g_op_meta.set_name(grad_name(0)))
               ->output(0);
    grad_b = MatMulOp(grad_c, a, true, false, g_op_meta.set_name(grad_name(1)))
               ->output(0);
  } else {
    // case 4: c = MatMul(a^T, b^T)
    // grad_a = MatMul(b^T, grad_c^T), grad_b = MatMul(grad_c^T, a^T)
    grad_a = MatMulOp(b, grad_c, true, true, g_op_meta.set_name(grad_name(0)))
               ->output(0);
    grad_b = MatMulOp(grad_c, a, true, true, g_op_meta.set_name(grad_name(1)))
               ->output(0);
  }
  return {grad_a, grad_b};
}

HTShapeList MatMulOpDef::DoInferShape(const HTShapeList& input_shapes) {
  const HTShape& a = input_shapes.at(0);
  const HTShape& b = input_shapes.at(1);
  HT_ASSERT(a.size() == 2 && b.size() == 2 &&
            a.at(trans_a() ? 0 : 1) == b.at(trans_b() ? 1 : 0))
    << "Failed to infer shape for the \"" << type() << "\" operation "
    << "(with name \"" << name() << "\"): "
    << "Invalid input shapes: " << a << " (transpose_a = " << trans_a()
    << ") vs. " << b << " (transpose_b = " << trans_b() << "). ";
  return {{a.at(trans_a() ? 1 : 0), b.at(trans_b() ? 0 : 1)}};
}

void MatMulOpDef::ForwardDeduceStates() {
  Tensor& a = _inputs[0];
  Tensor& b = _inputs[1];
  DistributedStates ds_a = a->get_distributed_states();
  DistributedStates ds_b = b->get_distributed_states();
  int32_t device_num = ds_a.get_device_num();

  HT_ASSERT(ds_a.is_valid() && ds_b.is_valid() && ds_a.get_device_num() == ds_b.get_device_num())
            << "cannot convert src distributed states to unpaired dst distributed states!";  
  std::vector<std::unordered_map<int32_t, int32_t>> l2res_case({
    {{-1, 1}, {0, 0}, {1, -2}}, // no trans
    {{-1, 1}, {1, 0}, {0, -2}}  // trans A
  });
  auto& l2res_map = l2res_case[trans_a()];
  std::vector<std::unordered_map<int32_t, int32_t>> r2res_case({
    {{-1, 0}, {0, -2}, {1, 1}}, // no trans
    {{-1, 0}, {0, 1}, {1, -2}}  // trans A
  });
  auto& r2res_map = r2res_case[trans_b()];
  // deduce states
  int32_t lrow = ds_a.get_dim(trans_a());
  int32_t lcol = ds_a.get_dim(1-trans_a());
  int32_t rrow = ds_b.get_dim(trans_b());
  int32_t rcol = ds_b.get_dim(1-trans_b());
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
  Tensor& c = _outputs[0];
  c->set_distributed_states({device_num, res_states, res_order});
}

DistributedStates MatMulOpDef::BackwardDeduceStates(int32_t index) {
  DistributedStates ds_grad;
  Tensor& c = _outputs[0];
  DistributedStates ds_c = c->get_distributed_states();
  HT_ASSERT(ds_c.is_valid()) << "MatMul: distributed states for output tensor is not valid!";

  if (index < 2) {
    std::vector<std::unordered_map<int32_t, int32_t>> res2gradl_case({
      {{-2, 1}, {0, 0}, {1, -2}, {-1, -1}}, // no trans
      {{-2, 0}, {0, 1}, {1, -2}, {-1, -1}}  // trans A
    });
    auto& res2gradl_map = res2gradl_case[trans_a()];
    std::vector<std::unordered_map<int32_t, int32_t>> res2gradr_case({
      {{-2, 0}, {0, -2}, {1, 1}, {-1, -1}}, // no trans
      {{-2, 1}, {0, -2}, {1, 0}, {-1, -1}}  // trans B
    });
    auto& res2gradr_map = res2gradr_case[trans_b()];
    std::unordered_map<int32_t, int32_t> grad_map = index == 0 ? res2gradl_map : res2gradr_map;
    std::unordered_map<int32_t, int32_t> grad_states;
    std::vector<int32_t> keys({-2, -1, 0, 1});
    for (auto key : keys) {
      grad_states[grad_map[key]] = ds_c.get_dim(key);
    }
    std::vector<int32_t> grad_order;
    for (auto x : ds_c.get_order()) {
      grad_order.push_back(grad_map[x]);
    }
    ds_grad.set_distributed_states({ds_c.get_device_num(), grad_states, grad_order});
  } else {
    // for bias in linear_op ?
    HT_ASSERT(index == 2) << "index must be equal to 2!";
    ;
  }
  return ds_grad;
}
} // namespace autograd
} // namespace hetu
