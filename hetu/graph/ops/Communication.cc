#include "hetu/graph/ops/Communication.h"
#include "hetu/graph/headers.h"
#include "hetu/graph/ops/kernel_links.h"
#include "hetu/impl/communication/nccl_comm_group.h"
#include "hetu/impl/communication/comm_group.h"
#include "hetu/core/symbol.h"
#include <numeric>

namespace hetu {
namespace graph {

using namespace hetu::impl::comm;

std::ostream& operator<<(std::ostream& os, const CommOpInfo& info) {
  os << "src group union = " << info.src_group_union
    << " and dst group union = " << info.dst_group_union
    << " and src ds union = " << info.src_ds_union.ds_union_info()
    << " and dst ds union = " << info.dst_ds_union.ds_union_info()
    << " and union idx = " << info.union_idx;
  return os;
}

CommOpInfo CommOpImpl::get_comm_info(Operator& op, const Device& inferred) const {
  Tensor& input = op->input(0);
  auto& graph = input->graph();
  HT_ASSERT(op->has_placement_group())
    << "get_comm_info should be called after DoMapToParallelDevices";
  // 这里一个比较tricky的是union中选哪个ds会依赖于具体的device
  // 但可能存在device不在placement group的情形
  // 这种情况下返回默认的第一个ds也没有关系
  // 因为该op最终不会出现在local topo中
  const auto& src_group_union = get_src_group_union(op);
  const auto& dst_group_union = get_dst_group_union(op);
  auto src_ds_union = input->cur_ds_union();
  auto dst_ds_union = get_dst_ds_union(op);
  HT_ASSERT(src_ds_union.size() == src_group_union.size() && dst_ds_union.size() == dst_group_union.size())
    << "Size of unions should be equal";
  size_t union_idx = 0;
  int32_t placement_pos = -1;
  if (op->placement_group_union().has(inferred)) {
    union_idx = op->placement_group_union().get_index(inferred);
  }
  const auto& src_group = src_group_union.get(union_idx);
  const auto& dst_group = dst_group_union.get(union_idx);
  const auto& src_ds = src_ds_union.get(union_idx);
  const auto& dst_ds = dst_ds_union.get(union_idx);
  if (dst_group.contains(inferred) && !src_group.contains(inferred)) {
    placement_pos = 0;
  } else if (src_group.contains(inferred) && !dst_group.contains(inferred)) {
    placement_pos = 1;
  } else if (src_group.contains(inferred) && dst_group.contains(inferred)) {
    placement_pos = 2;
  } else {
    placement_pos = -1;
  }
  // 一对多和多对一目前会在hetero dim上将二者尝试对齐
  // 其解决的主要是不同tp组间的activation通信
  if (src_ds_union.is_hetero() && !dst_ds_union.is_hetero()) {
    dst_ds_union = dst_ds_union.to_hetero(src_ds_union.hetero_dim(), src_ds_union.size());
  } 
  else if (!src_ds_union.is_hetero() && dst_ds_union.is_hetero()) {
    src_ds_union = src_ds_union.to_hetero(dst_ds_union.hetero_dim(), dst_ds_union.size());
  } 
  // 多对多情形则只要求union size一样
  // hetero dim可以不同
  else if (src_ds_union.is_hetero() && dst_ds_union.is_hetero()) {
    HT_ASSERT(src_ds_union.size() == dst_ds_union.size())
      << "Hetero size should be equal for src ds union and dst ds union";
  } 
  // 其余一对一情形
  else {
    // double check
    HT_ASSERT(src_ds_union.hetero_dim() == dst_ds_union.hetero_dim() && src_ds_union.hetero_dim() == NULL_HETERO_DIM)
      << "Double check fault";
    HT_ASSERT(src_ds_union.size() == dst_ds_union.size() && src_ds_union.size() == 1)
      << "Double check fault";
  }
  return CommOpInfo(src_group_union, dst_group_union, src_ds_union, dst_ds_union, union_idx, placement_pos);
}

uint64_t CommOpImpl::get_comm_type(Operator& op, const Device& inferred, const CommOpInfo& comm_info) {
  CommOpInfo info = comm_info.is_empty ? get_comm_info(op, inferred) : comm_info;
  // input may be inplaced, so comm_type should be updated for each call
  // 下面对不同情形进行处理
  // 1、hetero dim一样
  // 包括都是homo的情形，即hetero dim是NULL_HETERO_DIM
  // 这种情况下只用关心src ds和dst ds
  // 因为不会产生union内跨ds的事情
  if (info.src_ds_union.hetero_dim() == info.dst_ds_union.hetero_dim()) {
    // 1-1、src ds和dst ds一样
    // inter op (pp)
    if (info.src_ds.check_equal(info.dst_ds)) {
      if (info.src_group != info.dst_group) {
        HT_ASSERT(info.src_group.num_devices() == info.dst_group.num_devices())
          << "Something wrong in deducing ds union or dg union";
        _comm_type = P2P_OP; 
        HT_LOG_DEBUG << "P2P_OP";
      } else {
        HT_ASSERT(info.src_group_union.check_equal(info.dst_group_union))
          << "Something wrong in deducing ds union or dg union";
        _comm_type = UNUSED_OP; 
        HT_LOG_DEBUG << "UNUSED_OP";
      }
    } 
    // 1-2、src ds和dst ds不一样
    else {
      // 1-2-1、src group和dst group一样
      // intra op (tp)
      if (info.src_group == info.dst_group) {
        if (info.src_ds.check_pure_duplicate()) {
          _comm_type = COMM_SPLIT_OP;
          HT_LOG_DEBUG << "COMM_SPLIT_OP";
        } else if (info.src_ds.check_allreduce(info.dst_ds)) {
          _comm_type = ALL_REDUCE_OP;
          HT_LOG_DEBUG << "ALL_REDUCE_OP";
        } else if (info.src_ds.check_allgather(info.dst_ds)) {
          _comm_type = ALL_GATHER_OP;
          HT_LOG_DEBUG << "ALL_GATHER_OP";
        } else if (info.src_ds.check_reducescatter(info.dst_ds)) {
          _comm_type = REDUCE_SCATTER_OP;
          HT_LOG_DEBUG << "REDUCE_SCATTER_OP";
        } else {
          HT_RUNTIME_ERROR << "Not supported yet";
        }
      } 
      // 1-2-2、src group和dst group不一样
      // intra + inter op (tp + pp)
      else {
        // 目前只支持all to all
        // 即src的partial和dst的partial一样
        if (info.src_ds.states(-2) == info.dst_ds.states(-2)) {
          _comm_type = BATCHED_ISEND_IRECV_OP;
          HT_LOG_DEBUG << "BATCHED_ISEND_IRECV_OP";
        } else {
          HT_RUNTIME_ERROR << "Not supported yet";
        }
      }
    }
  }
  // 2、hetero dim不一样
  else {
    HT_ASSERT(info.src_group_union.check_equal(info.dst_group_union))
      << "Currently only support intra-group multi hetero dim comm";
    // DistributedStatesList src_local_ds_list, dst_local_ds_list;
    size_t size = info.src_group_union.size();
    for (size_t i = 0; i < size; i++) {
      auto src_local_ds = info.src_ds_union.get_local(i);
      auto dst_local_ds = info.dst_ds_union.get_local(i);
      HT_ASSERT(src_local_ds.check_equal(dst_local_ds))
        << "Currently only support equal local ds for different src and dst hetero dim";
      HT_ASSERT(src_local_ds.states(0) == 1 || src_local_ds.states(1) == 1)
        << "Currently only support local ds splits on a single dim";
      HT_ASSERT(src_local_ds.states(-2) == 1)
        << op << " has local partial"
        << ", the src ds union is " << info.src_ds_union.ds_union_info()
        << ", and dst ds union is " << info.dst_ds_union.ds_union_info()
        << ", which is not supported yet";
    }
    if (info.src_ds_union.hetero_dim() == -2 && info.dst_ds_union.hetero_dim() == -1) {
      _comm_type = SPLIT_ALL_REDUCE_OP;
      HT_LOG_DEBUG << "SPLIT_ALL_REDUCE_OP";
    } else if (info.src_ds_union.hetero_dim() == -2 && info.dst_ds_union.hetero_dim() == 0) {
      _comm_type = SPLIT_REDUCE_SCATTER_OP;
      HT_LOG_DEBUG << "SPLIT_REDUCE_SCATTER_OP";
    } else if (info.src_ds_union.hetero_dim() == 0 && info.dst_ds_union.hetero_dim() == -1) {
      _comm_type = SPLIT_ALL_GATHER_OP;
      HT_LOG_DEBUG << "SPLIT_ALL_GATHER_OP";
    } else {
      HT_RUNTIME_ERROR << "Currently not supported yet";
    }
  }
  return _comm_type;
}

// devices by dim for collective communication
DeviceGroup CommOpImpl::get_devices_by_dim(Operator& op, int32_t dim) const {
  const auto& placement = op->placement();
  CommOpInfo info = get_comm_info(op, placement);
  const auto& placement_group = info.src_group;
  HT_ASSERT(placement_group.contains(placement))
    << "Func get_devices_by_dim can only be called by device in src group: " 
    << placement_group << ", now get device " << placement << " in dst group!";
  
  int32_t local_device_idx = placement_group.get_index(placement);
  const auto& local_src_ds = info.local_src_ds;
  const auto& order = local_src_ds.get_order();
  const auto& states = local_src_ds.get_states();

  auto idx = std::find(order.begin(), order.end(), dim);
  int32_t interval = 1;
  for (auto cur_order = idx + 1; cur_order != order.end(); cur_order++) {
    interval *= states.at(*cur_order);
  }
  int32_t macro_interval = interval * local_src_ds.get_dim(dim);
  int32_t start = local_device_idx - local_device_idx % macro_interval + local_device_idx % interval;
  std::vector<Device> comm_group;
  for (auto i = start; i < start + macro_interval; i += interval) {
    comm_group.push_back(placement_group.get(i));
  }
  return DeviceGroup(comm_group);
}

// get split num and comm groups for local device
// *currently only support split on a single dim
std::tuple<size_t, DeviceGroupList> CommOpImpl::get_split_comm_groups(Operator& op, const DeviceGroupUnion& dg_union,
                                                                      const DistributedStatesUnion& ds_union) const {
  const auto& placement = op->placement();
  HT_ASSERT(!placement.is_undetermined())
    << "Please ensure you have instantiated the comm op";
  HT_ASSERT(ds_union.is_hetero() && dg_union.size() == ds_union.size())
    << "Please ensure the device group union has an equal size with the ds union and they are hetero";
  HT_ASSERT(dg_union.has(placement))
    << "Please ensure the device group union contains the placement";
  size_t union_size = dg_union.size();
  size_t union_idx = dg_union.get_index(placement);
  DeviceGroup local_dg = dg_union.get(union_idx);
  DistributedStates local_ds = ds_union.get(union_idx);
  DeviceGroupList dg_list;
  DistributedStatesList ds_list;

  int32_t split_dim = -3;
  for (size_t i = 0; i < union_size; i++) {
    const auto& dg = dg_union.get(i);
    const auto ds = ds_union.get_local(i);
    const auto& states = ds.get_states();
    bool flag = false;
    // check
    for (const auto& kv : states) {
      if (kv.second != 1) {
        if (kv.first == -1) {
          continue;
        }
        if (kv.first == -2) {
          HT_RUNTIME_ERROR << "Locad ds has partial, currently not supported";
        }
        if (flag) {
          HT_RUNTIME_ERROR << "Currently only support split on a single dim";
        }
        if (split_dim == -3) {
          split_dim = kv.first;
        }
        HT_ASSERT(split_dim == kv.first)
          << "Currently only support split on the same dim for all ds in the ds union";
        flag = true;
      }
    }
    ds_list.emplace_back(ds);
    dg_list.emplace_back(dg);
  }

  // 退化为不split
  // split dim设置成0即可
  if (split_dim == -3) {
    split_dim = 0;
  }

  DeviceGroupList comm_groups;
  // 在每个策略中，每个tensor在第split_dim维被切分为若干份block，首先确定placement对应的block位置，即block_idx
  int32_t local_device_idx = local_dg.get_index(placement);
  auto state_index = local_ds.map_device_to_state_index(local_device_idx);
  size_t dup_size = local_ds.get_dim(-1);
  size_t dup_idx = 0;
  if (dup_size > 1) {
    dup_idx = state_index.at(-1);
  }
  size_t block_idx = 0;
  if (state_index.find(split_dim) != state_index.end()) {
    block_idx = state_index[split_dim];
  }
  // 获取union中所有ds的tensor切分的最细粒度
  int32_t max_split_num = 0;
  for (const auto& ds : ds_list) {
    max_split_num = std::max(ds.get_dim(split_dim), max_split_num);
  }
  // 对当前ds下的block进一步切分至最细粒度，得到最细粒度下对应的起始micro_block位置，即micro_block_start_idx
  // ps: 一个block中可能有多个micro_block，此处micro_block_start_idx对应当中的第一个micro_block
  size_t micro_block_num = max_split_num / local_ds.get_dim(split_dim);
  size_t micro_block_start_idx = block_idx * micro_block_num;
  for (size_t i = 0; i < micro_block_num; i++) {
    size_t micro_block_idx = micro_block_start_idx + i;
    std::vector<Device> comm_group;
    // 在每个ds中找到micro_block_idx对应的device并组建通信组
    for (size_t j = 0; j < union_size; j++) {
      const auto& cur_ds = ds_list[j];
      size_t cur_micro_block_num = max_split_num / cur_ds.get_dim(split_dim);
      // 在union中第j个ds下micro_block_idx对应的block_idx
      size_t cur_block_idx = micro_block_idx / cur_micro_block_num;
      size_t cur_dup_size = cur_ds.get_dim(-1);
      // workaround
      // 当有多个dup时，采用round robin进行reduce
      size_t cur_dup_idx = 0;
      if (cur_dup_size >= dup_size) {
        cur_dup_idx = dup_idx;
      } else {
        HT_ASSERT(dup_size % cur_dup_size == 0)
          << "dup should be 2 or 4 or 8...";
        cur_dup_idx = dup_idx % cur_dup_size;
      }
      // 找到cur dup idx下cur block idx对应的device
      size_t cur_device_idx = 0;
      while (cur_device_idx < dg_list[j].num_devices()) {
        auto cur_state_index = cur_ds.map_device_to_state_index(cur_device_idx);
        size_t tmp_dup_idx = 0;
        if (cur_dup_size > 1) {
          tmp_dup_idx = cur_state_index.at(-1);
        }
        if (tmp_dup_idx == cur_dup_idx) {
          size_t tmp_block_idx = 0;
          if (cur_state_index.find(split_dim) != cur_state_index.end()) {
            tmp_block_idx = cur_state_index[split_dim];
          }
          if (tmp_block_idx == cur_block_idx) {
            break;
          }
        }
        cur_device_idx++;
      }
      HT_ASSERT(cur_device_idx != dg_list[j].num_devices())
        << "Can't find the device that owns the micro block " << micro_block_idx
        << " in the device group " << dg_list[j];
      comm_group.emplace_back(dg_list[j].get(cur_device_idx));
    }
    comm_groups.emplace_back(DeviceGroup(comm_group));
  }
  HT_LOG_DEBUG << "get comm groups = " << comm_groups << ", split_num = " << micro_block_num;
  return {micro_block_num, comm_groups};
}

void CommOpImpl::DoDeduceStates(const TensorList& inputs, TensorList& outputs, 
                                const OpMeta& op_meta) const {
  const Tensor& input = inputs.at(0);
  Tensor& output = outputs.at(0);
  const auto& ds_input = input->get_distributed_states();
  const auto& ds_dst = get_dst_distributed_states(input->producer());
  // TODO: check states/order between src and dst
  HT_ASSERT(ds_input.is_valid() && ds_dst.is_valid())
          << "distributed states for input and dst tensor must be valid"
          << ", but found ds_input " << ds_input.ds_info()
          << ", and ds dst " << ds_dst.ds_info();
  // now support hetero settings
  // device num could be different
  // sounds cool~
  /*
  HT_ASSERT(ds_input.get_device_num() == ds_dst.get_device_num())
          << "cannot convert src distributed states to unpaired dst distributed states!";
  */
  output->set_distributed_states(ds_dst);
}

void CommOpImpl::DoDeduceHeteroDim(const std::vector<int32_t>& inputs_hetero_dim,
                                   TensorList& outputs, const OpMeta& op_meta) const {
  int32_t hetero_dim = NULL_HETERO_DIM;
  if (_dst_ds_hierarchy.size() == 1) { // for comm op created in exec_graph, without multi ds
    hetero_dim = _dst_ds_hierarchy.get(0).hetero_dim();
  } else { // for comm op created in define_and_run_graph, with multi ds
    hetero_dim = _dst_ds_hierarchy.get(outputs.at(0)->graph().CUR_STRATEGY_ID).hetero_dim();
  }
  outputs.at(0)->cur_ds_union().set_hetero_dim(hetero_dim);
}

bool CommOpImpl::DoMapToParallelDevices(Operator& op,
                                        const DeviceGroupUnion& pg_union) const {
  const DeviceGroupUnion& src_group_union = get_src_group_union(op);
  const DeviceGroupUnion& dst_group_union = pg_union;
  const DistributedStatesUnion& src_ds_union = op->input(0)->cur_ds_union();
  const DistributedStatesUnion& dst_ds_union = get_dst_ds_union(op);
  HT_ASSERT(src_group_union.size() == src_ds_union.size() && dst_group_union.size() == dst_ds_union.size())
    << "Union sizes mismatch";
  DeviceGroupUnion merge_group_union;
  if (src_group_union.size() == 1 && dst_group_union.size() != 1) {
    merge_group_union = DeviceGroupUnion::device_group_to_union(src_group_union.get(0), src_ds_union.get(0), dst_ds_union.hetero_dim(), dst_ds_union.size());
    merge_group_union = DeviceGroupUnion::merge(merge_group_union, dst_group_union);
  } else if (dst_group_union.size() == 1 && src_group_union.size() != 1) {
    merge_group_union = DeviceGroupUnion::device_group_to_union(dst_group_union.get(0), dst_ds_union.get(0), src_ds_union.hetero_dim(), src_ds_union.size());
    merge_group_union = DeviceGroupUnion::merge(src_group_union, merge_group_union);
  } else {
    HT_ASSERT(src_group_union.size() == dst_group_union.size())
      << "Size of src group union and dst group union should be equal";
    merge_group_union = DeviceGroupUnion::merge(src_group_union, dst_group_union);
  }
  op->instantiation_ctx().placement_group_union = merge_group_union;
  op->instantiation_ctx().has_placement_group = true;
  Operator::for_each_output_tensor(
    op, [&](Tensor& tensor) { tensor->set_placement_group_union(dst_group_union); });
  return true;  
}

// unused comm ops have been removed before do intantiate
bool CommOpImpl::DoInstantiate(Operator& op, const Device& placement,
                               StreamIndex stream_index) const {
  CommOpInfo info = get_comm_info(op, placement);
  HT_ASSERT(info.placement_pos != -1)
    << "placement " << placement << " is not in comm op placemnt group union " << op->placement_group_union();
  auto& inst_ctx = op->instantiation_ctx();
  inst_ctx.placement = placement;
  inst_ctx.stream_index = stream_index;
  if (placement.is_cuda()) {
    for (size_t i = 0; i < HT_MAX_NUM_MICRO_BATCHES; i++) { 
      inst_ctx.start[i] = std::make_unique<hetu::impl::CUDAEvent>(placement);
      inst_ctx.stop[i] = std::make_unique<hetu::impl::CUDAEvent>(placement);
    }
  } else {
    for (size_t i = 0; i < HT_MAX_NUM_MICRO_BATCHES; i++) {     
      inst_ctx.start[i] = std::make_unique<hetu::impl::CPUEvent>();
      inst_ctx.stop[i] = std::make_unique<hetu::impl::CPUEvent>();
    }
  }
  Operator::for_each_output_tensor(op, [&](Tensor& tensor) {
    if (info.dst_group.contains(placement)) {
      tensor->set_placement(placement);
    }
  });
  return true;
}

std::vector<NDArrayMeta> 
CommOpImpl::DoInferMeta(const TensorList& inputs) const {
  const Tensor& input = inputs.at(0);
  const HTShape& input_shape = input->shape();
  const DistributedStates& src_ds = input->get_distributed_states();
  input->producer()->graph().USE_HETERO_ID = true;
  const DistributedStates& dst_ds = get_dst_distributed_states(input->producer());
  input->producer()->graph().USE_HETERO_ID = false;
  HTShape shape(input_shape.size());
  /*
  HT_LOG_INFO << "src ds union is " << input->cur_ds_union().ds_union_info()
    << ", src ds is " << src_ds.ds_info()
    << ", dst ds union is " << get_dst_ds_union(input->producer()).ds_union_info()
    << ", dst ds is " << dst_ds.ds_info();
  */
  for (size_t d = 0; d < input_shape.size(); d++) {
    shape[d] = input_shape[d] * src_ds.get_dim(d) / dst_ds.get_dim(d);
  }
  return {NDArrayMeta().set_dtype(input->dtype()).set_device(input->device()).set_shape(shape)};
}


HTShapeList CommOpImpl::DoInferShape(Operator& op, 
                                     const HTShapeList& input_shapes,
                                     RuntimeContext& runtime_ctx) const {
  const HTShape& input_shape = input_shapes.at(0);
  Tensor& input = op->input(0);
  const auto& src_ds = input->get_distributed_states();
  const auto& dst_ds = get_dst_distributed_states(op);
  HTShape shape(input_shape.size());
  HT_LOG_DEBUG << "CommOpImpl::DoInferShape, src_ds = " << src_ds.get_states()
    << " and dst_ds = " << dst_ds.get_states();
  for (size_t d = 0; d < input_shape.size(); d++) {
    shape[d] = input_shape[d] * src_ds.get_dim(d) / dst_ds.get_dim(d);
  }
  HT_LOG_DEBUG << "CommOpImpl::DoInferShape, shape = " << shape;
  return {shape};
}

// support ds hierarchy
TensorList CommOpImpl::DoGradient(Operator& op,
                                  const TensorList& grad_outputs) const {
  // if input not requires grad, then grad_output also will be Tensor()                                    
  if (!op->requires_grad(0))
    return {Tensor()};                                    
  Tensor& input = op->input(0);
  Tensor& output = op->output(0);
  const Tensor& grad_output = grad_outputs.at(0);
  auto& graph = input->graph();
  DistributedStatesHierarchy dst_ds_hierarchy;
  graph.USE_HETERO_ID = true;
  for (size_t cur_strategy_id = 0; cur_strategy_id < graph.NUM_STRATEGY; cur_strategy_id++) {
    graph.CUR_STRATEGY_ID = cur_strategy_id;
    DistributedStatesUnion dst_ds_union;
    int32_t ds_input_hetero_dim = input->cur_ds_union().hetero_dim();
    dst_ds_union.set_hetero_dim(ds_input_hetero_dim == -2 ? -1 : ds_input_hetero_dim);
    size_t hetero_size = std::max(std::max(input->cur_ds_union().size(), output->cur_ds_union().size()), grad_output->cur_ds_union().size());
    for (size_t cur_hetero_id = 0; cur_hetero_id < hetero_size; cur_hetero_id++) {
      graph.CUR_HETERO_ID = cur_hetero_id;
      const auto& ds_input = input->get_distributed_states();
      const auto& ds_output = output->get_distributed_states();
      const auto& ds_grad_output = grad_output->get_distributed_states();
      HT_ASSERT(ds_input.is_valid() && ds_output.is_valid())
              << "distributed states for input and output tensor must be valid!";
      // now support hetero settings
      // device num could be different
      // sounds cool~
      /*
      HT_ASSERT(ds_input.get_device_num() == ds_output.get_device_num())
              << "distributed states for input and output tensor must be matched!";
      */
      HT_ASSERT(ds_output.states(-2) == 1)
              << "partial should already be handled by intermediate comm op during gradient computing!";
      DistributedStates ds_grad_input(ds_input);
      if (ds_grad_input.get_dim(-2) > 1) { // partial->duplicate
        std::pair<std::vector<int32_t>, int32_t> src2dst({{-2}, -1});
        auto res_states = ds_grad_input.combine_states(src2dst);
        auto res_order = ds_grad_input.combine_order(src2dst);
        auto device_num = ds_grad_input.get_device_num();
        ds_grad_input.set_distributed_states({device_num, res_states, res_order});
      }
      dst_ds_union.add(ds_grad_input);
    }
    dst_ds_hierarchy.add(dst_ds_union);
  }
  graph.CUR_STRATEGY_ID = 0;
  graph.CUR_HETERO_ID = 0;
  graph.USE_HETERO_ID = false;
  Tensor grad_input = MakeCommOp(grad_output, dst_ds_hierarchy, OpMeta().set_name("grad_" + op->name()));
  return {grad_input};
}

bool AllReduceOpImpl::DoMapToParallelDevices(Operator& op, 
                                             const DeviceGroupUnion& pg_union) const {
  return OpInterface::DoMapToParallelDevices(op, pg_union);
}

bool AllReduceOpImpl::DoInstantiate(Operator& op, const Device& placement,
                                    StreamIndex stream_index) const {
  bool ret = OpInterface::DoInstantiate(op, placement, stream_index);
  for (int i = 0; i < _comm_group.num_devices(); i++) {
    HT_ASSERT(op->local_placement_group().contains(_comm_group.get(i))) 
      << "AllReduceOp: device in comm_group: " << _comm_group.get(i) 
      << " must in palcement_group: " << op->local_placement_group();
  }
  auto ranks = DeviceGroupToWorldRanks(_comm_group);
  NCCLCommunicationGroup::GetOrCreate(ranks, op->instantiation_ctx().stream());
  return ret;
}

std::vector<NDArrayMeta> 
AllReduceOpImpl::DoInferMeta(const TensorList& inputs) const {
  return {inputs[0]->meta()};
}

HTShapeList AllReduceOpImpl::DoInferShape(Operator& op, 
                                          const HTShapeList& input_shapes,
                                          RuntimeContext& runtime_ctx) const {
  return {input_shapes.at(0)};
}

NDArrayList AllReduceOpImpl::DoCompute(Operator& op,
                                       const NDArrayList& inputs,
                                       RuntimeContext& ctx) const {
  // NDArrayList outputs = inplace() ? inputs : DoAllocOutputs(op, inputs, ctx);
  NDArrayList outputs = inputs; // just inplace here
  HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), type(),
                                  hetu::impl::AllReduce, inputs.at(0),
                                  outputs.at(0), reduction_type(), _comm_group, // _comm_group is a subset of placement_group
                                  op->instantiation_ctx().stream());
  return outputs;
}

