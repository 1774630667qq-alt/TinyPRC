/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 22:20:32
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-28 17:05:46
 * @FilePath: /TinyRPC/src/RpcServer.cpp
 * @Description: RPC 服务器实现 — 纯业务路由层
 */
#include "RpcServer.hpp"
#include "EventLoop.hpp"
#include "TcpConnection.hpp"
#include "ProtocolUtil.hpp"
#include "Logger.hpp"
#include "order.pb.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <unistd.h>
#include "ModernClosure.hpp"
#include "Logger.hpp"

namespace MyRPC {
RpcServer::RpcServer(EventLoop* loop, int port, ThreadPool* pool)
    : server_(loop, port),
      pool_(pool) {

    // 1. 设置 codec_ 的上层业务回调
    codec_.setMessageCallback([this] (const std::shared_ptr<TcpConnection>& conn,
                                      const tiny_rpc::RpcMeta& meta,
                                      std::string raw_body) {
        onRpcMessage(conn, meta, std::move(raw_body));
    });

    // 2. 将底层 TcpServer 的原始字节流回调全部委托给 codec_
    server_.setOnMessageCallback([this] (const std::shared_ptr<TcpConnection>& conn, Buffer* buffer) {
        codec_.parseMessage(conn, buffer);
    });
}

void RpcServer::start() {
    server_.start();
}

void RpcServer::onRpcMessage(const std::shared_ptr<TcpConnection>& conn,
                              const tiny_rpc::RpcMeta& meta,
                              std::string raw_body) {
    // ============ 临时硬编码业务逻辑（过渡阶段） ============
    // 捕获 meta 和 raw_body 的副本，交给线程池异步执行
    if (services_.find(meta.service_name()) == services_.end()) {
        LOG_ERROR << "未注册服务: " << meta.service_name();
        return;
    }

    pool_->enqueue([conn, meta_copy = std::move(meta), body_copy = std::move(raw_body),
                    service = services_[meta.service_name()]] () {
        auto method = service->GetDescriptor()->FindMethodByName(meta_copy.method_name());

        google::protobuf::Message* request = service->GetRequestPrototype(method).New();
        google::protobuf::Message* response = service->GetResponsePrototype(method).New();

        request->ParseFromString(body_copy);

        google::protobuf::Closure* done = new MyRPC::ModernClosure(
            [conn, request, response, meta_copy] () {
                // 序列化业务响应，并封装成完整 RPC 响应帧
                std::string body;
                response->SerializeToString(&body);

                tiny_rpc::RpcMeta resp_meta;
                resp_meta.set_service_name(meta_copy.service_name());
                resp_meta.set_method_name(meta_copy.method_name());
                resp_meta.set_sequence_id(meta_copy.sequence_id());
                resp_meta.set_type(1);

                std::string packet = MyRPC::ProtocolUtil::Encode(resp_meta, body);
                EventLoop* loop = conn->getLoop();

                loop->queueInLoop([conn, packet] () {
                    LOG_INFO << "开始发送 RPC 响应包，长度：" << packet.size();
                    conn->send(packet);
                });
                delete request;
                delete response;
            }
        );
        service->CallMethod(method, nullptr, request, response, done);
    });
}

void RpcServer::registerService(google::protobuf::Service* service) {
    const google::protobuf::ServiceDescriptor* descriptor = service->GetDescriptor();
    
    std::string service_name = descriptor->full_name();
    
    if (services_.find(service_name) != services_.end()) {
        LOG_WARNING << "重复注册服务: " << service_name;
        return;
    }

    services_[service_name] = service;

    LOG_INFO << "注册服务: " << service_name;
}
} // namespace MyRPC
