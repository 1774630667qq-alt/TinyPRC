/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 17:19:46
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-17 19:46:33
 * @FilePath: /TinyPRC/src/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%A
 */
#include <iostream>
#include "order.pb.h"
#include "Buffer.hpp"
#include "ProtocolUtil.hpp"

int main() {
    MyRPC::Buffer buffer;
    
    // 1. 构造一个正常的请求并 Encode 放入 buffer
    tiny_rpc::OrderRequest req1;
    req1.set_order_id("Order-9527");
    req1.set_item_count(3);
    req1.set_price(299.9);

    RpcHeader header1;
    header1.sequence_id = 1;

    std::string encoded1 = MyRPC::ProtocolUtil::Encode(header1, req1);
    buffer.append(encoded1);

    // 2. 构造第二个请求，故意切断它，模拟半包放入 buffer
    tiny_rpc::OrderRequest req2;
    req2.set_order_id("Order-10086");
    req2.set_item_count(5);
    req2.set_price(15.9);

    RpcHeader header2;
    header2.sequence_id = 2;

    std::string encoded2 = MyRPC::ProtocolUtil::Encode(header2, req2);
    // 只截取一部分放入 Buffer，例如只放入前 10 个字节，模拟网络发送数据的断裂
    buffer.append(encoded2.c_str(), 10);

    // 3. 核心拆包循环（这就是你未来写在 TcpConnection::onMessage 里的代码）
    while (buffer.readableBytes() > 0) {
        RpcHeader header;
        tiny_rpc::OrderRequest req;
        
        MyRPC::DecodeStatus status = MyRPC::ProtocolUtil::Decode(&buffer, header, req);

        if (status == MyRPC::DecodeStatus::kSuccess) {
            std::cout << "✅ 成功解析出一个完整数据包！\n";
            std::cout << "  - Order ID: " << req.order_id() << "\n";
            std::cout << "  - Sequence ID: " << header.sequence_id << "\n";
            
            // 【关键步骤】：由调用者计算出整个包的长度，并控制 Buffer 丢弃这段数据！
            buffer.retrieve(sizeof(RpcHeader) + header.data_size); 
        } 
        else if (status == MyRPC::DecodeStatus::kHalfPacket) {
            std::cout << "数据不够，是个半包，break 等待下次网络事件...\n";
            break;
        } 
        else if (status == MyRPC::DecodeStatus::kError) {
            std::cout << "遭遇脏数据，立刻断开连接！\n";
            break;
        }
    }

    return 0;
}