cmake_minimum_required(VERSION 3.10)
project(TinyRPC VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. 寻找系统中的 Protobuf 及 Threads 依赖
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# 2. 设置头文件包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
# 非常重要：protoc 生成的 .pb.h 通常在 build 目录下，所以需要包含当前二进制目录
include_directories(${CMAKE_CURRENT_BINARY_DIR}) 

file(GLOB SRC_LIST "src/*.cpp")
list(REMOVE_ITEM SRC_LIST "src/main.cpp")

# =========================================================
# 下面是为你接下来的微任务 1.1 预留的编译规则
# =========================================================

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/order.proto)

# 3. 这是一个 CMake 魔法：它会自动调用 protoc 编译 .proto 文件
# 并且把生成的 .pb.cc 和 .pb.h 文件路径存入 PROTO_SRCS 和 PROTO_HDRS 变量中
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

# 4. 编译我们的测试可执行文件 (把生成的 .pb.cc 和 main.cpp 一起编译)
add_executable(proto_test src/main.cpp ${SRC_LIST} ${PROTO_SRCS})

# 5. 链接 Protobuf 的 C++ 运行时库及多线程库
target_link_libraries(proto_test PRIVATE ${Protobuf_LIBRARIES} Threads::Threads)