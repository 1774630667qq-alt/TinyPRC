#include "TcpClient.hpp"
#include "Connector.hpp"
#include "EventLoop.hpp"
#include "TcpConnection.hpp"
#include "Logger.hpp"

namespace MyRPC {

TcpClient::TcpClient(EventLoop* loop, const std::string& ip, int port)
    : loop_(loop),
      connector_(new Connector(loop, ip, port)),
      nextConnId_(0),
      connect_(false) {
    // Connector 连接成功后，把 sockfd 交给 newConnection 处理
    connector_->setNewConnectionCallback(
        [this](int sockfd) { newConnection(sockfd); }
    );
}

TcpClient::~TcpClient() {
    disconnect();
    delete connector_;
}

void TcpClient::connect() {
    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect() {
    connect_ = false;
    connector_->stop();

    std::shared_ptr<TcpConnection> conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn = connection_;
    }
    if (conn) {
        conn->forceClose();
    }
}

void TcpClient::send(const std::string& msg) {
    std::shared_ptr<TcpConnection> conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn = connection_;
    }
    if (conn) {
        // 确保在 IO 线程中发送
        EventLoop* ioLoop = conn->getLoop();
        ioLoop->queueInLoop([conn, msg]() {
            conn->send(msg);
        });
    } else {
        LOG_WARNING << "TcpClient: 连接未建立，无法发送数据";
    }
}

void TcpClient::newConnection(int sockfd) {
    int connId = nextConnId_++;
    LOG_INFO << "TcpClient: 新连接建立, fd=" << sockfd << ", connId=" << connId;

    // 创建 TcpConnection（与 TcpServer::newConnection 对称）
    // 客户端只有一个 EventLoop，不需要线程池分发
    std::shared_ptr<TcpConnection> conn(
        new TcpConnection(loop_, sockfd),
        [this](TcpConnection* p) {
            loop_->queueInLoop([p]() {
                delete p;
            });
        }
    );
    conn->setConnId(connId);

    // 绑定用户设置的消息回调
    conn->setMessageCallback(messageCallback_);

    // 绑定关闭回调
    conn->setCloseCallback(
        [this](const std::shared_ptr<TcpConnection>& c) {
            removeConnection(c);
        }
    );

    {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_ = conn;
    }

    // 在 EventLoop 线程中完成连接初始化（注册 epoll 读事件）
    loop_->queueInLoop([conn]() {
        conn->connectEstablished();
    });

    // 通知用户：连接已建立
    if (connectionCallback_) {
        connectionCallback_(conn);
    }
}

void TcpClient::removeConnection(const std::shared_ptr<TcpConnection>& conn) {
    LOG_INFO << "TcpClient: 连接断开, connId=" << conn->getConnId();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_.reset();
    }

    // 如果用户希望保持连接，自动重连
    if (connect_) {
        LOG_INFO << "TcpClient: 自动重连中...";
        connector_->start();
    }
}

} // namespace MyRPC
