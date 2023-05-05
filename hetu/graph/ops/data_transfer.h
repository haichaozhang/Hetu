#pragma once

#include "hetu/graph/operator.h"
#include "hetu/graph/utils/tensor_utils.h"

namespace hetu {
namespace graph {

class DataH2DOpImpl final : public OpInterface {
 public:
  DataH2DOpImpl(Device device)
  : OpInterface(quote(DataH2D)), _device(std::move(device)) {}

  uint64_t op_indicator() const noexcept override {
    return HOST_TO_DEVICE_OP;
  }

 protected:
  std::vector<NDArrayMeta>
  DoInferMeta(const TensorList& inputs) const override {
    return {NDArrayMeta().set(inputs.front()->meta()).set_device(device())};
  }

  TensorList DoGradient(Operator& op,
                        const TensorList& grad_outputs) const override;

  bool DoInstantiate(Operator& op, const Device& placement,
                     StreamIndex stream_id) const override;

  HTShapeList DoInferShape(Operator& op, const HTShapeList& input_shapes,
                           RuntimeContext& runtime_ctx) const override {
    return {input_shapes.front()};
  }

  void DoCompute(Operator& op, const NDArrayList& inputs, NDArrayList& outputs,
                 RuntimeContext& runtime_ctx) const override;

 public:
  bool operator==(const OpInterface& rhs) const override {
    if (OpInterface::operator==(rhs)) {
      const auto& rhs_ = reinterpret_cast<const DataH2DOpImpl&>(rhs);
      return device() == rhs_.device();
    }
    return false;
  }

  const Device& device() const {
    return _device;
  }

 protected:
  Device _device;
};

class DataD2HOpImpl final : public OpInterface {
 public:
  DataD2HOpImpl(Device device)
  : OpInterface(quote(DataD2H)), _device(std::move(device)) {}

  uint64_t op_indicator() const noexcept override {
    return DEVICE_TO_HOST_OP;
  }

 protected:
  std::vector<NDArrayMeta>
  DoInferMeta(const TensorList& inputs) const override {
    return {NDArrayMeta().set(inputs.front()->meta()).set_device(device())};
  }

  TensorList DoGradient(Operator& op,
                        const TensorList& grad_outputs) const override;

  bool DoInstantiate(Operator& op, const Device& placement,
                     StreamIndex stream_id) const override;

  HTShapeList DoInferShape(Operator& op, const HTShapeList& input_shapes,
                           RuntimeContext& runtime_ctx) const override {
    return {input_shapes.front()};
  }

  void DoCompute(Operator& op, const NDArrayList& inputs, NDArrayList& outputs,
                 RuntimeContext& runtime_ctx) const override;

 public:
  bool operator==(const OpInterface& rhs) const override {
    if (OpInterface::operator==(rhs)) {
      const auto& rhs_ = reinterpret_cast<const DataD2HOpImpl&>(rhs);
      return device() == rhs_.device();
    }
    return false;
  }

  const Device& device() const {
    return _device;
  }

 protected:
  Device _device;
};

Tensor MakeDataH2DOp(Device device, Tensor input, OpMeta op_meta = OpMeta());

Tensor MakeDataD2HOp(Device device, Tensor input, OpMeta op_meta = OpMeta());

} // namespace graph
} // namespace hetu