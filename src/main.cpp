/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 17:19:46
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-19 15:30:00
 * @FilePath: /TinyRPC/src/main.cpp
 * @Description: ProtocolUtil 编解码 + 切包循环单元测试
 */
#include <iostream>
#include "order.pb.h"
#include "rpc_meta.pb.h"
#include "Buffer.hpp"
#include "ProtocolUtil.hpp"

int main() {
    MyRPC::Buffer buffer;
    
    // 1. 构造一个正常的请求并 Encode 放入 buffer
    tiny_rpc::OrderRequest req1;
    req1.set_order_id("Order-9527");
    req1.set_item_count(3);
    req1.set_price(299.9);

    tiny_rpc::RpcMeta meta1;
    meta1.set_method_name("QueryOrder");
    meta1.set_service_name("OrderService");
    meta1.set_sequence_id(0);
    meta1.set_type(0); // 0 = 请求
    meta1.set_timeout_ms(1000);
    meta1.set_trace_id("trace-001");
    
    // Encode 的第二个参数现在是已序列化好的 body string
    std::string body1;
    req1.SerializeToString(&body1);
    std::string encoded1 = MyRPC::ProtocolUtil::Encode(meta1, body1);
    buffer.append(encoded1);

    // 2. 构造第二个请求，故意切断它，模拟半包放入 buffer
    tiny_rpc::OrderRequest req2;
    req2.set_order_id("Order-10086");
    req2.set_item_count(5);
    req2.set_price(15.9);

    tiny_rpc::RpcMeta meta2;
    meta2.set_method_name("QueryOrder");
    meta2.set_service_name("OrderService");
    meta2.set_sequence_id(1);
    meta2.set_type(0);
    meta2.set_timeout_ms(1000);
    meta2.set_trace_id("trace-002");

    std::string body2;
    req2.SerializeToString(&body2);
    std::string encoded2 = MyRPC::ProtocolUtil::Encode(meta2, body2);
    // 只截取一部分放入 Buffer，例如只放入前 10 个字节，模拟网络发送数据的断裂
    buffer.append(encoded2.data(), 10);

    // 3. 核心拆包循环（模拟 RpcCodec::parseMessage 的逻辑）
    while (buffer.readableBytes() > 0) {
        tiny_rpc::RpcMeta meta;
        std::string raw_body;
        size_t consumed_bytes = 0;
        
        MyRPC::DecodeStatus status = MyRPC::ProtocolUtil::Decode(
            buffer.peek(), buffer.readableBytes(),
            meta, raw_body, consumed_bytes);

        if (status == MyRPC::DecodeStatus::kSuccess) {
            std::cout << "✅ 成功解析出一个完整数据包！\n";
            std::cout << "  - Service: " << meta.service_name() << "\n";
            std::cout << "  - Method: " << meta.method_name() << "\n";
            std::cout << "  - Sequence ID: " << meta.sequence_id() << "\n";
            
            // 上层自行反序列化业务消息
            tiny_rpc::OrderRequest req;
            if (req.ParseFromString(raw_body)) {
                std::cout << "  - Order ID: " << req.order_id() << "\n";
                std::cout << "  - Item Count: " << req.item_count() << "\n";
                std::cout << "  - Price: " << req.price() << "\n";
            } else {
                std::cout << "  ❌ 业务消息反序列化失败\n";
            }

            // 由 consumed_bytes 精确推进 Buffer 游标
            buffer.retrieve(consumed_bytes); 
        } 
        else if (status == MyRPC::DecodeStatus::kHalfPacket) {
            std::cout << "⏳ 数据不够，是个半包，break 等待下次网络事件...\n";
            break;
        } 
        else if (status == MyRPC::DecodeStatus::kError) {
            std::cout << "❌ 遭遇脏数据，立刻断开连接！\n";
            break;
        }
    }

    return 0;
}