/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 18:38:34
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-19 15:47:00
 * @FilePath: /TinyRPC/src/ProtocolUtil.cpp
 * @Description: RPC 协议编解码工具实现（纯字节解析器，不依赖 Buffer）
 */
#include "ProtocolUtil.hpp"
#include <arpa/inet.h>
#include <cstring>

namespace MyRPC {
    std::string ProtocolUtil::Encode(const tiny_rpc::RpcMeta& meta, const std::string& body) {
        std::string meta_str;
        meta.SerializeToString(&meta_str);

        RpcHeader header;
        header.magic_num = htonl(header.magic_num);
        header.version = htonl(header.version);
        header.meta_size = htonl(meta_str.size());
        header.data_size = htonl(body.size());
        
        std::string result;
        // 优化策略： 预留空间，避免多次 realloc
        result.reserve(sizeof(header) + meta_str.size() + body.size());
        result.append((char*)&header, sizeof(header));
        result.append(meta_str);
        result.append(body);
        return result;
    }

    DecodeStatus ProtocolUtil::Decode(const char* data, size_t len, tiny_rpc::RpcMeta& out_meta, std::string& raw_body, size_t& consumed_bytes) {
        RpcHeader out_header;
        DecodeStatus status = findRPCHeader(data, len, out_header);
        if (status != DecodeStatus::kSuccess) {
            return status;
        }

        // 计算整包长度：Header + Meta + Body
        size_t total_size = sizeof(RpcHeader) + out_header.meta_size + out_header.data_size;

        if (len < total_size) {
            return DecodeStatus::kHalfPacket;
        }

        const uint32_t MAX_RPC_BODY_SIZE = 10 * 1024 * 1024;
        
        if (out_header.data_size > MAX_RPC_BODY_SIZE || out_header.meta_size > MAX_RPC_BODY_SIZE) {
            return DecodeStatus::kError;
        }

        // 零拷贝解析 RpcMeta
        const char* meta_data = data + sizeof(RpcHeader);
        if (!out_meta.ParseFromArray(meta_data, out_header.meta_size)) {
            return DecodeStatus::kError;
        }

        // 截取 raw_body 原始业务二进制数据（不反序列化，交给上层处理）
        const char* body_data = meta_data + out_header.meta_size;
        raw_body.assign(body_data, out_header.data_size);

        consumed_bytes = total_size;
        return DecodeStatus::kSuccess;
    }

    DecodeStatus ProtocolUtil::findRPCHeader(const char* data, size_t len, RpcHeader &out_header) {
        if (len < sizeof(RpcHeader)) {
            return DecodeStatus::kHalfPacket;
        }
        memcpy(&out_header, data, sizeof(RpcHeader));
        out_header.magic_num = ntohl(out_header.magic_num);
        out_header.version = ntohl(out_header.version);
        out_header.meta_size = ntohl(out_header.meta_size);
        out_header.data_size = ntohl(out_header.data_size);
        if (out_header.magic_num != 998244353) {
            return DecodeStatus::kError;
        }
        return DecodeStatus::kSuccess;
    }
} // namespace MyRPC