#include "oneflow/core/kernel/reduce_concat_kernel.h"

namespace oneflow {

template<DeviceType device_type>
void ReduceConcatKernel<device_type>::ForwardDataContent(
    const KernelCtx& ctx, std::function<Blob*(const std::string&)> BnInOp2Blob) const {
  if (!(this->job_desc().IsPredict()
        && this->job_desc().job_conf().predict_conf().has_tmp_split_fw_bw_train_conf())
      && device_type == DeviceType::kGPU && this->job_desc().enable_mem_sharing()) {
    return;
  }
  Blob* out_blob = BnInOp2Blob("out");
  FOR_RANGE(int, in_bn_id, 0, this->op_attribute().input_bns().size()) {
    char* dst_cur_dptr = out_blob->mut_dptr<char>()
                         + this->kernel_conf().reduce_concat_conf().data_offset().Get(in_bn_id);
    Blob* in_blob = BnInOp2Blob(this->op_attribute().input_bns().Get(in_bn_id));
    size_t in_byte_size = in_blob->ByteSizeOfDataContentField();
    Memcpy<device_type>(ctx.device_ctx, dst_cur_dptr, in_blob->dptr<char>(), in_byte_size);
  }
}

ADD_DEVICE_TYPE_KERNEL_CREATOR(OperatorConf::kReduceConcatConf, ReduceConcatKernel);

}  // namespace oneflow
