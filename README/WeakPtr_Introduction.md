# WeakPtr 自定义实现 README
## 概述
本实现是与自定义 `SharedPtr` 配套的**弱引用智能指针**，核心作用是“观测” `SharedPtr` 管理的资源，而非拥有资源所有权。`WeakPtr` 不增加 `SharedPtr` 的强引用计数，仅维护弱引用计数，可有效解决 `SharedPtr` 循环引用导致的内存泄漏问题。

本实现完全适配自定义 `SharedPtr` 的控制块（`ControlBlock`）设计，支持过期检测、原子化生成有效 `SharedPtr`、拷贝/移动语义等核心能力，接口与 C++ 标准库 `std::weak_ptr` 对齐，是共享式智能指针体系的重要补充。

## 核心设计
### 1. 核心依赖
`WeakPtr` 依赖 `SharedPtr` 的**共享控制块（ControlBlock）** 实现功能，所有关联同一资源的 `SharedPtr` 和 `WeakPtr` 共享同一个控制块：
| 控制块成员 | 对 WeakPtr 的意义 |
|------------|------------------|
| `strong_count` | 强引用计数：判断资源是否存活（`strong_count == 0` 则资源已释放） |
| `weak_count` | 弱引用计数：记录关联的 `WeakPtr` 数量，控制控制块的销毁时机 |
| `ptr` | 托管资源的裸指针：用于生成 `SharedPtr` 时关联资源 |

### 2. 核心规则
- `WeakPtr` 仅增加控制块的**弱引用计数**，不影响强引用计数，因此不会阻止资源释放；
- 调用 `expired()` 可检测资源是否存活（强引用计数为 0 则“过期”）；
- 调用 `lock()` 可原子化生成有效的 `SharedPtr`（资源未过期时强引用计数 +1，过期则返回空 `SharedPtr`）；
- 控制块的销毁条件：**强引用计数 + 弱引用计数均为 0**。

## 核心特性
| 特性 | 说明 |
|------|------|
| 弱引用特性 | 不拥有资源所有权，不增加强引用计数，不阻止资源释放 |
| 过期检测 | `expired()` 方法原子化检查资源是否存活，线程安全 |
| 安全获取资源 | `lock()` 方法原子化生成有效 `SharedPtr`，避免“悬空指针”问题 |
| 拷贝/移动语义 | 支持拷贝构造/赋值（弱引用计数 +1）、移动构造/赋值（转移所有权，不修改计数） |
| 线程安全 | 基于 `std::atomic` 的计数操作，计数增减、`expired()`/`lock()` 线程安全 |
| 空指针安全 | 默认构造/空 `SharedPtr` 关联的 `WeakPtr` 操作无异常 |
| 循环引用解决 | 配合 `SharedPtr` 使用，彻底解决循环引用导致的内存泄漏 |

## 接口说明
### 1. 模板参数
```cpp
template <typename T, typename Deleter = default_delete<T>>
class WeakPtr;
```
- `T`：观测的资源类型，需与关联的 `SharedPtr` 模板参数一致；
- `Deleter`：删除器类型，需与关联的 `SharedPtr` 模板参数一致（默认 `default_delete<T>`）。

### 2. 构造与析构
| 接口 | 说明 |
|------|------|
| `WeakPtr() noexcept` | 默认构造：创建空 `WeakPtr`，无关联控制块，`expired()` 返回 `true` |
| `WeakPtr(const SharedPtr<T, Deleter>& shared_ptr) noexcept` | 从 `SharedPtr` 构造：关联 `shared_ptr` 的控制块，弱引用计数 +1 |
| `WeakPtr(const WeakPtr& other) noexcept` | 拷贝构造：共享 `other` 的控制块，弱引用计数 +1 |
| `WeakPtr(WeakPtr&& other) noexcept` | 移动构造：转移 `other` 的控制块所有权，`other` 后置为空（不修改计数） |
| `~WeakPtr() noexcept` | 析构函数：调用 `reset()` 减少弱引用计数，必要时销毁控制块 |

### 3. 赋值运算符
| 接口 | 说明 |
|------|------|
| `WeakPtr& operator=(const WeakPtr& other) noexcept` | 拷贝赋值：先释放当前控制块（`reset()`），再共享 `other` 的控制块，弱引用计数 +1 |
| `WeakPtr& operator=(WeakPtr&& other) noexcept` | 移动赋值：先释放当前控制块（`reset()`），再转移 `other` 的所有权，`other` 后置为空 |
| `WeakPtr& operator=(const SharedPtr<T, Deleter>& shared_ptr) noexcept` | 从 `SharedPtr` 赋值：先释放当前控制块（`reset()`），再关联 `shared_ptr` 的控制块，弱引用计数 +1 |

