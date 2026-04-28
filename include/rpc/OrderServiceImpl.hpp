/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-20 14:07:55
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-28 16:23:27
 * @FilePath: /TinyRPC/include/rpc/OrderServiceImpl.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include "order.pb.h"
#include "google/protobuf/stubs/callback.h"
#include "Logger.hpp"
#include "unistd.h"

namespace MyRPC {
class OrderServiceImpl: public tiny_rpc::OrderService {
public:
    void QueryOrder(
        ::google::protobuf::RpcController* controller, 
        const tiny_rpc::OrderRequest* request, 
        tiny_rpc::OrderResponse* response, 
        ::google::protobuf::Closure* done) override {

        std::string order_id = request->order_id();
        LOG_INFO << "收到订单查询，订单号: " << order_id;

        // 模拟耗时操作
        sleep(1);

        // 设置响应
        response->set_ret_code(0);
        response->set_res_info("Success");
        response->set_order_id(order_id);

        // 调用回调
        if (done) {
            done->Run();
        }
    }
};
} // namespace MyRPC