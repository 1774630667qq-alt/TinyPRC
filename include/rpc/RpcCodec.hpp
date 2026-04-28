/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-19 15:28:00
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-20 13:56:43
 * @FilePath: /TinyRPC/include/rpc/RpcCodec.hpp
 * @Description: RPC 编解码器中间层，负责从字节流中切出完整的 RPC 包
 */
#pragma once
#include <functional>
#include <memory>
#include <string>
#include "rpc_meta.pb.h"

namespace MyRPC {

class TcpConnection;
class Buffer;

/**
 * @brief 高级业务回调类型：当一个完整的 RPC 包被成功切出后触发
 * @param conn 当前 TCP 连接的智能指针
 * @param meta 已解析的 RPC 协议元数据（service_name, method_name, sequence_id 等）
 * @param raw_body 原始业务二进制数据（未反序列化，由上层自行处理）
 */
using RpcMessageCallback = std::function<void(
    const std::shared_ptr<TcpConnection>& conn,
    const tiny_rpc::RpcMeta& meta,
    std::string raw_body
)>;

/**
 * @brief RPC 编解码器：接管底层 Buffer 的切包脏活
 * 
 * 该类实现了经典的"编解码器 / 协议分发器"模式，作为网络 IO 层（TcpServer）
 * 与业务路由层（RpcServer）之间的中间层。
 * 
 * 职责：
 * 1. 从 Buffer 中循环切出完整的 RPC 协议帧
 * 2. 处理半包（等待更多数据）和脏数据（强制断开连接）
 * 3. 将切好的干净数据包（RpcMeta + raw_body）通过回调抛给上层
 */
class RpcCodec {
public:
    /**
     * @brief 设置上层业务回调
     * @param cb 当完整 RPC 包被切出后触发的回调函数
     */
    void setMessageCallback(RpcMessageCallback cb) {
        messageCallback_ = std::move(cb);
    }

    /**
     * @brief 核心方法：接管底层 Buffer，执行切包循环
     * @signature void parseMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buffer)
     * @param conn 当前 TCP 连接
     * @param buffer 底层网络接收缓冲区
     * 
     * 内部逻辑：
     * - kSuccess：推进 Buffer 游标，触发 RpcMessageCallback
     * - kHalfPacket：break 等待网络底层继续填充
     * - kError：记录错误日志，强制断开连接
     */
    void parseMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buffer);

private:
    RpcMessageCallback messageCallback_;
};

} // namespace MyRPC
