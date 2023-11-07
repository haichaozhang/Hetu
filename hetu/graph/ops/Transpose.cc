#include "hetu/graph/ops/Transpose.h"
#include "hetu/graph/headers.h"
#include "hetu/graph/ops/kernel_links.h"

namespace hetu {
namespace graph {

void TransposeOpImpl::DoCompute(Operator& op, 
                                const NDArrayList& inputs, NDArrayList& outputs,
                                RuntimeContext& ctx) const {
  HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), type(),
                                  hetu::impl::Transpose, inputs.at(0),
                                  outputs.at(0), get_perms(), op->instantiation_ctx().stream());
}

TensorList TransposeOpImpl::DoGradient(Operator& op, const TensorList& grad_outputs) const {
  const auto& perm = get_perms();
  HTShape grad_perm = perm;
  for (size_t i = 0; i < perm.size(); ++i) {
    grad_perm[perm[i]] = i;
  }
  return {op->requires_grad(0) ? MakeTransposeOp(grad_outputs.at(0), grad_perm,
                                op->grad_op_meta().set_name(op->grad_name()))
                              : Tensor()};
}

HTShapeList TransposeOpImpl::DoInferShape(Operator& op, 
                                          const HTShapeList& input_shapes, 
                                          RuntimeContext& ctx) const {
  HTShape ori_shape = input_shapes.at(0);
  const auto& perm = get_perms();
  HT_ASSERT(perm.size() == ori_shape.size());
  int ndim = perm.size();
  HTShape vis(ndim);
  for (int i = 0; i < ndim; ++i) {
    HT_ASSERT(perm[i] < ndim);
    HT_ASSERT(vis[perm[i]] == 0);
    vis[perm[i]]++;
  }
  HTShape res_shape = ori_shape;
  for (int i = 0; i < ndim; ++i) {
    res_shape[i] = ori_shape[perm[i]];
  }
  return {res_shape};
}

void TransposeOpImpl::DoDeduceStates(const TensorList& inputs, TensorList& outputs, 
                                     const OpMeta& op_meta) const {
  HTShape perm = get_perms();
  const DistributedStates& ds_input = inputs.at(0)->get_distributed_states();
  HT_ASSERT(ds_input.is_valid()) 
    << "TransposeOpDef: distributed states for input must be valid!";
  HT_ASSERT(ds_input.get_dim(-2) == 1)
    << "Input tensor shouldn't be partial!";
  std::unordered_map<int32_t, int32_t> states = ds_input.get_states();
  std::vector<int32_t> order = ds_input.get_order();
  int32_t device_num = ds_input.get_device_num();
  auto get_perm_index = [&](int32_t key) -> int32_t {
    for (int i = 0; i < perm.size(); i++) {
      if (perm[i] == key) {
        return i;
      }
    }
    return -1;
  };
  // states
  std::unordered_map<int32_t, int32_t> new_states;
  for (auto& pair: states) {
    if (pair.first >= 0) {
      new_states[get_perm_index(pair.first)] = pair.second;
    } else {
      new_states[pair.first] = pair.second;
    }
  }
  // order
  std::vector<int32_t> new_order;
  for (auto o : order) {
    if (o >= 0) {
      new_order.push_back(get_perm_index(o));
    } else {
      new_order.push_back(o);
    }
  }
  outputs.at(0)->set_distributed_states({device_num, new_states, new_order});     
}

Tensor MakeTransposeOp(Tensor input, HTShape perms, OpMeta op_meta) {
  return Graph::MakeOp(
    std::make_shared<TransposeOpImpl>(perms),
    {std::move(input)},
    std::move(op_meta))->output(0);
}

} // namespace graph
} // namespace hetu
