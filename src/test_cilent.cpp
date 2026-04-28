#include <cstddef>
#include <cerrno>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "ProtocolUtil.hpp"
#include "Logger.hpp"
#include "Buffer.hpp"
#include "order.pb.h"
#include "rpc_meta.pb.h"

namespace {
bool sendAll(int fd, const std::string& data) {
    const char* p = data.data();
    size_t left = data.size();

    while (left > 0) {
        ssize_t n = send(fd, p, left, 0);
        if (n > 0) {
            p += n;
            left -= static_cast<size_t>(n);
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            LOG_ERROR << "发送 RPC 请求失败: " << strerror(errno);
            return false;
        }
    }

    return true;
}

MyRPC::DecodeStatus recvOneRpcFrame(int fd,
                                    tiny_rpc::RpcMeta& resp_meta,
                                    std::string& raw_body) {
    MyRPC::Buffer recv_buf;
    char buf[4096];

    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            recv_buf.append(buf, static_cast<size_t>(n));

            size_t consumed = 0;
            MyRPC::DecodeStatus status = MyRPC::ProtocolUtil::Decode(
                recv_buf.peek(), recv_buf.readableBytes(),
                resp_meta, raw_body, consumed);

            if (status == MyRPC::DecodeStatus::kSuccess) {
                recv_buf.retrieve(consumed);
                return status;
            }

            if (status == MyRPC::DecodeStatus::kError) {
                return status;
            }

            // 半包：继续阻塞读取，直到收到完整 RPC 帧或连接断开。
            continue;
        }

        if (n == 0) {
            LOG_ERROR << "接收响应失败: 服务器关闭连接";
            return MyRPC::DecodeStatus::kError;
        }

        if (errno == EINTR) {
            continue;
        }

        LOG_ERROR << "接收响应失败: " << strerror(errno);
        return MyRPC::DecodeStatus::kError;
    }
}
} // namespace

int main () {
    // 准备要发送的数据
    tiny_rpc::RpcMeta meta;
    meta.set_service_name("tiny_rpc.OrderService"); // 告诉服务器调哪个服务
    meta.set_method_name("QueryOrder");             // 调哪个方法
    meta.set_sequence_id(1001);                     // 随便给个序列号
    meta.set_type(0);                               // 0 代表 Request

    tiny_rpc::OrderRequest req;
    req.set_order_id("ORD-998244353");              // 填充业务数据

    // 构建二进制流
    std::string raw_body;
    req.SerializeToString(&raw_body);  // 将 req 序列化到 raw_body
    std::string body = MyRPC::ProtocolUtil::Encode(meta, raw_body);
    
    int clientFD = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr.s_addr);

    if (connect(clientFD, (sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        LOG_ERROR << "连接失败";
        return -1;
    }

    LOG_INFO << "已连接到服务器";

    if (!sendAll(clientFD, body)) {
        close(clientFD);
        return -1;
    }

    LOG_INFO << "已发送 RPC 请求，序列号: 1001，订单号: ORD-998244353";

    tiny_rpc::RpcMeta resp_meta;
    tiny_rpc::OrderResponse resp;
    raw_body.clear();

    MyRPC::DecodeStatus status = recvOneRpcFrame(clientFD, resp_meta, raw_body);

    if (status != MyRPC::DecodeStatus::kSuccess) {
        LOG_ERROR << "❌ 解析响应包失败";
        close(clientFD);
        return -1;
    }

    if (!resp.ParseFromString(raw_body)) {
        LOG_ERROR << "❌ 解析响应业务体失败";
        close(clientFD);
        return -1;
    }

    LOG_INFO << "\n🎉🎉🎉 测试成功！服务器返回了正确的响应！";
    LOG_INFO << "📥 [Meta解析] SeqID: " << resp_meta.sequence_id();
    LOG_INFO << "📥 [业务解析] RetCode: " << resp.ret_code() << ", Info: " << resp.res_info() << ", OrderID: " << resp.order_id();

    close(clientFD);
    return 0;
}