// void AllReduceOpImpl::DoCompute(Operator& op, const NDArrayList& inputs,
//                                 NDArrayList& outputs, RuntimeContext& runtime_ctx) const {
//   HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), type(),
//                                   hetu::impl::AllReduce, inputs.at(0),
//                                   outputs.at(0), _comm_group, // _comm_group is a subset of placement_group
//                                   op->instantiation_ctx().stream());                              
// }

bool P2PSendOpImpl::DoMapToParallelDevices(Operator& op,
                                           const DeviceGroupUnion& pg_union) const {
  return OpInterface::DoMapToParallelDevices(op, pg_union);                                          
}


bool P2PSendOpImpl::DoInstantiate(Operator& op, const Device& placement,
                                    StreamIndex stream_index) const {
  bool ret = OpInterface::DoInstantiate(op, placement, stream_index);
  HT_ASSERT(op->local_placement_group().num_devices() == _dst_group.num_devices())
    << "Currently we require equal tensor parallelism degree across "
    << "P2P communication. Got " << op->local_placement_group() << " vs. " << _dst_group;
  size_t dst_device_index = _dst_device_index == -1 ? 
         op->local_placement_group().get_index(op->placement()) : _dst_device_index;  
  auto src_rank = GetWorldRank();
  auto dst_rank = DeviceToWorldRank(_dst_group.get(dst_device_index));
  std::vector<int> ranks(2);
  ranks[0] = std::min(src_rank, dst_rank);
  ranks[1] = std::max(src_rank, dst_rank);
  NCCLCommunicationGroup::GetOrCreate(ranks, op->instantiation_ctx().stream());
  return ret;
}

