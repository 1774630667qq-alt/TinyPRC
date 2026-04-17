/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 22:20:21
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-17 23:33:44
 * @FilePath: /TinyRPC/include/RpcServer.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include "TcpServer.hpp"
#include "ThreadPool.hpp"
#include "Buffer.hpp"
#include <memory>

namespace MyRPC {

class RpcServer {
public:
    // 构造函数需要传入业务线程池
    RpcServer(EventLoop* loop, int port, ThreadPool* pool);
    
    void start();
    
    // 配置底层的 Sub-Reactor 数量
    void setThreadNum(int numThreads);

private:
    TcpServer server_;
    ThreadPool* pool_; // 业务后厨线程池

    // 收到网络数据时的入口回调
    void onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buffer);
};

} // namespace MyRPC