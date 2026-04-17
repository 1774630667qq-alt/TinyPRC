/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 18:38:34
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-17 19:44:18
 * @FilePath: /TinyPRC/src/ProtocolUtil.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "ProtocolUtil.hpp"
#include <arpa/inet.h>

namespace MyRPC {
    std::string ProtocolUtil::Encode(RpcHeader header, const google::protobuf::Message& message) {
        std::string body;
        message.SerializeToString(&body);
        header.data_size = body.size();
        header.magic_num = htonl(header.magic_num);
        header.version = htonl(header.version);
        header.type = htonl(header.type);
        header.sequence_id = htonl(header.sequence_id);
        header.data_size = htonl(header.data_size);
        std::string result;
        // 优化策略： 预留空间，避免多次 realloc
        result.reserve(sizeof(header) + body.size());
        result.append((char*)&header, sizeof(header));
        result.append(body);
        return result;
    }

    DecodeStatus ProtocolUtil::Decode(const Buffer* buffer, RpcHeader &out_header, google::protobuf::Message &out_message) {
        DecodeStatus status = findPRCHeader(buffer, out_header);
        if (status != DecodeStatus::kSuccess) {
            return status;
        }

        if (buffer->readableBytes() < sizeof(RpcHeader) + out_header.data_size) {
            return DecodeStatus::kHalfPacket;
        }

        // 优化策略：零拷贝提取并解析 body
        const char* body_data = buffer->peek() + sizeof(RpcHeader);
        // 优化策略：使用 ParseFromArray 避免中间 string 拷贝
        if (out_message.ParseFromArray(body_data, out_header.data_size)) {
            return DecodeStatus::kSuccess;
        } else {
            return DecodeStatus::kError;
        }
    }

    DecodeStatus ProtocolUtil::findPRCHeader(const Buffer *buffer, RpcHeader &out_header) {
        if (buffer->readableBytes() < sizeof(RpcHeader)) {
            return DecodeStatus::kHalfPacket;
        }
        memcpy(&out_header, buffer->peek(), sizeof(RpcHeader));
        out_header.magic_num = ntohl(out_header.magic_num);
        out_header.version = ntohl(out_header.version);
        out_header.type = ntohl(out_header.type);
        out_header.sequence_id = ntohl(out_header.sequence_id);
        out_header.data_size = ntohl(out_header.data_size);
        if (out_header.magic_num != 998244353) {
            return DecodeStatus::kError;
        }
        return DecodeStatus::kSuccess;
    }
} // namespace MyRPC