### 4. 核心方法
| 接口 | 说明 |
|------|------|
| `bool expired() const noexcept` | 检测资源是否过期：<br>- `true`：控制块为空 或 强引用计数为 0（资源已释放）；<br>- `false`：资源存活 |
| `SharedPtr<T, Deleter> lock() const noexcept` | 生成有效 `SharedPtr`：<br>- 资源未过期：强引用计数 +1，返回关联资源的 `SharedPtr`；<br>- 资源已过期：返回空 `SharedPtr` |
| `size_t use_count() const noexcept` | 获取当前强引用计数：<br>- 资源过期：返回 0；<br>- 资源存活：原子加载 `strong_count`（仅参考，不保证实时性） |
| `void reset() noexcept` | 重置弱引用：<br>1. 减少弱引用计数；<br>2. 若弱引用计数为 0 且强引用计数为 0，销毁控制块；<br>3. 置空指针和控制块 |
| `void swap(WeakPtr& other) noexcept` | 交换两个 `WeakPtr` 的指针和控制块（无计数修改） |

## 使用示例
### 示例 1：基础使用（观测 SharedPtr 资源）
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
// 包含 default_delete/ControlBlock/SharedPtr/WeakPtr 完整实现

int main() {
    // 1. 创建 SharedPtr 并关联 WeakPtr
    SharedPtr<int> sp(new int(123));
    WeakPtr<int> wp(sp);

    std::cout << "1. 初始状态：" << std::endl;
    std::cout << "   WeakPtr 是否过期: " << std::boolalpha << wp.expired() << std::endl; // false
    std::cout << "   观测的强引用计数: " << wp.use_count() << std::endl; // 1

    // 2. lock() 生成有效 SharedPtr
    SharedPtr<int> sp2 = wp.lock();
    std::cout << "\n2. lock() 后：" << std::endl;
    std::cout << "   新 SharedPtr 取值: " << *sp2 << std::endl; // 123
    std::cout << "   强引用计数: " << sp.use_count() << std::endl; // 2

    // 3. 释放 lock 生成的 SharedPtr
    sp2.reset();
    std::cout << "\n3. 释放 sp2 后：" << std::endl;
    std::cout << "   强引用计数: " << sp.use_count() << std::endl; // 1

    return 0;
}
```
**输出**：
```
1. 初始状态：
   WeakPtr 是否过期: false
   观测的强引用计数: 1

2. lock() 后：
   新 SharedPtr 取值: 123
   强引用计数: 2

3. 释放 sp2 后：
   强引用计数: 1
```

### 示例 2：资源过期检测
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
// 包含 default_delete/ControlBlock/SharedPtr/WeakPtr 完整实现

int main() {
    WeakPtr<std::string> wp;

    {
        // 局部作用域创建 SharedPtr
        SharedPtr<std::string> sp(new std::string("hello weakptr"));
        wp = sp;
        std::cout << "1. 局部作用域内：" << std::endl;
        std::cout << "   WeakPtr 是否过期: " << wp.expired() << std::endl; // false
    } // SharedPtr 析构，资源释放

    std::cout << "\n2. 局部作用域外：" << std::endl;
    std::cout << "   WeakPtr 是否过期: " << wp.expired() << std::endl; // true

    // 过期后 lock() 返回空 SharedPtr
    SharedPtr<std::string> sp2 = wp.lock();
    if (!sp2) {
        std::cout << "\n3. lock() 结果：返回空 SharedPtr" << std::endl;
    }

    return 0;
}
```
**输出**：
```
1. 局部作用域内：
   WeakPtr 是否过期: false

2. 局部作用域外：
   WeakPtr 是否过期: true

3. lock() 结果：返回空 SharedPtr
```

### 示例 3：解决 SharedPtr 循环引用（核心场景）
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
// 包含 default_delete/ControlBlock/SharedPtr/WeakPtr 完整实现

// 循环引用的节点类
struct Node {
    std::string name;
    WeakPtr<Node> peer; // 用 WeakPtr 替代 SharedPtr，避免循环引用

    Node(const std::string& n) : name(n) {
        std::cout << "构造 Node: " << name << std::endl;
    }
    ~Node() {
        std::cout << "析构 Node: " << name << std::endl;
    }
};

