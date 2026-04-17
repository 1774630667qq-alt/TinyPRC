/*
 * @Author: Zhang YuHua 1774630667@qq.com
 * @Date: 2026-04-17 18:21:24
 * @LastEditors: Zhang YuHua 1774630667@qq.com
 * @LastEditTime: 2026-04-17 22:53:01
 * @FilePath: /TinyPRC/include/rpc/RpcHeader.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <cstdint>

struct RpcHeader {
    uint32_t magic_num = 998244353;     ///< 魔法数字，用于标识这是一个RPC请求
    uint32_t version = 1;               ///< 版本号
    uint32_t type = 0;                  ///< 0表示请求，1表示响应
    uint32_t sequence_id = 0;           ///< 序列号
    uint32_t data_size = 0;             ///< 消息体大小
};