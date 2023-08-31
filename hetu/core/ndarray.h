#pragma once

#include "hetu/utils/shared_ptr_wrapper.h"
#include "hetu/core/reduction_type.h"
#include "hetu/core/binary_type.h"
#include "hetu/core/ndarray_meta.h"
#include "hetu/core/ndarray_storage.h"
#include "hetu/core/stream.h"

namespace hetu {

class NDArrayDef;
class NDArray;
using NDArrayList = std::vector<NDArray>;

class NDArray : public shared_ptr_wrapper<NDArrayDef> {
 public:
  NDArray() = default;
  NDArray(const NDArrayMeta& meta,
          std::shared_ptr<NDArrayStorage> storage = nullptr,
          size_t storage_offset = 0)
  : shared_ptr_wrapper<NDArrayDef>() {
    _ptr = make_ptr<NDArrayDef>(meta, storage, storage_offset);
  }
  using shared_ptr_wrapper<NDArrayDef>::operator=;

 private:
  static NDArray EMPTY;

 public:
  static const StreamIndex DEFAULT_STREAM;

  static NDArray to(const NDArray& input,
                    const Device& device = Device(kUndeterminedDevice),
                    DataType = kUndeterminedDataType,
                    StreamIndex stream_id = DEFAULT_STREAM,
                    NDArray& output = EMPTY);

