#pragma once

#include "hetu/core/memory_pool.h"
#include "hetu/utils/task_queue.h"
#include <functional>

namespace hetu {
namespace impl {

class CPUMemoryPool final : public MemoryPool {
 public:
  CPUMemoryPool();

  ~CPUMemoryPool();

  DataPtr AllocDataSpace(size_t num_bytes, const Stream& stream = Stream());

  DataPtr BorrowDataSpace(void* ptr, size_t num_bytes, DataPtrDeleter deleter);

  void FreeDataSpace(DataPtr data_ptr);

  void MarkDataSpaceUsedByStream(DataPtr data_ptr, const Stream& stream);

  void MarkDataSpacesUsedByStream(DataPtrList& data_ptrs, const Stream& stream);

  std::future<void> WaitDataSpace(DataPtr data_ptr, bool async = true);

  inline size_t get_data_alignment() const noexcept {
    return 16;
  }

 private:
  struct CPUDataPtrInfo {
    void* ptr;
    size_t num_bytes;
    Stream alloc_stream;
    DataPtrDeleter deleter;
    std::unordered_map<Stream, std::shared_ptr<Event>> dependent_events;

    CPUDataPtrInfo(void* ptr_, size_t num_bytes_, Stream alloc_stream_,
                   DataPtrDeleter deleter_ = {})
    : ptr(ptr_),
      num_bytes(num_bytes_),
      alloc_stream{std::move(alloc_stream_)},
      deleter{std::move(deleter_)} {}
  };

  static void _FreeOnAllocStream(CPUMemoryPool* const pool, DataPtr ptr);
  static void _FreeOnJoinStream(CPUMemoryPool* const pool, DataPtr ptr);

  size_t _allocated = 0;
  size_t _peak_allocated = 0;
  std::unordered_map<uint64_t, CPUDataPtrInfo> _data_ptr_info;
  std::function<void(DataPtr)> _free_on_alloc_stream_fn;
  std::function<void(DataPtr)> _free_on_join_stream_fn;
};

} // namespace impl
} // namespace hetu
