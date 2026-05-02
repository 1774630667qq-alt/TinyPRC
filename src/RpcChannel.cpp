#include "rpc/RpcChannel.hpp"
#include "TcpClient.hpp"
#include "TcpConnection.hpp"
#include "ProtocolUtil.hpp"
#include "Logger.hpp"
#include "Buffer.hpp"
#include "rpc_meta.pb.h"

namespace MyRPC {

RpcChannel::RpcChannel(TcpClient* client, EventLoop* loop)
    : client_(client), loop_(loop) {
    // 设置 codec_ 的上层回调：当完整 RPC 帧被切出后，路由到 onRpcResponse
    codec_.setMessageCallback(
        [this](const std::shared_ptr<TcpConnection>& conn,
               const tiny_rpc::RpcMeta& meta,
               std::string raw_body) {
            onRpcResponse(conn, meta, std::move(raw_body));
        }
    );
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const google::protobuf::Message* request,
                            google::protobuf::Message* response,
                            google::protobuf::Closure* done) {

    // 1. 生成唯一 sequence_id（原子自增，线程安全）
    uint32_t seqId = nextSequenceId_.fetch_add(1);

    // 2. 组装 RpcMeta
    tiny_rpc::RpcMeta meta;
    meta.set_service_name(method->service()->full_name());
    meta.set_method_name(method->name());
    meta.set_sequence_id(seqId);
    meta.set_type(0); // 0 = 请求

    // 3. 序列化请求体
    std::string req_body;
    request->SerializeToString(&req_body);

    // 4. 编码为完整的二进制帧
    std::string packet = ProtocolUtil::Encode(meta, req_body);

    // 5. 将 {response, done} 存入等待表
    {
        std::lock_guard<std::mutex> lock(mutex_);
        outstandings_[seqId] = {response, done};
    }

    LOG_INFO << "RpcChannel: 发送 RPC 请求, service=" << meta.service_name()
             << ", method=" << meta.method_name()
             << ", seqId=" << seqId;

    // 6. 通过 TcpClient 发送（线程安全）
    client_->send(packet);

    // 7. 立即返回！不阻塞等待回包
}

void RpcChannel::onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buffer) {
    // 委托 RpcCodec 进行切包，切出完整帧后会回调 onRpcResponse
    codec_.parseMessage(conn, buffer);
}

void RpcChannel::onRpcResponse(const std::shared_ptr<TcpConnection>& conn,
                               const tiny_rpc::RpcMeta& meta,
                               std::string raw_body) {
    uint32_t seqId = meta.sequence_id();

    // 1. 根据 sequence_id 从等待表中查找对应的请求上下文
    OutstandingCall call;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = outstandings_.find(seqId);
        if (it == outstandings_.end()) {
            LOG_WARNING << "RpcChannel: 收到未知 seqId=" << seqId << " 的响应，丢弃";
            return;
        }
        call = it->second;
        outstandings_.erase(it);
    }

    // 2. 检查是否为错误响应
    if (meta.type() == 2) {
        LOG_ERROR << "RpcChannel: 服务端返回错误, seqId=" << seqId
                  << ", code=" << meta.err_code()
                  << ", msg=" << meta.err_msg();
        // 仍然执行 done 回调，让调用方知道出错了
        if (call.done) {
            call.done->Run();
        }
        return;
    }

    // 3. 反序列化响应体到调用方准备好的 response 指针
    if (!call.response->ParseFromString(raw_body)) {
        LOG_ERROR << "RpcChannel: 响应体反序列化失败, seqId=" << seqId;
    } else {
        LOG_INFO << "RpcChannel: 收到 RPC 响应, seqId=" << seqId;
    }

    // 4. 执行完成回调
    if (call.done) {
        call.done->Run();
    }
}

} // namespace MyRPC