  static inline NDArray cuda(const NDArray& input, DeviceIndex dev_id = 0,
                             StreamIndex stream_id = DEFAULT_STREAM,
                             NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kCUDA, dev_id), kUndeterminedDataType,
                       stream_id, output);
  }

  static inline NDArray cpu(const NDArray& input,
                            StreamIndex stream_id = DEFAULT_STREAM,
                            NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kCPU), kUndeterminedDataType, stream_id,
                       output);
  }

  static inline NDArray toUInt8(const NDArray& input,
                                StreamIndex stream_id = DEFAULT_STREAM,
                                NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kUndeterminedDevice), kUInt8, stream_id,
                       output);
  }

  static inline NDArray toInt8(const NDArray& input,
                               StreamIndex stream_id = DEFAULT_STREAM,
                               NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kUndeterminedDevice), kInt8, stream_id,
                       output);
  }

  static inline NDArray toInt16(const NDArray& input,
                                StreamIndex stream_id = DEFAULT_STREAM,
                                NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kUndeterminedDevice), kInt16, stream_id,
                       output);
  }

  static inline NDArray toInt32(const NDArray& input,
                                StreamIndex stream_id = DEFAULT_STREAM,
                                NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kUndeterminedDevice), kInt32, stream_id,
                       output);
  }

  static inline NDArray toInt64(const NDArray& input,
                                StreamIndex stream_id = DEFAULT_STREAM,
                                NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kUndeterminedDevice), kInt64, stream_id,
                       output);
  }

  static inline NDArray toFloat32(const NDArray& input,
                                  StreamIndex stream_id = DEFAULT_STREAM,
                                  NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kUndeterminedDevice), kFloat32, stream_id,
                       output);
  }

  static inline NDArray toFloat64(const NDArray& input,
                                  StreamIndex stream_id = DEFAULT_STREAM,
                                  NDArray& output = EMPTY) {
    return NDArray::to(input, Device(kUndeterminedDevice), kFloat64, stream_id,
                       output);
  }

  static NDArray abs(const NDArray& input,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray add(const NDArray& x, const NDArray& y,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray add(const NDArray& input, double scalar,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray sub(const NDArray& x, const NDArray& y,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray sub(const NDArray& input, double scalar,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray sub(double scalar, const NDArray& input,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray neg(const NDArray& input,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray mul(const NDArray& x, const NDArray& y,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray mul(const NDArray& input, double scalar,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray div(const NDArray& x, const NDArray& y,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray div(const NDArray& input, double scalar,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray div(double scalar, const NDArray& input,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray pow(const NDArray& input, double exponent = 2.0,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray sqrt(const NDArray& input,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY);

  static NDArray reciprocal(const NDArray& input,
                            StreamIndex stream_id = DEFAULT_STREAM,
                            NDArray& output = EMPTY);

  static NDArray sigmoid(const NDArray& input,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);

  static NDArray relu(const NDArray& input,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY);

  static NDArray tanh(const NDArray& input,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY);

  static NDArray exp(const NDArray& input,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray log(const NDArray& input,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray ceil(const NDArray& input,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY);

  static NDArray floor(const NDArray& input,
                       StreamIndex stream_id = DEFAULT_STREAM,
                       NDArray& output = EMPTY);

  static NDArray round(const NDArray& input,
                       StreamIndex stream_id = DEFAULT_STREAM,
                       NDArray& output = EMPTY);

  static NDArray reduce(const NDArray& input, ReductionType red_type,
                        const HTAxes& axes = HTAxes(), bool keepdims = false,
                        StreamIndex stream_id = DEFAULT_STREAM,
                        NDArray& output = EMPTY);

  static NDArray sum(const NDArray& input, const HTAxes& axes = HTAxes(),
                     bool keepdims = false,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY) {
    return NDArray::reduce(input, kSUM, axes, keepdims, stream_id, output);
  }

  static NDArray mean(const NDArray& input, const HTAxes& axes = HTAxes(),
                      bool keepdims = false,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY) {
    return NDArray::reduce(input, kMEAN, axes, keepdims, stream_id, output);
  }

  static NDArray prod(const NDArray& input, const HTAxes& axes = HTAxes(),
                      bool keepdims = false,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY) {
    return NDArray::reduce(input, kPROD, axes, keepdims, stream_id, output);
  }

  static NDArray max(const NDArray& input, const HTAxes& axes = HTAxes(),
                     bool keepdims = false,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY) {
    return NDArray::reduce(input, kMAX, axes, keepdims, stream_id, output);
  }

  static NDArray min(const NDArray& input, const HTAxes& axes = HTAxes(),
                     bool keepdims = false,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY) {
    return NDArray::reduce(input, kMIN, axes, keepdims, stream_id, output);
  }

  static NDArray matmul(const NDArray& x, const NDArray& y,
                        bool trans_left = false, bool trans_right = false,
                        StreamIndex stream_id = DEFAULT_STREAM,
                        NDArray& output = EMPTY);

  static NDArray bmm(const NDArray& x, const NDArray& y,
                     bool trans_left = false, bool trans_right = false,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray dot(const NDArray& x, const NDArray& y,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray index_add(const NDArray& x, const NDArray& ids,
                           const NDArray& y, int64_t dim = 0,
                           StreamIndex stream_id = DEFAULT_STREAM,
                           NDArray& output = EMPTY);

  static NDArray reshape(const NDArray& input, const HTShape& new_shape,
                         StreamIndex stream_id = DEFAULT_STREAM);

  static NDArray view(const NDArray& input, const HTShape& view_shape);

  static NDArray squeeze(const NDArray& input);

  static NDArray squeeze(const NDArray& input, int64_t dim);

  static NDArray unsqueeze(const NDArray& input, int64_t dim);

  static NDArray flatten(const NDArray& input, int64_t start_dim = 0,
                         int64_t end_dim = -1);

  static NDArray permute(const NDArray& input, HTAxes& dims,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);

  static NDArray movedim(const NDArray& input, int64_t src, int64_t dst,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);

  static NDArray adddim(const NDArray& input, int64_t dim, int64_t size,
                        StreamIndex stream_id = DEFAULT_STREAM,
                        NDArray& output = EMPTY);

  static NDArray diagonal(const NDArray& input, int64_t dim1, int64_t dim2,
                          int64_t offset = 0,
                          StreamIndex stream_id = DEFAULT_STREAM,
                          NDArray& output = EMPTY);

  static NDArray diagonal_grad(const NDArray& input, int64_t dim1, int64_t dim2,
                               StreamIndex stream_id = DEFAULT_STREAM,
                               NDArray& output = EMPTY);

  static NDArrayList split(const NDArray& input, size_t num_chunks,
                           int64_t axis = 0,
                           StreamIndex stream_id = DEFAULT_STREAM);

  static NDArrayList split(const NDArray& input, const HTShape& chunks,
                           int64_t axis = 0,
                           StreamIndex stream_id = DEFAULT_STREAM);

//____________________________________________________________________________________________

  static NDArray arrayset(NDArray& input, double value,
                          StreamIndex stream_id = DEFAULT_STREAM);


  static NDArray avgpool(const NDArray& input, const size_t kernel_H,
                         const size_t kernel_W, const size_t padding,
                         const size_t stride,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);

  static NDArrayList batchnorm(const NDArray& input, const NDArray& bn_scale, const NDArray& bn_bias,
                               NDArray& running_mean, NDArray& running_var,
                               double momentum = 0.1, double eps = 1e-5,
                               StreamIndex stream_id = DEFAULT_STREAM,
                               NDArray& output = EMPTY,
                               NDArray& save_mean = EMPTY,
                               NDArray& save_var = EMPTY);
   
  static NDArray broadcast(const NDArray& input, const HTShape& shape,
                           StreamIndex stream_id = DEFAULT_STREAM,
                           NDArray& output = EMPTY);

  static NDArray broadcast(const NDArray& input, const HTShape& shape,
                           const HTAxes& add_axes,
                           StreamIndex stream_id = DEFAULT_STREAM,
                           NDArray& output = EMPTY);

  static NDArray conv2d(const NDArray& input, const NDArray& filter, 
                        const HTShape& padding, const HTShape& stride,
                        StreamIndex stream_id = DEFAULT_STREAM,
                        NDArray& output = EMPTY);

  static NDArray embedding(const NDArray& input, const NDArray& id,
                           StreamIndex stream_id = DEFAULT_STREAM,
                           NDArray& output = EMPTY);

  static NDArray gather(const NDArray& input, const NDArray& id, int64_t dim,
                        StreamIndex stream_id = DEFAULT_STREAM,
                        NDArray& output = EMPTY);

  static NDArrayList instancenorm(const NDArray& input, double eps = 1e-7,
                                  StreamIndex stream_id = DEFAULT_STREAM,
                                  NDArray& output = EMPTY,
                                  NDArray& save_mean = EMPTY,
                                  NDArray& save_var = EMPTY);

  static NDArray kldiv(const NDArray& preds, const NDArray& labels,
                       ReductionType reduction = kMEAN,
                       StreamIndex stream_id = DEFAULT_STREAM,
                       NDArray& output = EMPTY);

  static NDArrayList layernorm(const NDArray& input, const NDArray& bn_scale, const NDArray& bn_bias, 
                               const HTShape& normalized_shape, double eps = 0.01,
                               StreamIndex stream_id = DEFAULT_STREAM,
                               NDArray& output = EMPTY,
                               NDArray& save_mean = EMPTY,
                               NDArray& save_var = EMPTY);

  static NDArray leakyrelu(const NDArray& input, double alpha,
                           StreamIndex stream_id = DEFAULT_STREAM,
                           NDArray& output = EMPTY);

  static NDArray linear(const NDArray& a, const NDArray& b, const NDArray& bias, 
                        bool trans_a = false, bool trans_b = true,
                        StreamIndex stream_id = DEFAULT_STREAM,
                        NDArray& output = EMPTY);

  static NDArray maxpool(const NDArray& input, const size_t kernel_H,
                         const size_t kernel_W, const size_t padding,
                         const size_t stride,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);

  static NDArray mseloss(const NDArray& preds, const NDArray& labels,
                         ReductionType reduction = kMEAN,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);

  static NDArray nllloss(const NDArray& preds, const NDArray& labels,
                         ReductionType reduction = kMEAN,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);

  static NDArray norm(const NDArray& input, int64_t p = 1, int64_t dim = 0, 
                      bool keepdim = false,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY);

  static NDArray onehot(const NDArray& input, size_t num_classes,
                        StreamIndex stream_id = DEFAULT_STREAM,
                        NDArray& output = EMPTY);

  static NDArray pad(const NDArray& input, const HTShape& paddings, 
                     std::string mode, double constant,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray repeat(const NDArray& input, HTShape repeats,
                        StreamIndex stream_id = DEFAULT_STREAM,
                        NDArray& output = EMPTY);

  static NDArray roll(const NDArray& input, HTShape shifts, HTAxes dims,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY);

  static NDArray sin(const NDArray& input,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static NDArray slice(const NDArray& input, const HTShape& begin_pos, const HTShape& output_shape,
                       StreamIndex stream_id = DEFAULT_STREAM,
                       NDArray& output = EMPTY);

  static NDArray softmax(const NDArray& input, int64_t dim,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);

  static NDArray sceloss(const NDArray& preds, const NDArray& labels,
                         ReductionType reduction = kMEAN,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);     

  static NDArray sceloss(const NDArray& preds, const NDArray& labels, const int64_t ignored_index = -1, 
                         ReductionType reduction = kMEAN,
                         StreamIndex stream_id = DEFAULT_STREAM,
                         NDArray& output = EMPTY);                     

  static NDArray triu(const NDArray& input, bool lower = false, int64_t diagonal = 0, 
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY);   

  static NDArray where(const NDArray& cond, const NDArray& x, const NDArray& y, 
                       StreamIndex stream_id = DEFAULT_STREAM,
                       NDArray& output = EMPTY); 
//____________________________________________________________________________________________

  static NDArray cat(const NDArrayList& inputs, int axis = 0,
                     StreamIndex stream_id = DEFAULT_STREAM,
                     NDArray& output = EMPTY);

  static inline NDArray zeros(const HTShape& shape,
                              const Device& device = Device(kCPU),
                              DataType dtype = kFloat32,
                              StreamIndex stream_id = DEFAULT_STREAM) {
    return NDArray::full(shape, 0, device, dtype, stream_id);
  }

  static inline NDArray zeros_like(const NDArray& other,
                                   StreamIndex stream_id = DEFAULT_STREAM) {
    return NDArray::full_like(other, 0, stream_id);
  }

  static inline NDArray zeros_(NDArray& data,
                               StreamIndex stream_id = DEFAULT_STREAM) {
    return NDArray::full_(data, 0, stream_id);
  }

  static inline NDArray ones(const HTShape& shape,
                             const Device& device = Device(kCPU),
                             DataType dtype = kFloat32,
                             StreamIndex stream_id = DEFAULT_STREAM) {
    return NDArray::full(shape, 1, device, dtype, stream_id);
  }

  static inline NDArray ones_like(const NDArray& other,
                                  StreamIndex stream_id = DEFAULT_STREAM) {
    return NDArray::full_like(other, 1, stream_id);
  }

  static inline NDArray ones_(NDArray& data,
                              StreamIndex stream_id = DEFAULT_STREAM) {
    return NDArray::full_(data, 1, stream_id);
  }

  static NDArray empty(const HTShape& shape,
                       const Device& device = Device(kCPU),
                       DataType dtype = kFloat32);

  static NDArray empty_like(const NDArray& other);

  static NDArray full(const HTShape& shape, double fill_value,
                      const Device& device = Device(kCPU),
                      DataType dtype = kFloat32,
                      StreamIndex stream_id = DEFAULT_STREAM);

  static NDArray full_like(const NDArray& other, double fill_value,
                           StreamIndex stream_id = DEFAULT_STREAM);

  static NDArray full_(NDArray& data, double fill_value,
                       StreamIndex stream_id = DEFAULT_STREAM);

  static NDArray copy(const NDArray& input,
                      StreamIndex stream_id = DEFAULT_STREAM,
                      NDArray& output = EMPTY);

  static NDArray rand(const HTShape& shape, const Device& device = Device(kCPU),
                      DataType dtype = kFloat32, double lb = 0.0,
                      double ub = 1.0, uint64_t seed = 0,
                      StreamIndex stream_id = DEFAULT_STREAM);

  static NDArray randn(const HTShape& shape,
                       const Device& device = Device(kCPU),
                       DataType dtype = kFloat32, double mean = 0.0,
                       double stddev = 1.0, uint64_t seed = 0,
                       StreamIndex stream_id = DEFAULT_STREAM);

  static NDArray uniform_(NDArray& data, double lb = 0.0, double ub = 1.0,
                          uint64_t seed = 0,
                          StreamIndex stream_id = DEFAULT_STREAM);

  static NDArray normal_(NDArray& data, double mean = 0.0, double stddev = 1.0,
                         uint64_t seed = 0,
                         StreamIndex stream_id = DEFAULT_STREAM);

  static NDArray truncated_normal_(NDArray& data, double mean = 0.0,
                                   double stddev = 1.0, double lb = 0.0,
                                   double ub = 1.0, uint64_t seed = 0,
                                   StreamIndex stream_id = DEFAULT_STREAM);
};

inline NDArray operator+(const NDArray& x, const NDArray& y) {
  return NDArray::add(x, y);
}
inline NDArray operator+(const NDArray& input, double scalar) {
  return NDArray::add(input, scalar);
}
inline NDArray operator+(double scalar, const NDArray& input) {
  return NDArray::add(input, scalar);
}
inline NDArray operator-(const NDArray& x, const NDArray& y) {
  return NDArray::sub(x, y);
}
inline NDArray operator-(const NDArray& input, double scalar) {
  return NDArray::sub(input, scalar);
}
inline NDArray operator-(double scalar, const NDArray& input) {
  return NDArray::sub(scalar, input);
}
inline NDArray operator*(const NDArray& x, const NDArray& y) {
  return NDArray::mul(x, y);
}
inline NDArray operator*(const NDArray& input, double scalar) {
  return NDArray::mul(input, scalar);
}
inline NDArray operator*(double scalar, const NDArray& input) {
  return NDArray::mul(input, scalar);
}
inline NDArray operator/(const NDArray& x, const NDArray& y) {
  return NDArray::div(x, y);
}
inline NDArray operator/(const NDArray& input, double scalar) {
  return NDArray::div(input, scalar);
}
inline NDArray operator/(double scalar, const NDArray& input) {
  return NDArray::div(scalar, input);
}

class NDArrayDef : public shared_ptr_target {
 public:
  NDArrayDef(const NDArrayMeta& meta,
             std::shared_ptr<NDArrayStorage> storage = nullptr,
             size_t storage_offset = 0)
  : _meta(meta), _storage(storage) {
    HT_ASSERT(_meta.dtype != kUndeterminedDataType &&
              _meta.device != kUndeterminedDevice && meta.numel() > 0)
      << "Invalid meta: " << _meta;
    size_t bytes_per_value = DataType2Size(meta.dtype);
    if (storage == nullptr) {
      _storage = std::make_shared<NDArrayStorage>(
        meta.numel() * bytes_per_value, meta.device);
      _storage_offset = 0;
    } else {
      HT_ASSERT_GE(_storage->size() - storage_offset * bytes_per_value,
                   meta.numel() * bytes_per_value)
        << "Storage size is not sufficient";
      _storage_offset = storage_offset;
    }
  }

  NDArrayDef(NDArray& other)
  : NDArrayDef(other->_meta, other->_storage, other->_storage_offset) {}

  // disable copy constructor and move constructor
  NDArrayDef(const NDArrayDef& other) = delete;
  NDArrayDef& operator=(const NDArrayDef& other) = delete;
  NDArrayDef(NDArrayDef&& other) = delete;
  NDArrayDef& operator=(NDArrayDef&& other) = delete;

  inline void* raw_data_ptr() {
    return const_cast<void*>(
      const_cast<const NDArrayDef*>(this)->raw_data_ptr());
  }

  inline const void* raw_data_ptr() const {
    HT_ASSERT_NE(_storage, nullptr) << "Storage is not initialized";
    auto* ptr = static_cast<uint8_t*>(_storage->mutable_data());
    ptr += _storage_offset * DataType2Size(dtype());
    return static_cast<void*>(ptr);
  }

  template <typename T>
  inline T* data_ptr() {
    return static_cast<T*>(raw_data_ptr());
  }

  template <typename T>
  inline const T* data_ptr() const {
    return static_cast<const T*>(raw_data_ptr());
  }

  template <typename T>
  inline T item() {
    HT_VALUE_ERROR_IF(numel() != 1)
      << "Cannot convert data with shape " << shape() << " to a scalar value";
    return *(data_ptr<T>());
  }

  const NDArrayMeta& meta() const {
    return _meta;
  }

  size_t ndim() const {
    return _meta.ndim();
  }

  size_t numel() const {
    return _meta.numel();
  }

  DataType dtype() const {
    return _meta.dtype;
  }

  Device device() const {
    return _meta.device;
  }

  bool is_cpu() const {
    return _meta.device.is_cpu();
  }

  bool is_cuda() const {
    return _meta.device.is_cuda();
  }

  const HTShape& shape() const {
    return _meta.shape;
  }

  int64_t shape(size_t axis) const {
    return _meta.shape[axis];
  }

  const HTStride& stride() const {
    return _meta.stride;
  }

  std::shared_ptr<NDArrayStorage> storage() const {
    return _storage;
  }

  size_t storage_offset() const {
    return _storage_offset;
  }

  void set_meta(const NDArrayMeta& meta) {
    _meta = meta;
  }

 protected:
  friend class NDArray;
  void Serialize(std::ostream& os, size_t n_print = 10) const;
  friend std::ostream& operator<<(std::ostream&, const NDArray&);

  NDArrayMeta _meta;
  std::shared_ptr<NDArrayStorage> _storage;
  size_t _storage_offset;
};

std::ostream& operator<<(std::ostream&, const NDArray&);

inline bool IsCopiable(const NDArray& x, const NDArray& y) {
  return IsCopiable(x->meta(), y->meta());
}

inline bool IsExchangable(const NDArray& x, const NDArray& y) {
  return IsExchangable(x->meta(), y->meta());
}

inline bool IsBroadcastable(const NDArray& x, const NDArray& y) {
  return IsBroadcastable(x->meta(), y->meta());
}

inline bool IsConcatable(const NDArray& x, const NDArray& y, const NDArray& z,
                         int64_t axis) {
  return IsConcatable(x->meta(), y->meta(), z->meta(), axis);
}

inline bool IsConcatable(const NDArray& x, const NDArray& y, int64_t axis) {
  return IsConcatable(x->meta(), y->meta(), axis);
}

} // namespace hetu
