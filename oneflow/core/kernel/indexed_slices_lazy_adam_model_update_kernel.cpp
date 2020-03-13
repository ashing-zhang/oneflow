#include "oneflow/core/kernel/kernel.h"
#include "oneflow/core/kernel/kernel_context.h"
#include "oneflow/core/kernel/indexed_slices_reduce_sum_kernel_util.h"
#include "oneflow/core/kernel/indexed_slices_lazy_adam_model_update_kernel_util.h"

namespace oneflow {

template<DeviceType device_type, typename T, typename K>
class IndexedSlicesLazyAdamMdUpdateKernel final : public KernelIf<device_type> {
 public:
  OF_DISALLOW_COPY_AND_MOVE(IndexedSlicesLazyAdamMdUpdateKernel);
  IndexedSlicesLazyAdamMdUpdateKernel() = default;
  ~IndexedSlicesLazyAdamMdUpdateKernel() override = default;

 private:
  using ReduceSumUtilT = IndexedSlicesReduceSumKernelUtil<device_type, K, T, int32_t>;
  using MdUpdateUtilT = IndexedSlicesLazyAdamMdUpdateKernelUtil<device_type, T, K, int32_t>;
  void ForwardDataContent(const KernelCtx&,
                          std::function<Blob*(const std::string&)>) const override;
};

template<DeviceType device_type, typename T, typename K>
void IndexedSlicesLazyAdamMdUpdateKernel<device_type, T, K>::ForwardDataContent(
    const KernelCtx& ctx, std::function<Blob*(const std::string&)> BnInOp2Blob) const {
  const IndexedSlicesLazyAdamModelUpdateOpConf& conf =
      this->op_conf().indexed_slices_lazy_adam_model_update_conf();
  const T beta1 = static_cast<T>(conf.beta1());
  const T beta2 = static_cast<T>(conf.beta2());
  const T epsilon = static_cast<T>(conf.epsilon());
  const int64_t* train_step_ptr = BnInOp2Blob("train_step")->dptr<int64_t>();
  const float* learning_rate_ptr = BnInOp2Blob("learning_rate")->dptr<float>();
  float* local_learning_rate_ptr = BnInOp2Blob("local_learning_rate")->mut_dptr<float>();
  const Blob* diff_indices = BnInOp2Blob("model_diff_indices");
  const Blob* diff_values = BnInOp2Blob("model_diff_values");
  const int64_t num_indices = diff_indices->shape().elem_cnt();
  const int64_t num_values = diff_values->shape().elem_cnt();
  CHECK_EQ(num_values % num_indices, 0);
  const int64_t feature_size = num_values / num_indices;
  Blob* unique_diff_indices = BnInOp2Blob("unique_diff_indices");
  Blob* unique_diff_values = BnInOp2Blob("unique_diff_values");
  Blob* num_unique_diff_indices = BnInOp2Blob("num_unique_diff_indices");
  Blob* unique_workspace = BnInOp2Blob("unique_workspace");
  const int64_t lower_bound =
      this->kernel_conf().indexed_slices_lazy_adam_model_update_conf().lower_bound();
  const int64_t upper_bound =
      this->kernel_conf().indexed_slices_lazy_adam_model_update_conf().upper_bound();
  const K* unique_diff_indices_ptr = nullptr;
  const T* unique_diff_values_ptr = nullptr;
  const int32_t* num_unique_indices_ptr = nullptr;
  if (conf.no_duplicates_in_indices()) {
    unique_diff_indices_ptr = diff_indices->dptr<K>();
    unique_diff_values_ptr = diff_values->dptr<T>();
  } else {
    ReduceSumUtilT::ReduceSum(ctx.device_ctx, num_indices, feature_size, diff_indices->dptr<K>(),
                              diff_values->dptr<T>(), num_unique_diff_indices->mut_dptr<int32_t>(),
                              unique_diff_indices->mut_dptr<K>(), unique_diff_values->mut_dptr<T>(),
                              unique_workspace->mut_dptr(), unique_workspace->ByteSizeOfBlobBody());
    unique_diff_indices_ptr = unique_diff_indices->dptr<K>();
    unique_diff_values_ptr = unique_diff_values->mut_dptr<T>();
    num_unique_indices_ptr = unique_diff_indices->dptr<int32_t>();
  }
  MdUpdateUtilT::ComputeLocalLearningRate(ctx.device_ctx, beta1, beta2, train_step_ptr,
                                          learning_rate_ptr, local_learning_rate_ptr);
  MdUpdateUtilT::Update(ctx.device_ctx, beta1, beta2, epsilon, num_indices, feature_size,
                        lower_bound, upper_bound, num_unique_indices_ptr, train_step_ptr,
                        local_learning_rate_ptr, unique_diff_indices_ptr, unique_diff_values_ptr,
                        BnInOp2Blob("model")->mut_dptr<T>(), BnInOp2Blob("m")->mut_dptr<T>(),
                        BnInOp2Blob("v")->mut_dptr<T>());
}

#define MAKE_INDEXED_SLICES_LAZY_ADAM_MODEL_UPDATE_KERNEL_ENTRY(device_type_v, data_type_pair,     \
                                                                indices_type_pair)                 \
  NEW_REGISTER_KERNEL(                                                                             \
      OperatorConf::kIndexedSlicesLazyAdamModelUpdateConf,                                         \
      IndexedSlicesLazyAdamMdUpdateKernel<device_type_v, OF_PP_PAIR_FIRST(data_type_pair),         \
                                          OF_PP_PAIR_FIRST(indices_type_pair)>)                    \
      .SetIsMatchedPred([](const KernelConf& kernel_conf) -> bool {                                \
        return (                                                                                   \
            (kernel_conf.op_attribute().op_conf().device_type() == device_type_v)                  \
            && ((OF_PP_PAIR_SECOND(data_type_pair)) == kernel_conf.data_type())                    \
            && (OF_PP_PAIR_SECOND(indices_type_pair)                                               \
                == kernel_conf.indexed_slices_lazy_adam_model_update_conf().indices_data_type())); \
      });

OF_PP_SEQ_PRODUCT_FOR_EACH_TUPLE(MAKE_INDEXED_SLICES_LAZY_ADAM_MODEL_UPDATE_KERNEL_ENTRY,
                                 DEVICE_TYPE_SEQ, FLOATING_DATA_TYPE_SEQ, INDEX_DATA_TYPE_SEQ)
#undef MAKE_INDEXED_SLICES_LAZY_ADAM_MODEL_UPDATE_KERNEL_ENTRY

}  // namespace oneflow