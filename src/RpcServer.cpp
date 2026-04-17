/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 22:20:32
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-17 23:42:58
 * @FilePath: /TinyRPC/src/RpcServer.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "RpcServer.hpp"
#include "ProtocolUtil.hpp"
#include "TcpConnection.hpp"
#include "Logger.hpp"
#include "Status.hpp"
#include "order.pb.h"
#include <unistd.h>

namespace MyRPC {
RpcServer::RpcServer(EventLoop* loop, int port, ThreadPool* pool)
    : server_(loop, port),
      pool_(pool) {
    server_.setOnMessageCallback([this] (const std::shared_ptr<TcpConnection>& conn, Buffer* buffer) {
        onMessage(conn, buffer);
    });
}

void RpcServer::start() {
    server_.start();
}

void RpcServer::onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buffer) {
    while (buffer->readableBytes() > sizeof(RpcHeader)) {
        RpcHeader header;
        auto status = ProtocolUtil::findRPCHeader(buffer, header); 
        if (status == DecodeStatus::kError) {
            conn->forceClose();
            return;
        }
        
        uint32_t tot_size = sizeof(RpcHeader) + header.data_size;
        
        if (buffer->readableBytes() < tot_size) {
            // 半包，等待更多数据
            break;
        }

        std::string package_data = buffer->retrieveAsString(tot_size);
        
        pool_->enqueue([conn, package_data, header] () {
            tiny_rpc::OrderRequest req;
            
            const char* body_ptr = package_data.data() + sizeof(RpcHeader);
            
            if (!req.ParseFromArray(body_ptr, header.data_size)) {
                LOG_ERROR << "[Worker Thread] Protobuf 反序列化失败，序列号: " << header.sequence_id;

                // 1. 构造一个错误响应告诉客户端：“你的包我解不开，别等了！”
                tiny_rpc::OrderResponse err_resp;
                err_resp.set_ret_code(-1); // 约定 -1 或特定的状态码表示反序列化失败/Bad Request
                err_resp.set_res_info("RPC Error: Protobuf Parse Failed!");
                // err_resp.set_order_id() 就不设了，因为解不开

                // 2. 打包 Header，【必须】原样带上刚才的 sequence_id
                RpcHeader err_header;
                err_header.sequence_id = header.sequence_id; 
                err_header.type = 1; // 1 表示这是一条响应

                // 3. 序列化并安全投递回 IO 线程发送
                std::string err_str = MyRPC::ProtocolUtil::Encode(err_header, err_resp);
                EventLoop* io_loop = conn->getLoop();
                io_loop->queueInLoop([conn, err_str]() {
                    conn->send(err_str);

                    if (conn->recordBadRequest()) {
                        LOG_ERROR << "fd: " << conn->getFd() << " 多次发送错误请求，断开连接";
                        conn->forceClose();
                    }
                });

                return; // 错误响应发完后，这个任务完美结束
            }

            // =============== 业务逻辑 =================
            LOG_INFO << "[Worker Thread] 收到订单查询: " << req.order_id();

            // 假设此时数据库查询需要 1 秒钟
            sleep(1);

            // 构建响应
            tiny_rpc::OrderResponse resp;
            resp.set_ret_code(0);
            resp.set_res_info("Success");
            resp.set_order_id(req.order_id());

            RpcHeader resp_header;
            resp_header.sequence_id = header.sequence_id; // Seq ID 原样返回
            resp_header.type = 1; // 1 表示响应

            // 序列化
            std::string response_str = ProtocolUtil::Encode(resp_header, resp);

            // ======== 发送回客户端 ========
            // 【极其危险】：不要在 Worker 线程直接调 conn->send()，这可能会发生线程竞态！
            // 【正确做法】：像你以前 HttpServer 里的做法一样，投递回这个连接专属的 IO 线程去发！
            EventLoop* io_loop = conn->getLoop();
            io_loop->queueInLoop([conn, response_str]() {
                conn->clearBadRecord();
                conn->send(response_str);
            });
        });
    }
}
}