/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 19:39:38
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-17 19:39:52
 * @FilePath: /TinyPRC/include/Status.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once


namespace MyRPC {
    // 定义解码状态机
    enum class DecodeStatus {
        kSuccess,     // 成功解析出一个完整的包
        kHalfPacket,  // 数据不够，是个半包，需要等待更多网络数据
        kError        // 魔数不对或协议损坏，遭遇脏数据
    };
} // namespace MyRPC