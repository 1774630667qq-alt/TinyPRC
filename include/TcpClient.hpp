#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include "Buffer.hpp"

namespace MyRPC {

class EventLoop;
class TcpConnection;
class Connector;

/**
 * @brief TCP 客户端门面类（与 TcpServer 对称）
 *
 * 持有一个 Connector 和一个 TcpConnection：
 * - Connector 负责非阻塞地发起连接（含指数退避重连）
 * - TcpConnection 负责已建立连接上的读写和生命周期管理
 *
 * 用户只需设置 ConnectionCallback 和 MessageCallback，
 * 然后调用 connect() 即可使用。断连后会自动重连（可通过 disconnect() 禁用）。
 */
class TcpClient {
public:
    using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
    using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&, Buffer*)>;

    /**
     * @brief 构造函数
     * @param loop 事件循环（TcpClient 的所有操作在此线程中执行）
     * @param ip   目标服务器 IP 地址
     * @param port 目标服务器端口号
     */
    TcpClient(EventLoop* loop, const std::string& ip, int port);
    ~TcpClient();

    /**
     * @brief 发起连接（线程安全）
     */
    void connect();

    /**
     * @brief 主动断开连接（并停止自动重连）
     */
    void disconnect();

    /**
     * @brief 发送数据到服务器（线程安全）
     * @param msg 要发送的二进制数据
     */
    void send(const std::string& msg);

    /**
     * @brief 设置连接建立/断开时的回调
     * @param cb 回调函数，参数为当前 TcpConnection 的智能指针
     */
    void setConnectionCallback(ConnectionCallback cb) {
        connectionCallback_ = std::move(cb);
    }

    /**
     * @brief 设置收到数据时的回调
     * @param cb 回调函数
     */
    void setMessageCallback(MessageCallback cb) {
        messageCallback_ = std::move(cb);
    }

    /**
     * @brief 获取当前活跃的 TcpConnection（可能为 nullptr）
     * @return TcpConnection 的智能指针
     */
    std::shared_ptr<TcpConnection> connection() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_;
    }

    /**
     * @brief 获取所属的 EventLoop
     */
    EventLoop* getLoop() const { return loop_; }

private:
    /**
     * @brief Connector 成功建立连接后的回调
     * @param sockfd 已连接的非阻塞套接字
     */
    void newConnection(int sockfd);

    /**
     * @brief 连接断开时的回调（由 TcpConnection 触发）
     * @param conn 已断开的 TcpConnection
     */
    void removeConnection(const std::shared_ptr<TcpConnection>& conn);

    EventLoop* loop_;                                ///< 所属事件循环
    Connector* connector_;                           ///< 非阻塞连接器
    std::shared_ptr<TcpConnection> connection_;      ///< 当前活跃连接
    mutable std::mutex mutex_;                       ///< 保护 connection_

    ConnectionCallback connectionCallback_;          ///< 连接状态变更回调
    MessageCallback messageCallback_;                ///< 数据到达回调

    int nextConnId_;                                 ///< 连接 ID 分配器
    bool connect_;                                   ///< 是否保持连接（断开后自动重连）
};

} // namespace MyRPC
