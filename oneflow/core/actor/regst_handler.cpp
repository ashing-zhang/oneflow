#include "oneflow/core/actor/regst_handler.h"
#include "oneflow/core/actor/op_actor.h"

namespace oneflow {

namespace actor {

void ActorMsgUtil::AsyncSendMsg(MsgDeliveryCtx* msg_ctx, const ActorMsg& msg) {
  // TODO: for all consumers of regst, send only 1 callback
  std::function<void()> callback = [msg]() { Global<ActorMsgBus>::Get()->SendMsg(msg); };
  if (Global<IDMgr>::Get()->GlobalWorkStreamId4ActorId(msg_ctx->actor_id)
      == Global<IDMgr>::Get()->GlobalWorkStreamId4ActorId(msg.dst_actor_id())) {
    callback();
  } else {
    msg_ctx->device_ctx->AddCallBack(callback);
  }
}

namespace {

HashMap<std::string, RegstHandlerCreateFn>* MutRegstHandlerRegistry() {
  static HashMap<std::string, RegstHandlerCreateFn> creators;
  return &creators;
}

}  // namespace

RegstHandlerRegistrar::RegstHandlerRegistrar(const std::string& handler_type,
                                             RegstHandlerCreateFn f) {
  auto* creators = MutRegstHandlerRegistry();
  creators->emplace(handler_type, f);
}

RegstHandlerIf* CreateRegstHandler(const std::string& handler_type) {
  RegstHandlerIf* ptr = MutRegstHandlerRegistry()->at(handler_type)();
  return ptr;
}

}  // namespace actor

}  // namespace oneflow