int main() {
    {
        SharedPtr<Node> nodeA(new Node("A"));
        SharedPtr<Node> nodeB(new Node("B"));

        // 互相引用：WeakPtr 不增加强引用计数
        nodeA->peer = nodeB;
        nodeB->peer = nodeA;

        // 通过 lock() 访问关联节点
        SharedPtr<Node> peerOfA = nodeA->peer.lock();
        std::cout << "nodeA 的关联节点: " << peerOfA->name << std::endl; // B
    } // 作用域结束，节点正常析构，无内存泄漏

    return 0;
}
```
**输出**（无内存泄漏，析构函数执行）：
```
构造 Node: A
构造 Node: B
nodeA 的关联节点: B
析构 Node: B
析构 Node: A
```

### 示例 4：移动/拷贝赋值与 swap
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
// 包含 default_delete/ControlBlock/SharedPtr/WeakPtr 完整实现

int main() {
    SharedPtr<int> sp1(new int(100));
    SharedPtr<int> sp2(new int(200));

    // 1. 拷贝赋值
    WeakPtr<int> wp1(sp1);
    WeakPtr<int> wp2;
    wp2 = wp1;
    std::cout << "1. 拷贝赋值后 wp2 lock() 取值: " << *(wp2.lock()) << std::endl; // 100

    // 2. 移动赋值
    WeakPtr<int> wp3 = std::move(wp2);
    std::cout << "2. 移动赋值后 wp2 是否过期: " << std::boolalpha << wp2.expired() << std::endl; // true
    std::cout << "   wp3 lock() 取值: " << *(wp3.lock()) << std::endl; // 100

    // 3. swap 交换
    WeakPtr<int> wp4(sp1);
    WeakPtr<int> wp5(sp2);
    wp4.swap(wp5);
    std::cout << "\n3. swap 后 wp4 lock() 取值: " << *(wp4.lock()) << std::endl; // 200

    return 0;
}
```
**输出**：
```
1. 拷贝赋值后 wp2 lock() 取值: 100
2. 移动赋值后 wp2 是否过期: true
   wp3 lock() 取值: 100

3. swap 后 wp4 lock() 取值: 200
```

## 注意事项
### 1. 不可直接访问资源
`WeakPtr` 没有重载 `*`/`->` 运算符，**不能直接解引用或访问资源成员**，必须通过 `lock()` 生成 `SharedPtr` 后才能访问资源，避免访问已释放的“悬空指针”。

### 2. 线程安全说明
- `expired()`/`lock()`/计数增减操作基于 `std::atomic`，是线程安全的；
- 对 `WeakPtr` 对象本身的修改（如 `reset()`/赋值/swap）**非线程安全**，需避免多线程同时修改同一 `WeakPtr`；
- `lock()` 返回的 `SharedPtr` 线程安全特性与自定义 `SharedPtr` 一致（计数安全，资源读写需自行加锁）。

### 3. 循环引用解决要点
- 当两个对象通过 `SharedPtr` 互相引用时，强引用计数无法归 0，资源永久泄漏；
- 解决方案：将其中一个方向的引用改为 `WeakPtr`，仅观测对方资源，不增加强引用计数。

### 4. 控制块销毁时机
- 控制块的销毁需满足：`strong_count == 0` **且** `weak_count == 0`；
- 即使 `strong_count == 0`（资源已释放），只要 `weak_count > 0`，控制块仍会保留，直到所有 `WeakPtr` 析构/重置。

### 5. 空指针安全
- 默认构造的空 `WeakPtr`、关联空 `SharedPtr` 的 `WeakPtr` 调用 `expired()`/`lock()`/`reset()` 均无异常；
- 空 `WeakPtr` 的 `expired()` 返回 `true`，`lock()` 返回空 `SharedPtr`。

## 与 std::weak_ptr 的对比
| 特性 | 自定义 WeakPtr | std::weak_ptr |
|------|----------------|---------------|
| 核心功能 | 弱引用观测、过期检测、lock 生成 SharedPtr | 完全一致 |
| 线程安全 | 计数操作原子化，线程安全 | 完全一致 |
| 接口兼容 | 核心接口（expired/lock/use_count/reset/swap）与标准库对齐 | 原生支持更多扩展接口（如 owner_before） |
| 依赖关系 | 强依赖自定义 SharedPtr 的控制块设计 | 与 std::shared_ptr 深度集成 |
| 异常安全 | 基础异常安全（无抛异常接口） | 强异常安全保证 |

## 总结
本 `WeakPtr` 实现是自定义 `SharedPtr` 的完美配套组件，核心价值在于：
1. **解决循环引用**：通过弱引用特性打破 `SharedPtr` 循环引用，避免内存泄漏；
2. **安全观测资源**：`expired()`/`lock()` 原子化操作，避免访问已释放的资源；
3. **不影响资源生命周期**：弱引用不增加强计数，不阻止资源的正常释放；
4. **接口易用性**：与标准库 `std::weak_ptr` 接口对齐，学习和使用成本低。

适合与自定义 `SharedPtr` 配合使用，用于学习智能指针的弱引用原理，或在轻量级项目中替代标准库 `std::weak_ptr`（需确保与自定义 `SharedPtr` 配套）。
