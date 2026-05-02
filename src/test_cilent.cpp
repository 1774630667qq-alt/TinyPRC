/**
 * @brief 异步 RPC 客户端测试
 *
 * 使用 TcpClient + RpcChannel 实现基于 EventLoop 的异步长连接 RPC 调用。
 * 启动后自动连接服务器，连接成功后发起 OrderService.QueryOrder 调用，
 * 收到响应后打印结果并退出。
 *
 * 特性：
 * - 连接失败时自动指数退避重连（500ms → 1s → 2s → ... → 30s）
 * - 断连后自动重连
 * - 完全异步非阻塞
 */
#include "EventLoop.hpp"
#include "TcpClient.hpp"
#include "TcpConnection.hpp"
#include "rpc/RpcChannel.hpp"
#include "Logger.hpp"
#include "ModernClosure.hpp"
#include "order.pb.h"

int main() {
    MyRPC::EventLoop loop;
    MyRPC::TcpClient client(&loop, "127.0.0.1", 8080);
    MyRPC::RpcChannel channel(&client, &loop);

    // 注册 RpcChannel::onMessage 为 TcpClient 的消息回调
    client.setMessageCallback(
        [&channel](const std::shared_ptr<MyRPC::TcpConnection>& conn, MyRPC::Buffer* buf) {
            channel.onMessage(conn, buf);
        }
    );

    // 连接建立后发起 RPC 调用
    client.setConnectionCallback(
        [&channel, &loop](const std::shared_ptr<MyRPC::TcpConnection>& conn) {
            LOG_INFO << "✅ 已连接到服务器，准备发起 RPC 调用...";

            // 构造请求
            auto* req = new tiny_rpc::OrderRequest();
            auto* resp = new tiny_rpc::OrderResponse();
            req->set_order_id("ORD-998244353");

            // 构造完成回调
            auto* done = new MyRPC::ModernClosure(
                [req, resp, &loop]() {
                    LOG_INFO << "\n🎉🎉🎉 异步 RPC 调用成功！";
                    LOG_INFO << "📥 RetCode: " << resp->ret_code()
                             << ", Info: " << resp->res_info()
                             << ", OrderID: " << resp->order_id();
                    delete req;
                    delete resp;

                    // 收到响应后优雅退出
                    loop.quit();
                }
            );

            // 通过 Protobuf Stub 发起异步 RPC 调用
            tiny_rpc::OrderService_Stub stub(&channel);
            stub.QueryOrder(nullptr, req, resp, done);

            LOG_INFO << "📤 已发送 RPC 请求 (异步), 订单号: ORD-998244353";
        }
    );

    LOG_INFO << "🚀 启动 RPC 客户端，连接 127.0.0.1:8080 ...";
    client.connect();
    loop.loop();  // 进入事件循环

    LOG_INFO << "👋 客户端退出";
    return 0;
}
