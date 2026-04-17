/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 18:38:10
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-17 19:43:43
 * @FilePath: /TinyPRC/include/ProtocolUtil.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <string>
#include "rpc/RpcHeader.hpp"
#include "Status.hpp"
#include "Buffer.hpp"
#include <google/protobuf/message.h>

namespace MyRPC {
    struct ProtocolUtil {
        /**
         * @brief 将 RPC 协议头和 Protobuf 消息体序列化编码为二进制字节流（包含网络字节序转换）
         * @signature static std::string Encode(RpcHeader header, const google::protobuf::Message& message)
         * @param header 要发送的 RPC 协议头，包含魔数、版本号、消息类型等字段
         * @param message 需要发送的具体 Protobuf 消息对象
         * @return 包含序列化后协议头和消息体的完整二进制字节流
         */
        static std::string Encode(RpcHeader header, const google::protobuf::Message& message);

        /**
         * @brief 将接收到的二进制字节流解码为 RPC 协议头和 Protobuf 消息体（包含主机字节序转换和校验）
         * @signature static bool Decode(const std::string& input_data, RpcHeader& out_header, google::protobuf::Message& out_message)
         * @param input_data 从网络接收到的原始二进制字节流数据
         * @param out_header 用于存储解析出并转换为主机字节序的 RPC 协议头部信息
         * @param out_message 用于存储反序列化后的具体 Protobuf 业务消息对象
         * @return 若解析成功、魔数正确且数据总长度完整则返回 true，否则返回 false
         */
        static DecodeStatus Decode(const Buffer* buffer, RpcHeader& out_header, google::protobuf::Message& out_message);

        static DecodeStatus findPRCHeader(const Buffer* buffer, RpcHeader& out_header);
    };
} // namespace MyRPC