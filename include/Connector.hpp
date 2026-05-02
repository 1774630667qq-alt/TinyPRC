#pragma once
#include <functional>
#include <memory>
#include <string>
#include "Timer.hpp"

namespace MyRPC {

class EventLoop;
class Channel;

/**
 * @brief 非阻塞连接器：负责主动发起 TCP 连接（与 Acceptor 对称）
 *
 * 核心状态机：
 *   kDisconnected → connect() → kConnecting → handleWrite() 检测结果
 *     → 成功：kConnected，执行 newConnectionCallback_ 移交 sockfd
 *     → 失败：retry() 指数退避后重回 kDisconnected 再次 connect()
 *
 * 设计要点：
 * - 所有操作必须在所属 EventLoop 线程中执行（通过 queueInLoop 保证）
 * - sockfd 移交后，Connector 不再持有该 fd，也不负责 close
 * - 指数退避重试：500ms → 1s → 2s → ... → 30s（封顶）
 */
class Connector {
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    /**
     * @brief 构造函数
     * @param loop 所属的 EventLoop（所有操作在该线程中执行）
     * @param ip   目标服务器 IP 地址
     * @param port 目标服务器端口号
     */
    Connector(EventLoop* loop, const std::string& ip, int port);
    ~Connector();

    /**
     * @brief 设置连接成功后的回调（移交 sockfd 给上层）
     * @param cb 回调函数，参数为已连接成功的非阻塞 sockfd
     */
    void setNewConnectionCallback(NewConnectionCallback cb) {
        newConnectionCallback_ = std::move(cb);
    }

    /**
     * @brief 发起连接（线程安全，内部通过 queueInLoop 投递）
     */
    void start();

    /**
     * @brief 停止连接（取消重试定时器）
     */
    void stop();

private:
    /**
     * @brief 连接器内部状态机
     */
    enum class State { kDisconnected, kConnecting, kConnected };

    /**
     * @brief 在 EventLoop 线程中执行连接启动逻辑
     */
    void startInLoop();

    /**
     * @brief 创建非阻塞 socket 并调用 ::connect()
     * @details 根据 connect() 返回值决定下一步：
     *  - 0 / EISCONN：直接成功
     *  - EINPROGRESS：进入 connecting() 监听可写事件
     *  - 可恢复错误（ECONNREFUSED 等）：retry() 指数退避
     *  - 致命错误：关闭 fd 并打印日志
     */
    void connect();

    /**
     * @brief EINPROGRESS 后的处理：注册 Channel 监听可写事件
     * @param sockfd 正在连接中的非阻塞套接字
     */
    void connecting(int sockfd);

    /**
     * @brief 可写事件回调：使用 getsockopt(SO_ERROR) 检测连接结果
     */
    void handleWrite();

    /**
     * @brief 错误事件回调
     */
    void handleError();

    /**
     * @brief 关闭失败的 sockfd，启动指数退避定时器后重连
     * @param sockfd 需要关闭的套接字
     */
    void retry(int sockfd);

    /**
     * @brief 清理当前 Channel（从 epoll 中删除并释放）
     * @return 被 Channel 封装的 fd（已从 epoll 中移除但尚未 close）
     */
    int resetChannel();

    EventLoop* loop_;              ///< 所属的事件循环
    std::string ip_;               ///< 目标 IP
    int port_;                     ///< 目标端口
    State state_;                  ///< 当前连接状态
    bool connect_;                 ///< 用户是否希望保持连接

    std::unique_ptr<Channel> channel_;  ///< 监听连接中 sockfd 的 Channel
    NewConnectionCallback newConnectionCallback_;
    std::shared_ptr<Timer> retryTimer_; ///< 指数退避重试定时器

    int retryDelayMs_;                                  ///< 当前重试间隔（毫秒）
    static constexpr int kMaxRetryDelayMs = 30000;      ///< 最大重试间隔：30 秒
    static constexpr int kInitRetryDelayMs = 500;       ///< 初始重试间隔：500 毫秒
};

} // namespace MyRPC
