#pragma once

#include "hetu/graph/operator.h"
#include "hetu/graph/utils/tensor_utils.h"

namespace hetu {
namespace graph {

class OnehotOpImpl;
class OnehotOp;

class OnehotOpImpl : public OpInterface {
 public:
  OnehotOpImpl(size_t num_classes)
  : OpInterface(quote(OnehotOp)), _classes(num_classes) {
  }

  size_t num_classes() const {
    return _classes;
  }

 protected:
  std::vector<NDArrayMeta> 
  DoInferMeta(const TensorList& inputs) const override {
    HTShape shape;
    if (inputs[0]->has_shape()) {
      shape = inputs[0]->shape();
      shape.emplace_back(num_classes());
    }
    NDArrayMeta output_meta =  NDArrayMeta().set_dtype(inputs[0]->dtype())
                                            .set_shape(shape)
                                            .set_device(inputs[0]->device());
    return {output_meta};
  };

  void DoCompute(Operator& op, const NDArrayList& inputs, NDArrayList& outputs,
                 RuntimeContext& ctx) const override;

  TensorList DoGradient(Operator& op, const TensorList& grad_outputs) const override;

  HTShapeList DoInferShape(Operator& op, const HTShapeList& input_shapes, RuntimeContext& ctx) const override;

  size_t _classes;

 public:
  bool operator==(const OpInterface& rhs) const override {
    if (OpInterface::operator==(rhs)) {
      const auto& rhs_ = reinterpret_cast<const OnehotOpImpl&>(rhs);
      return (num_classes() == rhs_.num_classes());
    }
    return false;
  }
};

Tensor MakeOnehotOp(Tensor input, size_t num_classes, const OpMeta& op_meta = OpMeta());

} // namespace graph
} // namespace hetu
