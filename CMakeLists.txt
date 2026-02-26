# 指定CMake最低版本（按需调整，如3.5/3.15）
cmake_minimum_required(VERSION 3.10)

# 项目名称 + 支持的语言（CXX=C++，C=C，可同时写C CXX）
project(YourProjectName LANGUAGES CXX)

# 配置编译标准（C++17为例，C项目改CMAKE_C_STANDARD）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 强制使用指定标准
set(CMAKE_CXX_EXTENSIONS OFF)        # 禁用编译器扩展（保证跨平台）

# 编译警告配置（可选，按需增减）
if(MSVC)  # Windows MSVC编译器
    add_compile_options(/W4 /WX)  # W4=最高警告级别，WX=警告视为错误
else()    # GCC/Clang（Linux/MacOS）
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# 1. 手动指定源码文件（适合文件少的场景）
set(SOURCES
    src/main.cpp
    src/utils.cpp
    # 补充你的其他.cpp文件...
)

# 2. 递归查找源码（适合文件多的场景，二选一）
# file(GLOB_RECURSE SOURCES "src/*.cpp")

# 头文件目录（如果头文件在include/下）
include_directories(include)

# 生成可执行文件（库项目见下方注释）
add_executable(${PROJECT_NAME} ${SOURCES})

# 【可选】链接第三方库/系统库（示例）
# target_link_libraries(${PROJECT_NAME} PRIVATE pthread)  # Linux线程库
# target_link_libraries(${PROJECT_NAME} PRIVATE fmt)      # 第三方fmt库

# 若为库项目，替换add_executable：
# add_library(${PROJECT_NAME} STATIC ${SOURCES})  # 静态库
# add_library(${PROJECT_NAME} SHARED ${SOURCES})  # 动态库
