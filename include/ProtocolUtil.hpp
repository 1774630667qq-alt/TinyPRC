/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 18:38:10
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-19 14:33:13
 * @FilePath: /TinyPRC/include/ProtocolUtil.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <string>
#include <cstddef>
#include "rpc/RpcHeader.hpp"
#include "Status.hpp"
#include "rpc_meta.pb.h"

namespace MyRPC {
    struct ProtocolUtil {
        /**
         * @brief 将 RpcMeta 和 Protobuf 业务消息编码为完整的二进制帧（RpcHeader + Meta + Body）
         * @signature static std::string Encode(const tiny_rpc::RpcMeta& meta, const std::string& body)
         * @param meta RPC 协议元数据（service_name, method_name, sequence_id 等）
         * @param body 已经序列化好的业务消息二进制数据
         * @return 包含 RpcHeader + 序列化 Meta + Body 的完整二进制字节流
         */
        static std::string Encode(const tiny_rpc::RpcMeta& meta, const std::string& body);

        /**
         * @brief 从原始字节流中解码一个完整的 RPC 协议帧（纯字节解析器，不依赖 Buffer）
         * @signature static DecodeStatus Decode(const char* data, size_t len, tiny_rpc::RpcMeta& out_meta, std::string& raw_body, size_t& consumed_bytes)
         * @param data 原始字节流起始指针
         * @param len 可读字节数
         * @param out_meta 输出参数：解析出的 RPC 协议元数据
         * @param raw_body 输出参数：截取出的原始业务二进制数据
         * @param consumed_bytes 输出参数：本次解析消耗的总字节数
         * @return kSuccess/kHalfPacket/kError 三态解码结果
         */
        static DecodeStatus Decode(const char* data, size_t len, tiny_rpc::RpcMeta& out_meta, std::string& raw_body, size_t& consumed_bytes);

        /**
         * @brief 从原始字节流中解析 RPC 固定协议头（含网络字节序转换和魔数校验）
         * @param data 原始字节流起始指针
         * @param len 可读字节数
         * @param out_header 输出参数：解析出的 RpcHeader
         * @return kSuccess/kHalfPacket/kError
         */
        static DecodeStatus findRPCHeader(const char* data, size_t len, RpcHeader& out_header);
    };
} // namespace MyRPC