/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-03-20 15:29:51
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-03-31 14:03:32
 * @FilePath: /ServerPractice/src/TcpConnection.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "TcpConnection.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"
#include "Logger.hpp"
#include "Buffer.hpp"
#include <sys/socket.h>
#include <unistd.h>

namespace MyRPC {
    TcpConnection::TcpConnection(EventLoop* loop, int fd)
        : loop_(loop), fd_(fd), connId_(-1), state_(StateE::kConnected) {
        channel_ = new Channel(loop_, fd_);
        // 绑定读事件的回调函数
        channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this));
        channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
        // 注意：不再在构造函数中 enableReading，必须等到 IO 线程中通过 connectEstablished() 来完成注册
    }

    TcpConnection::~TcpConnection() {
        // 析构时必须关闭底层文件描述符，防止幽灵连接
        channel_->disableAll();
        delete channel_;
        ::close(fd_); // 必须关闭底层文件描述符，防止幽灵连接
    }

    void TcpConnection::connectEstablished() {
        channel_->enableReading(); // 在 IO 线程中安全地注册 epoll 读事件
    }

    void TcpConnection::handleRead() {
        // 如果连接已经不在正常状态，直接忽略，防止重入
        if (state_ != StateE::kConnected) return;
        // 使用智能指针守卫，防止在回调过程中自己被析构导致崩溃
        auto guard = shared_from_this();
        int active_fd = channel_->getFd();
        while (true) {
            char buf[1024];
            /**
             * @brief 从套接字接收数据 (系统调用)
             * @param sockfd  用于接收数据的套接字文件描述符 (此处的 active_fd)
             * @param buf     指向存放接收数据缓冲区的指针
             * @param len     缓冲区的最大长度
             * @param flags   接收操作的标志位，通常设为 0
             * @return 成功返回实际接收到的字节数；返回 0 表示对端已正常关闭连接；失败返回 -1 并设置 errno
             */
            int bytes_read = recv(active_fd, buf, sizeof(buf), 0);
            if (bytes_read > 0) {
                buffer_.append(buf, bytes_read); // 把读到的数据追加到缓冲区
                // 每次读到数据都要续命一下，重置秒表
                extendLife();
            } else if (bytes_read == -1){
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有更多数据可读了，退出循环
                    break;
                } else {
                    LOG_ERROR << "Recv 失败!";
                    state_ = StateE::kDisconnecting;
                    channel_->disableAll(); // 立即从 epoll 中注销，防止后续事件重入
                    // 将回调拷贝到栈上，防止对象自杀导致段错误
                    auto cb = closeCallback_;
                    if (cb) {
                        cb(guard);
                    }
                    return; // close 后直接返回，不再执行 messageCallback_
                }
            } else {
                // bytes_read == 0，客户端断开连接
                LOG_INFO << "客户端 fd " << active_fd << " 断开连接";
                state_ = StateE::kDisconnecting;
                channel_->disableAll(); // 立即从 epoll 中注销，防止后续事件重入
                // 同理，拷贝到栈上安全执行
                auto cb = closeCallback_;
                if (cb) {
                    cb(guard);
                }
                return; // close 后直接返回，不再执行 messageCallback_
            }
        }

        if (state_ == StateE::kConnected) {
            if (messageCallback_) {
                messageCallback_(guard, &buffer_);
            }
        }
    }

    void TcpConnection::send(const std::string& msg) {
        if (state_ != StateE::kConnected) return; // 连接已断开，不再发送

        // 1. 先将数据全量追加进应用层写缓冲区
        writeBuffer_.append(msg);

        // 2. 如果当前没有监听写事件（即上一次已全部发完），就尝试直接发送
        if (!channel_->isWriting()) {
            handleWrite(); // 尝试直接发送，如果内核缓冲区满了会自动注册 EPOLLOUT
        }
    }

    void TcpConnection::handleWrite() {
        if (state_ != StateE::kConnected) return; // 连接已断开，不再处理写事件
        auto guard = shared_from_this();

        while (writeBuffer_.readableBytes() > 0) {
            ssize_t bytes = ::send(fd_, writeBuffer_.peek(), writeBuffer_.readableBytes(), 0);
            if (bytes > 0) {
                writeBuffer_.retrieve(bytes); // 消费掉已发送的数据
            } else {
                if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    // 内核发送缓冲区已满，注册 EPOLLOUT，等待下次可写事件由 epoll 回调
                    channel_->enableWriting();
                    return;
                } else {
                    LOG_ERROR << "发送数据失败!";
                    return;
                }
            }
        }

        // 能跑到这里，说明 writeBuffer_ 里的数据已经全部发完，取消 EPOLLOUT 监听，防止 CPU 空转
        channel_->disableWriting();
    }



    void TcpConnection::extendLife() {
        // 1. 如果之前已经有一个秒表了，我们直接把它“标记删除”（惰性删除，O(1)复杂度）
        // 这样大管家在处理时会自动忽略它，极其高效！
        if (keepAliveTimer_) {
            keepAliveTimer_->setDeleted();
        }

        // 2. 重新开启一个 30 秒的定时器！
        std::weak_ptr<TcpConnection> weak_conn = shared_from_this();

        keepAliveTimer_ = loop_->runAfter(30000, [weak_conn]() {
            // 闹钟响了，尝试把 weak_ptr 提升为 shared_ptr
            auto conn = weak_conn.lock();
            if (conn) {
                // 如果提升成功，说明连接还没被常规途径关闭，立刻执行踢人逻辑！
                conn->handleTimeout();
            }
        });
    }

    void TcpConnection::handleTimeout() {
        LOG_WARNING << "客户端 fd " << fd_ << " 长时间未发送数据，心跳超时，强制踢出！";
        if (state_ != StateE::kConnected) return; // 已经在关闭了，不重复处理
        state_ = StateE::kDisconnecting;
        channel_->disableAll(); // 立即从 epoll 中注销，防止后续事件重入
        // 触发关闭回调，TcpServer 会负责把它从账本里删掉，并销毁堆内存
        auto guard = shared_from_this();
        if (closeCallback_) {
            closeCallback_(guard);
        }
    }

    void TcpConnection::forceClose() {
        if (state_ == StateE::kConnected) {
            auto guard = shared_from_this();
            // 1. 立即修改状态机，防止新的读写事件被调度
            state_ = StateE::kDisconnecting;
            // 2. 取消所有定时器
            if (keepAliveTimer_) {
                keepAliveTimer_->setDeleted();
            }
            // 将任务丢回到 I/O 线程中处理
            loop_->queueInLoop([this, guard]() {
                channel_->disableAll(); // 在 IO 线程中注销 epoll，防止后续事件重入
                if (closeCallback_) {
                    closeCallback_(guard);
                }
            });
        }
    }
}