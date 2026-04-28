/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-28 16:08:35
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-28 16:09:48
 * @FilePath: /TinyRPC/include/ModernClosure.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <google/protobuf/stubs/callback.h>
#include <functional>

namespace MyRPC {

// 一个支持现代 C++ Lambda 的完美闭包适配器
class ModernClosure : public ::google::protobuf::Closure {
public:
    // 构造时接收一个 std::function (Lambda 表达式会被自动转换存进来)
    explicit ModernClosure(std::function<void()> cb) : cb_(std::move(cb)) {}

    void Run() override {
        if (cb_) {
            cb_();
        }
        // Protobuf 的约定是，Closure 执行完后必须自我销毁
        delete this; 
    }

private:
    std::function<void()> cb_;
};

} // namespace MyRPC