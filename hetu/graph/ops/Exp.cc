#include "hetu/graph/ops/Exp.h"
#include "hetu/graph/ops/Arithmetics.h"
#include "hetu/graph/headers.h"
#include "hetu/graph/ops/kernel_links.h"

namespace hetu {
namespace graph {

NDArrayList ExpOpImpl::DoCompute(Operator& op,
                                 const NDArrayList& inputs,
                                 RuntimeContext& ctx) const {
  NDArrayList outputs = inplace() ? inputs : DoAllocOutputs(op, inputs, ctx);
  NDArray::exp(inputs.at(0), op->instantiation_ctx().stream_index, outputs.at(0));
  return outputs;
}

TensorList ExpOpImpl::DoGradient(Operator& op, const TensorList& grad_outputs) const {
  return {op->requires_grad(0) ? MakeMulElewiseOp(op->output(0), grad_outputs.at(0),
                                 op->grad_op_meta().set_name(op->grad_name()))
                               : Tensor()};
}

HTShapeList ExpOpImpl::DoInferShape(Operator& op, 
                                     const HTShapeList& input_shapes, 
                                     RuntimeContext& ctx) const {
  return {input_shapes.at(0)};
}

Tensor MakeExpOp(Tensor input, OpMeta op_meta) {
  TensorList inputs = {std::move(input)};
  DataType input_type = DataType::FLOAT16;
  AutoCast::Tensor_AutoCast(inputs, input_type);
  return Graph::MakeOp(
        std::make_shared<ExpOpImpl>(false),
        std::move(inputs),
        std::move(op_meta))->output(0);
}

Tensor MakeExpInplaceOp(Tensor input, OpMeta op_meta) {
  TensorList inputs = {std::move(input)};
  DataType input_type = DataType::FLOAT16;
  AutoCast::Tensor_AutoCast(inputs, input_type);
  return Graph::MakeOp(
        std::make_shared<ExpOpImpl>(true),
        std::move(inputs),
        std::move(op_meta))->output(0);
}

} // namespace graph
} // namespace hetu