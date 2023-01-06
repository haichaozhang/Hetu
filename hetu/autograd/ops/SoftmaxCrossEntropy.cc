#include "hetu/autograd/ops/SoftmaxCrossEntropy.h"
#include "hetu/autograd/ops/kernel_links.h"

namespace hetu {
namespace autograd {

using SCEOpDef = SoftmaxCrossEntropyOpDef;
using SCEGradOpDef = SoftmaxCrossEntropyGradientOpDef;

void SCEOpDef::DoCompute(const NDArrayList& inputs, NDArrayList& outputs,
                         RuntimeContext& ctx) {
  HT_DISPATCH_KERNEL_CUDA_ONLY(placement().type(), type(),
                               hetu::impl::SoftmaxCrossEntropy, inputs.at(0),
                               inputs.at(1), outputs.at(0), stream());
}

TensorList SCEOpDef::DoGradient(const TensorList& grad_outputs) {
  auto grad_input =
    SoftmaxCrossEntropyGradientOp(_inputs[0], _inputs[1], grad_outputs.at(0),
                                  grad_op_meta().set_name(grad_name()))
      ->output(0);
  return {grad_input, Tensor()};
}

HTShapeList SCEOpDef::DoInferShape(const HTShapeList& input_shapes) {
  HT_ASSERT_GE(input_shapes.at(0).size(), 2)
    << "Invalid shape for " << type() << ": " << input_shapes.at(0);
  HTShape output_shape = {};
  for (size_t i = 0; i < input_shapes.at(0).size() - 1; ++i) {
    output_shape.emplace_back(input_shapes.at(0)[i]);
  }
  return {output_shape};
}

void SCEGradOpDef::DoCompute(const NDArrayList& inputs, NDArrayList& outputs,
                             RuntimeContext& ctx) {
  if (placement().is_cuda()) {
    hetu::impl::SoftmaxCrossEntropyGradientCuda(
      inputs.at(0), inputs.at(1), inputs.at(2), outputs.at(0), stream());
  }
}

HTShapeList SCEGradOpDef::DoInferShape(const HTShapeList& input_shapes) {
  CheckNumInputsEqual(input_shapes.size());
  HT_ASSERT_GE(input_shapes.at(0).size(), 2)
    << "Invalid shape for " << type() << ": " << input_shapes.at(0);
  return {input_shapes.at(0)};
}

} // namespace autograd
} // namespace hetu