std::vector<NDArrayMeta> 
P2PSendOpImpl::DoInferMeta(const TensorList& inputs) const {
  return {};
}

HTShapeList P2PSendOpImpl::DoInferShape(Operator& op, 
                                        const HTShapeList& input_shapes,
                                        RuntimeContext& runtime_ctx) const {
  return {};
}

void P2PSendOpImpl::DoCompute(Operator& op, 
                              const NDArrayList& inputs,
                              NDArrayList& outputs,
                              RuntimeContext& runtime_ctx) const {
  NDArray input = inputs.at(0);
  HT_ASSERT(input->dtype() == op->input(0)->dtype())
    << "Data type mismatched for P2P communication: " << input->dtype()
    << " vs. " << op->input(0)->dtype();
  size_t dst_device_index = _dst_device_index == -1 ? 
         op->local_placement_group().get_index(op->placement()) : _dst_device_index;

  // HT_LOG_INFO << "send to " << _dst_group.get(dst_device_index);
  HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), 
                                  type(), hetu::impl::P2PSend, input,
                                  _dst_group.get(dst_device_index), 
                                  op->instantiation_ctx().stream());                                 
}

bool P2PRecvOpImpl::DoMapToParallelDevices(Operator& op,
                                           const DeviceGroupUnion& pg_union) const {
  return OpInterface::DoMapToParallelDevices(op, pg_union);                                          
}

