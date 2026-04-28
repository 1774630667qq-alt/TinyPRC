/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-19 15:28:00
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-20 13:56:11
 * @FilePath: /TinyRPC/src/RpcCodec.cpp
 * @Description: RPC 编解码器中间层实现
 */
#include "rpc/RpcCodec.hpp"
#include "ProtocolUtil.hpp"
#include "Buffer.hpp"
#include "TcpConnection.hpp"
#include "Logger.hpp"
#include <utility>

namespace MyRPC {

void RpcCodec::parseMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buffer) {
    while (buffer->readableBytes() > 0) {
        tiny_rpc::RpcMeta meta;
        std::string raw_body;
        size_t consumed_bytes = 0;

        // RpcCodec 是 Buffer 的唯一操控者：
        // 从 Buffer 中取出裸指针和可读长度，交给纯字节解析器 ProtocolUtil 处理
        DecodeStatus status = ProtocolUtil::Decode(
            buffer->peek(), buffer->readableBytes(),
            meta, raw_body, consumed_bytes);

        if (status == DecodeStatus::kSuccess) {
            // 由 RpcCodec 负责推进 Buffer 游标，丢弃已解析的数据
            buffer->retrieve(consumed_bytes);

            // 将切好的干净包抛给上层业务
            if (messageCallback_) {
                messageCallback_(conn, meta, std::move(raw_body));
            }
        } 
        else if (status == DecodeStatus::kHalfPacket) {
            // 数据不够，等待网络底层继续填充 Buffer
            break;
        } 
        else {
            // 脏数据：魔数不对或协议损坏
            LOG_ERROR << "RpcCodec: 协议解析失败，fd=" << conn->getFd() << "，强制断开连接";
            conn->forceClose();
            return;
        }
    }
}

} // namespace MyRPC

