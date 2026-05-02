#pragma once
#include <google/protobuf/service.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include "rpc/RpcCodec.hpp"

namespace MyRPC {

class TcpClient;
class TcpConnection;
class Buffer;
class EventLoop;

/**
 * @brief 异步 RPC 通道：基于 TcpClient 的长连接异步 RPC 调用
 *
 * 核心设计：
 * - CallMethod 不再阻塞等待回包，而是将 {response, done} 存入 outstandings_ map，
 *   通过 TcpClient 发送请求后立即返回
 * - onMessage 作为 TcpClient 的 MessageCallback，接收服务端回包，
 *   根据 sequence_id 路由到对应的 response 和 done 回调
 *
 * 线程安全：
 * - sequence_id 使用 atomic 自增
 * - outstandings_ map 使用 mutex 保护
 */
class RpcChannel : public google::protobuf::RpcChannel {
public:
    /**
     * @brief 构造函数
     * @param client TcpClient 指针（RpcChannel 不拥有其生命周期）
     * @param loop   事件循环（用于调度回调）
     */
    RpcChannel(TcpClient* client, EventLoop* loop);
    ~RpcChannel() override = default;

    /**
     * @brief 异步发起 RPC 调用
     * @signature void CallMethod(const MethodDescriptor*, RpcController*, const Message*, Message*, Closure*)
     * @param method   Protobuf 方法描述符（自动获取 service_name 和 method_name）
     * @param controller RPC 控制器（当前未使用）
     * @param request  已填充的请求消息（由调用方分配）
     * @param response 空的响应消息指针（由调用方分配，异步回调后填充）
     * @param done     完成回调（异步回包后由 onMessage 执行）
     */
    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    google::protobuf::RpcController* controller,
                    const google::protobuf::Message* request,
                    google::protobuf::Message* response,
                    google::protobuf::Closure* done) override;

    /**
     * @brief 处理服务端回包（注册为 TcpClient 的 MessageCallback）
     * @param conn   当前 TCP 连接
     * @param buffer 接收缓冲区
     */
    void onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buffer);

private:
    /**
     * @brief 处理解码后的完整 RPC 响应帧
     * @param conn     当前连接
     * @param meta     响应帧的 RpcMeta
     * @param raw_body 响应的原始业务二进制数据
     */
    void onRpcResponse(const std::shared_ptr<TcpConnection>& conn,
                       const tiny_rpc::RpcMeta& meta,
                       std::string raw_body);

    /**
     * @brief 异步请求上下文：保存等待回包的 response 和 done
     */
    struct OutstandingCall {
        google::protobuf::Message* response;   ///< 由调用方分配的响应消息指针
        google::protobuf::Closure* done;       ///< 完成回调
    };

    TcpClient* client_;                              ///< 底层网络传输
    EventLoop* loop_;                                ///< 事件循环
    RpcCodec codec_;                                 ///< 编解码器（切包）

    std::atomic<uint32_t> nextSequenceId_{1};         ///< 原子递增的序列号生成器
    std::mutex mutex_;                                ///< 保护 outstandings_ map
    std::unordered_map<uint32_t, OutstandingCall> outstandings_; ///< 等待回包的请求表
};

} // namespace MyRPC