bool P2PRecvOpImpl::DoInstantiate(Operator& op, const Device& placement,
                                  StreamIndex stream_index) const {
  bool ret = OpInterface::DoInstantiate(op, placement, stream_index);
  HT_ASSERT(op->local_placement_group().num_devices() == _src_group.num_devices())
    << "Currently we require equal tensor parallelism degree across "
    << "P2P communication. Got " << _src_group << " vs. " << op->local_placement_group();
  size_t src_device_index = _src_device_index == -1 ?
         op->local_placement_group().get_index(op->placement()) : _src_device_index;
  auto src_rank = DeviceToWorldRank(_src_group.get(src_device_index));
  auto dst_rank = GetWorldRank();
  std::vector<int> ranks(2);
  ranks[0] = std::min(src_rank, dst_rank);
  ranks[1] = std::max(src_rank, dst_rank);
  NCCLCommunicationGroup::GetOrCreate(ranks, op->instantiation_ctx().stream());
  return ret;
}

std::vector<NDArrayMeta> 
P2PRecvOpImpl::DoInferMeta(const TensorList& inputs) const {
  return {NDArrayMeta().set_dtype(_dtype).set_shape(get_shape())};
}

HTShapeList P2PRecvOpImpl::DoInferShape(Operator& op, 
                                        const HTShapeList& input_shapes,
                                        RuntimeContext& runtime_ctx) const {
  return {get_shape()};
}

