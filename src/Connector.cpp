#include "Connector.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

namespace MyRPC {

Connector::Connector(EventLoop* loop, const std::string& ip, int port)
    : loop_(loop),
      ip_(ip),
      port_(port),
      state_(State::kDisconnected),
      connect_(false),
      retryDelayMs_(kInitRetryDelayMs) {
}

Connector::~Connector() {
    stop();
}

void Connector::start() {
    connect_ = true;
    loop_->queueInLoop([this]() { startInLoop(); });
}

void Connector::stop() {
    connect_ = false;
    // 取消正在等待的重试定时器
    if (retryTimer_) {
        retryTimer_->setDeleted();
        retryTimer_.reset();
    }
}

void Connector::startInLoop() {
    if (!connect_) return;

    if (state_ == State::kDisconnected) {
        connect();
    }
}

void Connector::connect() {
    // 1. 创建非阻塞 TCP socket
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG_ERROR << "Connector: 创建 Socket 失败: " << strerror(errno);
        return;
    }

    // 2. 构造目标地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr);

    // 3. 发起非阻塞连接
    int ret = ::connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    int savedErrno = (ret == 0) ? 0 : errno;

    switch (savedErrno) {
        case 0:
        case EISCONN:
            // 连接立即成功（极少数情况，如 loopback）
            LOG_INFO << "Connector: 连接立即成功, fd=" << sockfd;
            state_ = State::kConnected;
            retryDelayMs_ = kInitRetryDelayMs; // 重置退避间隔
            if (newConnectionCallback_) {
                newConnectionCallback_(sockfd);
            }
            break;

        case EINPROGRESS:
            // 三次握手进行中，注册可写事件等待结果
            LOG_INFO << "Connector: 连接进行中 (EINPROGRESS), fd=" << sockfd;
            connecting(sockfd);
            break;

        case ECONNREFUSED:
        case ENETUNREACH:
        case EHOSTUNREACH:
        case EADDRNOTAVAIL:
        case ETIMEDOUT:
            // 可恢复错误：指数退避重试
            LOG_WARNING << "Connector: 连接失败 (可恢复), errno=" << savedErrno
                        << " (" << strerror(savedErrno) << ")";
            retry(sockfd);
            break;

        default:
            // 致命错误：关闭 fd，放弃连接
            LOG_ERROR << "Connector: 连接失败 (致命错误), errno=" << savedErrno
                      << " (" << strerror(savedErrno) << ")";
            ::close(sockfd);
            break;
    }
}

void Connector::connecting(int sockfd) {
    state_ = State::kConnecting;

    // 创建 Channel 监听可写事件
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleError(); });
    channel_->enableWriting();
}

void Connector::handleWrite() {
    if (state_ != State::kConnecting) return;

    // 1. 从 epoll 中移除 Channel 并取回 fd
    int sockfd = resetChannel();

    // 2. 使用 getsockopt 检查真实连接结果
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        optval = errno;
    }

    if (optval != 0) {
        // 连接失败
        LOG_WARNING << "Connector: getsockopt 检测连接失败, errno=" << optval
                    << " (" << strerror(optval) << ")";
        retry(sockfd);
        return;
    }

    // 3. 检查自连接（本地端口 == 远端端口 on loopback）
    struct sockaddr_in localAddr, peerAddr;
    socklen_t addrLen = sizeof(localAddr);
    ::getsockname(sockfd, (struct sockaddr*)&localAddr, &addrLen);
    ::getpeername(sockfd, (struct sockaddr*)&peerAddr, &addrLen);
    if (localAddr.sin_port == peerAddr.sin_port &&
        localAddr.sin_addr.s_addr == peerAddr.sin_addr.s_addr) {
        LOG_WARNING << "Connector: 检测到自连接, 重试...";
        retry(sockfd);
        return;
    }

    // 4. 连接成功！
    state_ = State::kConnected;
    retryDelayMs_ = kInitRetryDelayMs; // 重置退避间隔
    LOG_INFO << "Connector: 连接成功, fd=" << sockfd;

    if (connect_ && newConnectionCallback_) {
        newConnectionCallback_(sockfd);
    } else {
        ::close(sockfd);
    }
}

void Connector::handleError() {
    if (state_ != State::kConnecting) return;

    int sockfd = resetChannel();

    int optval = 0;
    socklen_t optlen = sizeof(optval);
    ::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
    LOG_ERROR << "Connector: Channel 错误事件, errno=" << optval
              << " (" << strerror(optval) << ")";

    retry(sockfd);
}

void Connector::retry(int sockfd) {
    ::close(sockfd);
    state_ = State::kDisconnected;

    if (!connect_) {
        LOG_INFO << "Connector: 用户已停止连接，不再重试";
        return;
    }

    LOG_INFO << "Connector: 将在 " << retryDelayMs_ << "ms 后重试连接 "
             << ip_ << ":" << port_;

    retryTimer_ = loop_->runAfter(retryDelayMs_, [this]() {
        startInLoop();
    });

    // 指数退避：翻倍，上限封顶
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
}

int Connector::resetChannel() {
    int sockfd = channel_->getFd();
    channel_->disableAll();
    loop_->removeChannel(channel_.get());
    // 必须延迟释放 Channel（当前正处于其回调中，不能立即 delete）
    // 使用 shared_ptr 转换，因为 queueInLoop 接受 std::function（要求可拷贝）
    std::shared_ptr<Channel> ch(channel_.release());
    loop_->queueInLoop([ch]() {
        // ch 在此 lambda 执行完毕后自动释放
    });
    return sockfd;
}

} // namespace MyRPC
