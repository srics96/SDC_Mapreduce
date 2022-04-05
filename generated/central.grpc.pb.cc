// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: central.proto

#include "central.pb.h"
#include "central.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/message_allocator.h>
#include <grpcpp/impl/codegen/method_handler.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/server_callback_handlers.h>
#include <grpcpp/impl/codegen/server_context.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace mapr {

static const char* WorkerService_method_names[] = {
  "/mapr.WorkerService/execute_task",
};

std::unique_ptr< WorkerService::Stub> WorkerService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< WorkerService::Stub> stub(new WorkerService::Stub(channel));
  return stub;
}

WorkerService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_execute_task_(WorkerService_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status WorkerService::Stub::execute_task(::grpc::ClientContext* context, const ::mapr::Task& request, ::mapr::TaskReception* response) {
  return ::grpc::internal::BlockingUnaryCall< ::mapr::Task, ::mapr::TaskReception, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_execute_task_, context, request, response);
}

void WorkerService::Stub::experimental_async::execute_task(::grpc::ClientContext* context, const ::mapr::Task* request, ::mapr::TaskReception* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::mapr::Task, ::mapr::TaskReception, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_execute_task_, context, request, response, std::move(f));
}

void WorkerService::Stub::experimental_async::execute_task(::grpc::ClientContext* context, const ::mapr::Task* request, ::mapr::TaskReception* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_execute_task_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::mapr::TaskReception>* WorkerService::Stub::PrepareAsyncexecute_taskRaw(::grpc::ClientContext* context, const ::mapr::Task& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::mapr::TaskReception, ::mapr::Task, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_execute_task_, context, request);
}

::grpc::ClientAsyncResponseReader< ::mapr::TaskReception>* WorkerService::Stub::Asyncexecute_taskRaw(::grpc::ClientContext* context, const ::mapr::Task& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncexecute_taskRaw(context, request, cq);
  result->StartCall();
  return result;
}

WorkerService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      WorkerService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< WorkerService::Service, ::mapr::Task, ::mapr::TaskReception, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](WorkerService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::mapr::Task* req,
             ::mapr::TaskReception* resp) {
               return service->execute_task(ctx, req, resp);
             }, this)));
}

WorkerService::Service::~Service() {
}

::grpc::Status WorkerService::Service::execute_task(::grpc::ServerContext* context, const ::mapr::Task* request, ::mapr::TaskReception* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


static const char* MasterService_method_names[] = {
  "/mapr.MasterService/task_complete",
};

std::unique_ptr< MasterService::Stub> MasterService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< MasterService::Stub> stub(new MasterService::Stub(channel));
  return stub;
}

MasterService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_task_complete_(MasterService_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status MasterService::Stub::task_complete(::grpc::ClientContext* context, const ::mapr::TaskCompletion& request, ::mapr::TaskCompletionAck* response) {
  return ::grpc::internal::BlockingUnaryCall< ::mapr::TaskCompletion, ::mapr::TaskCompletionAck, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_task_complete_, context, request, response);
}

void MasterService::Stub::experimental_async::task_complete(::grpc::ClientContext* context, const ::mapr::TaskCompletion* request, ::mapr::TaskCompletionAck* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::mapr::TaskCompletion, ::mapr::TaskCompletionAck, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_task_complete_, context, request, response, std::move(f));
}

void MasterService::Stub::experimental_async::task_complete(::grpc::ClientContext* context, const ::mapr::TaskCompletion* request, ::mapr::TaskCompletionAck* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_task_complete_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::mapr::TaskCompletionAck>* MasterService::Stub::PrepareAsynctask_completeRaw(::grpc::ClientContext* context, const ::mapr::TaskCompletion& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::mapr::TaskCompletionAck, ::mapr::TaskCompletion, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_task_complete_, context, request);
}

::grpc::ClientAsyncResponseReader< ::mapr::TaskCompletionAck>* MasterService::Stub::Asynctask_completeRaw(::grpc::ClientContext* context, const ::mapr::TaskCompletion& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsynctask_completeRaw(context, request, cq);
  result->StartCall();
  return result;
}

MasterService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      MasterService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< MasterService::Service, ::mapr::TaskCompletion, ::mapr::TaskCompletionAck, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](MasterService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::mapr::TaskCompletion* req,
             ::mapr::TaskCompletionAck* resp) {
               return service->task_complete(ctx, req, resp);
             }, this)));
}

MasterService::Service::~Service() {
}

::grpc::Status MasterService::Service::task_complete(::grpc::ServerContext* context, const ::mapr::TaskCompletion* request, ::mapr::TaskCompletionAck* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace mapr