void P2PRecvOpImpl::DoCompute(Operator& op, 
                              const NDArrayList& inputs,
                              NDArrayList& outputs,
                              RuntimeContext& runtime_ctx) const {
  size_t src_device_index = _src_device_index == -1 ?
         op->local_placement_group().get_index(op->placement()) : _src_device_index;

  // HT_LOG_INFO << "recv from " << _src_group.get(src_device_index);
  HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(),
                                  type(), hetu::impl::P2PRecv, outputs.at(0),
                                  _src_group.get(src_device_index),
                                  op->instantiation_ctx().stream());
}

bool BatchedISendIRecvOpImpl::DoInstantiate(Operator& op, const Device& placement,
                                            StreamIndex stream_index) const {
  bool ret = OpInterface::DoInstantiate(op, placement, stream_index);                                      
  std::vector<int> ranks(_comm_devices.size());
  std::transform(_comm_devices.begin(), _comm_devices.end(), ranks.begin(), [&](const Device& device) { return DeviceToWorldRank(device); });
  std::sort(ranks.begin(), ranks.end());
  NCCLCommunicationGroup::GetOrCreate(ranks, op->instantiation_ctx().stream());
  return ret;
}

std::vector<NDArrayMeta> 
BatchedISendIRecvOpImpl::DoInferMeta(const TensorList& inputs) const {
  HTShapeList outputs_shape = get_outputs_shape();
  if (outputs_shape.size() == 0)
    return {};
  std::vector<NDArrayMeta> output_meta_lsit;
  for (auto& output_shape: outputs_shape) {
    output_meta_lsit.push_back(NDArrayMeta().set_dtype(_dtype).set_shape(output_shape));
  }
  return output_meta_lsit;
}

