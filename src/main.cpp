/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 17:19:46
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-28 16:17:19
 * @FilePath: /TinyRPC/src/main.cpp
 * @Description: ProtocolUtil 编解码 + 切包循环单元测试
 */
// server_main.cpp
#include "EventLoop.hpp"
#include "ThreadPool.hpp"
#include "RpcServer.hpp"
#include "rpc/OrderServiceImpl.hpp"

int main() {
    // 1. 初始化底层网络与线程池
    MyRPC::EventLoop loop;
    MyRPC::ThreadPool pool(4); // 4个 Worker 线程
    
    // 2. 初始化 RPC 服务器 (监听 8080 端口)
    MyRPC::RpcServer server(&loop, 8080, &pool);

    // 3. 注册服务 (全自动反射获取名字)
    tiny_rpc::OrderService* order_service = new MyRPC::OrderServiceImpl();
    server.registerService(order_service);

    // 4. 启动！
    server.start();
    std::cout << "🚀 RPC Server 已启动，监听 8080 端口..." << std::endl;
    loop.loop(); // 死循环，监听 Epoll 事件

    return 0;
}