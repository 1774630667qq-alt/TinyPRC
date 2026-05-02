/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 22:20:21
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-28 17:52:35
 * @FilePath: /TinyRPC/include/RpcServer.hpp
 * @Description: RPC 服务器 — 纯业务路由层
 */
#pragma once
#include "TcpServer.hpp"
#include "ThreadPool.hpp"
#include "rpc/RpcCodec.hpp"
#include <google/protobuf/service.h>
#include <memory>
#include <unordered_map>

namespace MyRPC {

class RpcServer {
public:
    /// 构造函数：需要传入事件循环、监听端口和业务线程池
    RpcServer(EventLoop* loop, int port, ThreadPool* pool);
    std::unordered_map<std::string, google::protobuf::Service*> services_;
    void start();
    
    /// 配置底层的 Sub-Reactor 数量
    void setThreadNum(int numThreads);
    void registerService(google::protobuf::Service* service);

private:
    TcpServer server_;
    ThreadPool* pool_;      ///< 业务后厨线程池
    RpcCodec codec_;        ///< 编解码器中间层：负责切包

    /**
     * @brief 纯业务回调：接收已切好的完整 RPC 包
     * @param conn 当前 TCP 连接
     * @param meta 已解析的 RPC 协议元数据
     * @param raw_body 原始业务二进制数据（由本方法负责反序列化）
     */
    void onRpcMessage(const std::shared_ptr<TcpConnection>& conn,
                      const tiny_rpc::RpcMeta& meta,
                      std::string raw_body);

    /**
     * @brief 构造并发送一个错误响应帧给客户端
     * @signature void sendErrorResponse(const std::shared_ptr<TcpConnection>& conn, const tiny_rpc::RpcMeta& meta, int32_t err_code, const std::string& err_msg)
     * @param conn 当前 TCP 连接
     * @param meta 原始请求的 RpcMeta（用于复制路由三元组：service_name, method_name, sequence_id）
     * @param err_code 错误码
     * @param err_msg 错误描述信息
     */
    void sendErrorResponse(const std::shared_ptr<TcpConnection>& conn,
                           const tiny_rpc::RpcMeta& meta,
                           int32_t err_code,
                           const std::string& err_msg);

};

} // namespace MyRPC