HTShapeList BatchedISendIRecvOpImpl::DoInferShape(Operator& op, 
                                                  const HTShapeList& input_shapes,
                                                  RuntimeContext& runtime_ctx) const {
  if (_outputs_shape.size() == 0)
    return {};                                                    
  return get_outputs_shape();                                                    
}  

// deprecated: only used in gpt inference, before symbolic shape is realized
HTShapeList BatchedISendIRecvOpImpl::DoInferDynamicShape(Operator& op, 
                                                  const HTShapeList& input_shapes,
                                                  RuntimeContext& runtime_ctx) const {                                             
  HT_RUNTIME_ERROR << "deprecated";
}  

void BatchedISendIRecvOpImpl::DoCompute(Operator& op, 
                                        const NDArrayList& inputs,
                                        NDArrayList& outputs, 
                                        RuntimeContext& runtime_ctx) const {
  for (int i = 0; i < op->num_inputs(); i++) {
    const NDArray& input = inputs.at(i);
    HT_ASSERT(input->dtype() == op->input(i)->dtype())
      << "Data type mismatched for ISend communication: " << input->dtype()
      << " vs. " << op->input(i)->dtype();
  }
  // NOTE: For communication ops, we insert Contiguous op during `MakeOp()`
  // to ensure inputs are contiguous. But for BatchedISendIRecv, we found
  // that inputs may be non-contiguous, which is weird. So we make them
  // contiguous again here.
  NDArrayList contig_inputs;
  std::transform(inputs.begin(), inputs.end(), std::back_inserter(contig_inputs), [&](const NDArray& input) {
    return input->is_contiguous() ? input : NDArray::contiguous(input, op->instantiation_ctx().stream_index);
  });

  HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), type(), 
                                  hetu::impl::BatchedISendIRecv, contig_inputs, _dst_devices, outputs, 
                                  _src_devices, _comm_devices, op->instantiation_ctx().stream());
}

bool AllGatherOpImpl::DoMapToParallelDevices(Operator& op,
                                             const DeviceGroupUnion& pg_union) const {
  return OpInterface::DoMapToParallelDevices(op, pg_union);  
}

bool AllGatherOpImpl::DoInstantiate(Operator& op, const Device& placement,
                                    StreamIndex stream_index) const {
  bool ret = OpInterface::DoInstantiate(op, placement, stream_index);   
  for (int i = 0; i < _comm_group.num_devices(); i++) {
    HT_ASSERT(op->local_placement_group().contains(_comm_group.get(i))) 
      << "Allgather: device in comm_group: " << _comm_group.get(i) 
      << " must in device group: " << op->local_placement_group();
  }                                   
  auto ranks = DeviceGroupToWorldRanks(_comm_group);
  NCCLCommunicationGroup::GetOrCreate(ranks, op->instantiation_ctx().stream());
  return ret;
}

std::vector<NDArrayMeta> 
AllGatherOpImpl::DoInferMeta(const TensorList& inputs) const {
  const Tensor& input = inputs.at(0);
  DataType dtype = input->dtype();
  HTShape gather_shape = input->shape();
  gather_shape[0] *= _comm_group.num_devices();
  return {NDArrayMeta().set_dtype(dtype).set_shape(gather_shape)};
}

HTShapeList AllGatherOpImpl::DoInferShape(Operator& op, 
                                          const HTShapeList& input_shapes,
                                          RuntimeContext& runtime_ctx) const {
  HTShape gather_shape = input_shapes.at(0);
  gather_shape[0] *= _comm_group.num_devices();
  return {gather_shape};  
}

void AllGatherOpImpl::DoCompute(Operator& op, 
                                const NDArrayList& inputs,
                                NDArrayList& outputs,
                                RuntimeContext& runtime_ctx) const {
  HT_ASSERT(inputs.at(0)->dtype() == op->input(0)->dtype())
    << "Data type mismatched for AllGather communication: " << inputs.at(0)->dtype()
    << " vs. " << op->input(0)->dtype();

  HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), type(),
                                  hetu::impl::AllGather, inputs.at(0), outputs.at(0), 
                                  _comm_group, op->instantiation_ctx().stream());
}

bool ReduceScatterOpImpl::DoMapToParallelDevices(Operator& op,
                                                 const DeviceGroupUnion& pg_union) const {
  return OpInterface::DoMapToParallelDevices(op, pg_union);  
}

bool ReduceScatterOpImpl::DoInstantiate(Operator& op, const Device& placement,
                                        StreamIndex stream_index) const {
  bool ret = OpInterface::DoInstantiate(op, placement, stream_index);  
  for (int i = 0; i < _comm_group.num_devices(); i++) {
    HT_ASSERT(op->local_placement_group().contains(_comm_group.get(i))) 
      << "ReduceScatter: device in comm_group: " << _comm_group.get(i) 
      << " must in device group: " << op->local_placement_group();
  }                                    
  auto ranks = DeviceGroupToWorldRanks(_comm_group);
  NCCLCommunicationGroup::GetOrCreate(ranks, op->instantiation_ctx().stream());
  return ret;
}

std::vector<NDArrayMeta> 
ReduceScatterOpImpl::DoInferMeta(const TensorList& inputs) const {
  const Tensor& input = inputs.at(0);
  DataType dtype = input->dtype();
  HTShape scatter_shape = input->shape();
  scatter_shape[0] /= _comm_group.num_devices();
  HT_ASSERT(scatter_shape[0] >= 1) << "ReduceScatter: input shape[0]: " 
    << input->shape()[0] << " must >= comm devices num: " << _comm_group.num_devices();  
  return {NDArrayMeta().set_dtype(dtype).set_shape(scatter_shape)};
}

HTShapeList ReduceScatterOpImpl::DoInferShape(Operator& op, 
                                              const HTShapeList& input_shapes,
                                              RuntimeContext& runtime_ctx) const {
  HTShape scatter_shape = input_shapes.at(0);
  scatter_shape[0] /= _comm_group.num_devices();
  HT_ASSERT(scatter_shape[0] >= 1) << "ReduceScatter: input shape[0]: " 
    << input_shapes.at(0)[0] << " must >= comm devices num: " << _comm_group.num_devices();  
  return {scatter_shape};
}

