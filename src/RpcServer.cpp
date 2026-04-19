/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 22:20:32
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-19 15:30:00
 * @FilePath: /TinyRPC/src/RpcServer.cpp
 * @Description: RPC 服务器实现 — 纯业务路由层
 */
#include "RpcServer.hpp"
#include "ProtocolUtil.hpp"
#include "TcpConnection.hpp"
#include "Logger.hpp"
#include "order.pb.h"
#include <unistd.h>

namespace MyRPC {
RpcServer::RpcServer(EventLoop* loop, int port, ThreadPool* pool)
    : server_(loop, port),
      pool_(pool) {

    // 1. 设置 codec_ 的上层业务回调
    codec_.setMessageCallback([this] (const std::shared_ptr<TcpConnection>& conn,
                                      const tiny_rpc::RpcMeta& meta,
                                      const std::string& raw_body) {
        onRpcMessage(conn, meta, raw_body);
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
                              const std::string& raw_body) {
    // TODO: 后续实现动态路由分发
    //   1. 根据 meta.service_name() 查找已注册的 Service 对象
    //   2. 根据 meta.method_name() 找到该 Service 上的具体方法描述符
    //   3. 用 raw_body 反序列化出具体的 Request Message
    //   4. 扔进线程池 pool_ 执行业务逻辑
    //   5. 将执行结果通过 Encode 打包后投递回 IO 线程发送

    // ============ 临时硬编码业务逻辑（过渡阶段） ============
    // 捕获 meta 和 raw_body 的副本，交给线程池异步执行
    std::string body_copy = raw_body;
    tiny_rpc::RpcMeta meta_copy = meta;

    pool_->enqueue([conn, meta_copy = std::move(meta_copy), body_copy = std::move(body_copy)] () {
        tiny_rpc::OrderRequest req;
        
        if (!req.ParseFromString(body_copy)) {
            LOG_ERROR << "[Worker] Protobuf 反序列化失败，method=" << meta_copy.method_name()
                      << ", seq=" << meta_copy.sequence_id();

            // 构造错误响应
            tiny_rpc::OrderResponse err_resp;
            err_resp.set_ret_code(-1);
            err_resp.set_res_info("RPC Error: Protobuf Parse Failed!");

            // 打包并投递回 IO 线程发送
            tiny_rpc::RpcMeta resp_meta;
            resp_meta.set_sequence_id(meta_copy.sequence_id());
            resp_meta.set_service_name(meta_copy.service_name());
            resp_meta.set_method_name(meta_copy.method_name());
            resp_meta.set_type(1); // 1 = 响应

            std::string err_body;
            err_resp.SerializeToString(&err_body);
            std::string err_str = ProtocolUtil::Encode(resp_meta, err_body);

            EventLoop* io_loop = conn->getLoop();
            io_loop->queueInLoop([conn, err_str]() {
                conn->send(err_str);

                if (conn->recordBadRequest()) {
                    LOG_ERROR << "fd=" << conn->getFd() << " 多次发送错误请求，断开连接";
                    conn->forceClose();
                }
            });

            return;
        }

        // =============== 业务逻辑 =================
        LOG_INFO << "[Worker] 收到订单查询: " << req.order_id()
                 << ", method=" << meta_copy.method_name()
                 << ", seq=" << meta_copy.sequence_id();

        // 模拟耗时的数据库查询
        sleep(1);

        // 构建响应
        tiny_rpc::OrderResponse resp;
        resp.set_ret_code(0);
        resp.set_res_info("Success");
        resp.set_order_id(req.order_id());

        // 构建响应 Meta（sequence_id 原样返回）
        tiny_rpc::RpcMeta resp_meta;
        resp_meta.set_sequence_id(meta_copy.sequence_id());
        resp_meta.set_service_name(meta_copy.service_name());
        resp_meta.set_method_name(meta_copy.method_name());
        resp_meta.set_type(1); // 1 = 响应

        // 序列化并投递回 IO 线程发送
        std::string resp_body;
        resp.SerializeToString(&resp_body);
        std::string response_str = ProtocolUtil::Encode(resp_meta, resp_body);

        EventLoop* io_loop = conn->getLoop();
        io_loop->queueInLoop([conn, response_str]() {
            conn->clearBadRecord();
            conn->send(response_str);
        });
    });
}
} // namespace MyRPC