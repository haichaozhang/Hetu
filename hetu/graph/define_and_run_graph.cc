#include "hetu/graph/define_and_run_graph.h"
#include "hetu/graph/executable_graph.h"

namespace hetu {
namespace graph {

Operator& DefineAndRunGraph::MakeOpInner(std::shared_ptr<OpInterface> body,
                                         TensorList inputs, OpMeta op_meta) {
  // HT_RUNTIME_ERROR_IF(_instantiated)
  //   << "Cannot make new ops to the " << type() << " graph " << id()
  //   << " since it has been instantiated";
  _check_all_inputs_in_graph(inputs, op_meta.extra_deps);
  _instantiated = false;
  return MakeAndAddOp(std::move(body), std::move(inputs), std::move(op_meta));
}

void DefineAndRunGraph::Instantiate() {
  // if (_instantiated)
  //   this->Clear();
  int64_t idx = rand() % 103516531 + 3;
  if (_exec_graph == nullptr)
    _exec_graph =
        Graph::_make_new_graph<ExecutableGraph>(name() + "_executable" + std::to_string(_next_op_id + idx));
  
  auto get_exec_input = [&](const Tensor& input) -> Tensor {
    auto it = _tensor_to_exec_tensor_mapping.find(input->id());
    HT_RUNTIME_ERROR_IF(it == _tensor_to_exec_tensor_mapping.end())
      << "Cannot find the executable version of Tensor " << input;
    return it->second;
  };

  auto put_exec_output = [&](Tensor& tensor, Tensor& exec_tensor) -> void {
    _tensor_to_exec_tensor_mapping[tensor->id()] = exec_tensor;
  };

  OpRefList topo = topo_order();
  HT_LOG_TRACE << "Instantiating a " << type() << " graph with topo " << topo;
  for (auto& op_ref : topo) {
    auto& op = op_ref.get();
    HT_LOG_TRACE << "Creating an executable version of op " << op;
    TensorList exec_inputs, exec_in_deps;
    if (_op_to_exec_op_mapping.find(op->id()) == _op_to_exec_op_mapping.end()) {
      // HT_LOG_INFO << "Insert:" << op;
      std::tie(exec_inputs, exec_in_deps) =
        Operator::transform_each_input_tensor(op, get_exec_input);

      _op_to_exec_op_mapping[op->id()] = Graph::MakeOp(
        op->_body, std::move(exec_inputs),
        OpMeta().set(op->op_meta()).set_extra_deps(std::move(exec_in_deps)),
        *_exec_graph);
      
      Operator::for_each_output_tensor_pair(op, _op_to_exec_op_mapping[op->id()],
                                            put_exec_output);
    }
  }
  // HT_LOG_INFO << id() << ", INI!, and execute graph id is " << _exec_graph->id();
  _instantiated = true;
}

NDArrayList DefineAndRunGraph::Run(const TensorList& fetches,
                                   const FeedDict& feed_dict) {
  if (!_instantiated)
    Instantiate();
  
  TensorList exec_fetches;
  exec_fetches.reserve(fetches.size());
  for (const auto& fetch : fetches) {
    exec_fetches.push_back(_tensor_to_exec_tensor_mapping[fetch->id()]);
  }
  FeedDict exec_feed_dict;
  exec_feed_dict.reserve(feed_dict.size());
  for (const auto& kv : feed_dict)
    exec_feed_dict[_tensor_to_exec_tensor_mapping[kv.first]->id()] = kv.second;
  auto out = _exec_graph->Run(exec_fetches, exec_feed_dict);
  // this->Clear();
  return out;
}

} // namespace graph
} // namespace hetu