NDArrayList ReduceScatterOpImpl::DoCompute(Operator& op,
                                           const NDArrayList& inputs,
                                           RuntimeContext& ctx) const {                              
  NDArrayList outputs = {};
  HT_ASSERT(inputs.at(0)->dtype() == op->input(0)->dtype())
    << "Data type mismatched for ReduceScatter communication: " << inputs.at(0)->dtype()
    << " vs. " << op->input(0)->dtype();

  // if (inplace()) {
  // just inplace here
  NDArrayMeta meta = inputs.at(0)->meta();
  HTShape scatter_shape = inputs.at(0)->shape();
  scatter_shape[0] /= _comm_group.num_devices();
  meta.set_shape(scatter_shape);
  // int rank = GetWorldRank();
  int rank = _comm_group.get_index(op->placement());
  size_t storage_offset = rank * (inputs.at(0)->numel() / _comm_group.num_devices());
  NDArray output = NDArray(meta, inputs.at(0)->storage(), inputs.at(0)->storage_offset() + storage_offset);
  outputs.emplace_back(output);
  // }
  // else {
  //   outputs = DoAllocOutputs(op, inputs, ctx);
  // }
  
  // HT_LOG_INFO << "comm group " << _comm_group
  HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), type(),
                                  hetu::impl::ReduceScatter, inputs.at(0), outputs.at(0), 
                                  reduction_type(), _comm_group, op->instantiation_ctx().stream());
  return outputs;
}

// void ReduceScatterOpImpl::DoCompute(Operator& op, 
//                                     const NDArrayList& inputs,
//                                     NDArrayList& outputs,
//                                     RuntimeContext& runtime_ctx) const {
//   HT_ASSERT(inputs.at(0)->dtype() == op->input(0)->dtype())
//     << "Data type mismatched for ReduceScatter communication: " << inputs.at(0)->dtype()
//     << " vs. " << op->input(0)->dtype();

//   HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), type(),
//                                   hetu::impl::ReduceScatter, inputs.at(0), outputs.at(0), 
//                                   _comm_group, op->instantiation_ctx().stream());
// }

bool SplitAllReduceOpImpl::DoMapToParallelDevices(Operator& op, 
                                                  const DeviceGroupUnion& pg_union) const {
  return OpInterface::DoMapToParallelDevices(op, pg_union);
}

bool SplitAllReduceOpImpl::DoInstantiate(Operator& op, const Device& placement,
                                         StreamIndex stream_index) const {
  bool ret = OpInterface::DoInstantiate(op, placement, stream_index);
  for (const auto& comm_group : _comm_groups) {
    auto ranks = DeviceGroupToWorldRanks(comm_group);
    NCCLCommunicationGroup::GetOrCreate(ranks, op->instantiation_ctx().stream());
  }
  return ret;
}

std::vector<NDArrayMeta> 
SplitAllReduceOpImpl::DoInferMeta(const TensorList& inputs) const {
  return {inputs[0]->meta()};
}

HTShapeList SplitAllReduceOpImpl::DoInferShape(Operator& op, 
                                               const HTShapeList& input_shapes,
                                               RuntimeContext& runtime_ctx) const {
  return {input_shapes.at(0)};
}

NDArrayList SplitAllReduceOpImpl::DoCompute(Operator& op,
                                            const NDArrayList& inputs,
                                            RuntimeContext& ctx) const {
  NDArrayList outputs = inputs; // just inplace here
  NDArrayList split_inputs = NDArray::split(inputs.at(0), split_num());
  NDArrayList split_outputs = NDArray::split(outputs.at(0), split_num());
  for (auto i = 0; i < _comm_groups.size(); i++) {
    const auto& comm_group = _comm_groups[i];
    HT_DISPATCH_KERNEL_CPU_AND_CUDA(op->instantiation_ctx().placement.type(), type(),
                                    hetu::impl::AllReduce, split_inputs.at(i),
                                    split_outputs.at(i), reduction_type(), comm_group,
                                    op->instantiation_ctx().stream());
  }
  return outputs;
}

Tensor MakeCommOp(Tensor input, DistributedStatesHierarchy dst_ds_hierarchy, 
                  ReductionType red_type, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0)
    << "MakeCommOp mustn't use device group hierarchy, please use its official attribute (dst group hierarchy) instead to avoid chaos";
  return Graph::MakeOp(std::make_shared<CommOpImpl>(std::move(dst_ds_hierarchy), DeviceGroupHierarchy(), red_type), 
                      {input}, std::move(op_meta))->output(0);
}

Tensor MakeCommOp(Tensor input, DistributedStatesHierarchy dst_ds_hierarchy,
                  const std::string& mode, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0)
    << "MakeCommOp mustn't use device group hierarchy, please use its official attribute (dst group hierarchy) instead to avoid chaos";
  return Graph::MakeOp(std::make_shared<CommOpImpl>(std::move(dst_ds_hierarchy), DeviceGroupHierarchy(), Str2ReductionType(mode)), 
                      {input}, std::move(op_meta))->output(0);
}

Tensor MakeCommOp(Tensor input, DistributedStatesHierarchy dst_ds_hierarchy, DeviceGroupHierarchy dst_group_hierarchy, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0)
    << "MakeCommOp mustn't use device group hierarchy, please use its official attribute (dst group hierarchy) instead to avoid chaos";
  return Graph::MakeOp(std::make_shared<CommOpImpl>(std::move(dst_ds_hierarchy), std::move(dst_group_hierarchy)), 
                      {input}, std::move(op_meta))->output(0);
}

Tensor MakeCommOp(Tensor input, DistributedStatesHierarchy dst_ds_hierarchy, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0)
    << "MakeCommOp mustn't use device group hierarchy, please use its official attribute (dst group hierarchy) instead to avoid chaos";
  return Graph::MakeOp(std::make_shared<CommOpImpl>(std::move(dst_ds_hierarchy)), 
                      {input}, std::move(op_meta))->output(0);
}

// for comm ops created in exec_graph, the device_group_hierarchy only contains one device_group_union
// which only contains one device_group
Tensor MakeAllReduceOp(Tensor input, DeviceGroup comm_group, bool inplace, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0 || (op_meta.device_group_hierarchy.size() == 1 && op_meta.device_group_hierarchy.get(0).size() == 1
    && op_meta.device_group_hierarchy.get(0).get(0).is_subset(comm_group))) << "comm_group must be subset of device_group!";
  return Graph::MakeOp(std::make_shared<AllReduceOpImpl>(std::move(comm_group), kSUM, inplace), 
                      {input}, std::move(op_meta))->output(0);
}

Tensor MakeAllReduceOp(Tensor input, DeviceGroup comm_group, 
                       ReductionType red_type, bool inplace, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0 || (op_meta.device_group_hierarchy.size() == 1 && op_meta.device_group_hierarchy.get(0).size() == 1
    && op_meta.device_group_hierarchy.get(0).get(0).is_subset(comm_group))) << "comm_group must be subset of device_group!";
  return Graph::MakeOp(std::make_shared<AllReduceOpImpl>(std::move(comm_group), red_type, inplace), 
                      {input}, std::move(op_meta))->output(0);
}

// p2p send no output
Tensor MakeP2PSendOp(Tensor input, DeviceGroup dst_group, 
                     int dst_device_index, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0 || (op_meta.device_group_hierarchy.size() == 1 && op_meta.device_group_hierarchy.get(0).size() == 1
    && op_meta.device_group_hierarchy.get(0).get(0).num_devices() == dst_group.num_devices()))
    << "Currently we require equal tensor parallelism degree across "
    << "P2P communication. Got " << op_meta.device_group_hierarchy.get(0).get(0) << " vs. " << dst_group;
  return Graph::MakeOp(std::make_shared<P2PSendOpImpl>(std::move(dst_group), 
                       dst_device_index), {input}, std::move(op_meta))->out_dep_linker();
}

Tensor MakeP2PRecvOp(DeviceGroup src_group, DataType dtype,
                     HTShape shape, int src_device_index, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0 || (op_meta.device_group_hierarchy.size() == 1 && op_meta.device_group_hierarchy.get(0).size() == 1
    && op_meta.device_group_hierarchy.get(0).get(0).num_devices() == src_group.num_devices()))
    << "Currently we require equal tensor parallelism degree across "
    << "P2P communication. Got " << op_meta.device_group_hierarchy.get(0).get(0) << " vs. " << src_group;
  return Graph::MakeOp(std::make_shared<P2PRecvOpImpl>(std::move(src_group), dtype, std::move(shape), 
                       src_device_index), {}, std::move(op_meta))->output(0);
}

// symbolic shape
Tensor MakeP2PRecvOp(DeviceGroup src_group, DataType dtype,
                     SyShape shape, int src_device_index, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0 || (op_meta.device_group_hierarchy.size() == 1 && op_meta.device_group_hierarchy.get(0).size() == 1
    && op_meta.device_group_hierarchy.get(0).get(0).num_devices() == src_group.num_devices()))
    << "Currently we require equal tensor parallelism degree across "
    << "P2P communication. Got " << op_meta.device_group_hierarchy.get(0).get(0) << " vs. " << src_group;
  return Graph::MakeOp(std::make_shared<P2PRecvOpImpl>(std::move(src_group), dtype, std::move(shape), 
                       src_device_index), {}, std::move(op_meta))->output(0);
}

// fixed shape
Tensor MakeBatchedISendIRecvOp(TensorList inputs, 
                               std::vector<Device> dst_devices, 
                               HTShapeList outputs_shape, 
                               std::vector<Device> src_devices, 
                               std::vector<Device> comm_devices, 
                               DataType dtype, OpMeta op_meta) {
  if (src_devices.size() == 0)
    return Graph::MakeOp(std::make_shared<BatchedISendIRecvOpImpl>(std::move(dst_devices), std::move(outputs_shape),
                        std::move(src_devices), std::move(comm_devices), dtype), std::move(inputs), std::move(op_meta))->out_dep_linker();
  else
    return Graph::MakeOp(std::make_shared<BatchedISendIRecvOpImpl>(std::move(dst_devices), std::move(outputs_shape),
                        std::move(src_devices), std::move(comm_devices), dtype), std::move(inputs), std::move(op_meta))->output(0);  
}

// symbolic shape
Tensor MakeBatchedISendIRecvOp(TensorList inputs, 
                               std::vector<Device> dst_devices, 
                               SyShapeList outputs_shape, 
                               std::vector<Device> src_devices, 
                               std::vector<Device> comm_devices, 
                               DataType dtype, OpMeta op_meta) {
  if (src_devices.size() == 0)
    return Graph::MakeOp(std::make_shared<BatchedISendIRecvOpImpl>(std::move(dst_devices), std::move(outputs_shape),
                        std::move(src_devices), std::move(comm_devices), dtype), std::move(inputs), std::move(op_meta))->out_dep_linker();
  else
    return Graph::MakeOp(std::make_shared<BatchedISendIRecvOpImpl>(std::move(dst_devices), std::move(outputs_shape),
                        std::move(src_devices), std::move(comm_devices), dtype), std::move(inputs), std::move(op_meta))->output(0);  
}

Tensor MakeAllGatherOp(Tensor input, DeviceGroup comm_group, 
                       OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0 || (op_meta.device_group_hierarchy.size() == 1 && op_meta.device_group_hierarchy.get(0).size() == 1
    && op_meta.device_group_hierarchy.get(0).get(0).is_subset(comm_group))) << "comm_group must be subset of device_group!";
  return Graph::MakeOp(std::make_shared<AllGatherOpImpl>(std::move(comm_group)), 
                      {input}, std::move(op_meta))->output(0);
}

Tensor MakeReduceScatterOp(Tensor input, DeviceGroup comm_group, 
                           bool inplace, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0 || (op_meta.device_group_hierarchy.size() == 1 && op_meta.device_group_hierarchy.get(0).size() == 1
    && op_meta.device_group_hierarchy.get(0).get(0).is_subset(comm_group))) << "comm_group must be subset of device_group!";
  return Graph::MakeOp(std::make_shared<ReduceScatterOpImpl>(std::move(comm_group), kSUM, inplace), 
                      {input}, std::move(op_meta))->output(0);
}

Tensor MakeReduceScatterOp(Tensor input, DeviceGroup comm_group, 
                           ReductionType red_type, bool inplace, OpMeta op_meta) {
  HT_ASSERT(op_meta.device_group_hierarchy.size() == 0 || (op_meta.device_group_hierarchy.size() == 1 && op_meta.device_group_hierarchy.get(0).size() == 1
    && op_meta.device_group_hierarchy.get(0).get(0).is_subset(comm_group))) << "comm_group must be subset of device_group!";
  return Graph::MakeOp(std::make_shared<ReduceScatterOpImpl>(std::move(comm_group), red_type, inplace), 
                      {input}, std::move(op_meta))->output(0);
}

Tensor MakeSplitAllReduceOp(Tensor input, DeviceGroupList comm_groups, size_t split_num, bool inplace, OpMeta op_meta) {
  return Graph::MakeOp(std::make_shared<SplitAllReduceOpImpl>(std::move(comm_groups), split_num, kSUM, inplace), 
                      {input}, std::move(op_meta))->output(0);
}

Tensor MakeSplitAllReduceOp(Tensor input, DeviceGroupList comm_groups, size_t split_num,
                            ReductionType red_type, bool inplace, OpMeta op_meta) {
  return Graph::MakeOp(std::make_shared<SplitAllReduceOpImpl>(std::move(comm_groups), split_num, red_type, inplace), 
                      {input}, std::move(op_meta))->output(0);
}

} // namespace graph
} // namespace hetu
