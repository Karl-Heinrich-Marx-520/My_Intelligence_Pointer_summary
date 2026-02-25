# SharedPtr 实现中的双重释放问题分析与修复
## 一、背景介绍
本文分析一段自定义 `SharedPtr` 智能指针实现代码中隐藏的**双重释放（double free）** 问题。该代码包含核心的控制块（`ControlBlock`）定义、`reset` 方法实现，以及对应的裸指针构造测试用例。核心逻辑是通过强/弱引用计数管理动态资源的生命周期，但在资源释放环节存在细节性的逻辑错误，可能导致内存安全问题。

### 原始核心代码
```cpp
#include <atomic>
#include <cassert>
#include <iostream>

// 默认删除器
template <typename T>
struct default_delete {
    void operator()(T* ptr) const noexcept {
        delete ptr;
    }
};

//-------------------------控制块定义-----------------------------
// 统一的控制块，包含强引用计数和弱引用计数
template <typename T, typename Deleter = default_delete<T>>
struct ControlBlock {
    std::atomic<size_t> strong_count; // 强引用计数
    std::atomic<size_t> weak_count;   // 弱引用计数
    T* ptr;                           // 指向托管对象的裸指针
    Deleter deleter;                  // 删除器

    ControlBlock(T* p, const Deleter& del) noexcept 
        : strong_count(1), weak_count(0), ptr(p), deleter(del) {}

    ~ControlBlock() noexcept {
        if (ptr) {
            deleter(ptr); // 析构时删除托管对象
        }
    }
};

// 简化的 SharedPtr 类骨架（仅保留 reset 方法）
template <typename T, typename Deleter = default_delete<T>>
class SharedPtr {
private:
    T* ptr_ = nullptr;
    ControlBlock<T, Deleter>* control_block_ = nullptr;

public:
    //重置指针
    void reset(T* ptr = nullptr, const Deleter& deleter = Deleter{}) noexcept {
        // 1.处理旧资源：减少强引用计数，必要时销毁对象
        if (control_block_) {
            if (--(control_block_->strong_count) == 0) {
                if (ptr_) {
                    control_block_->deleter(ptr_); // 主动调用删除器
                }
                ptr_ = nullptr;
                // 检查弱引用是否为0，销毁控制块
                if (control_block_->weak_count == 0) {
                    delete control_block_; // 销毁控制块会触发其析构函数
                }
            }
            control_block_ = nullptr;
        }

        // 2.设置新资源
        ptr_ = ptr;
        if (ptr) {
            control_block_ = new ControlBlock<T, Deleter>(ptr, deleter);
        } else {
            control_block_ = nullptr;
        }
    }
};

// ------------------------------- 测试块：裸指针构造与引用计数 -------------------------------
int main() {
    {
        std::cout << "=== 测试：裸指针构造与引用计数 ===" << std::endl;
        // 构造托管int对象的SharedPtr
        int* raw_ptr = new int(100);
        SharedPtr<int> sp1(raw_ptr);

        assert(sp1.get() == raw_ptr);       // 裸指针匹配
        assert(sp1.use_count() == 1);       // 初始强引用计数为1
        assert(static_cast<bool>(sp1));     // bool转换为true
        assert(*sp1 == 100);                // 解引用正确

        std::cout << "裸指针构造测试通过！" << std::endl;

        // 触发reset，会暴露双重释放问题
        sp1.reset();
    }
    return 0;
}
```

## 二、双重释放问题分析
### 1. 核心问题定位
问题出现在 `reset` 方法和 `ControlBlock` 析构函数的**重复删除逻辑**，具体流程如下：
1. 当 `sp1.reset()` 被调用时，`control_block_->strong_count` 从 1 减至 0；
2. `reset` 方法中主动调用 `control_block_->deleter(ptr_)`，删除 `raw_ptr` 指向的 int 对象；
3. 此时 `weak_count` 为 0，执行 `delete control_block_`，触发 `ControlBlock` 的析构函数；
4. `ControlBlock` 析构函数中检查 `ptr` 不为空（仍指向原 `raw_ptr`），再次调用 `deleter(ptr)`，对同一个指针执行第二次删除 → **双重释放**。

### 2. 双重释放的危害
双重释放是严重的内存安全问题，可能导致：
- 程序崩溃（操作系统检测到非法内存操作）；
- 内存损坏，引发不可预测的程序行为；
- 安全漏洞（攻击者可利用内存损坏执行恶意代码）。

## 三、修复方案
### 核心思路
删除 `reset` 方法中**主动调用删除器**的逻辑，仅由 `ControlBlock` 析构函数统一处理资源释放（符合“单一职责”原则）。

### 修复后的代码
```cpp
#include <atomic>
#include <cassert>
#include <iostream>

// 默认删除器
template <typename T>
struct default_delete {
    void operator()(T* ptr) const noexcept {
        delete ptr;
    }
};

//-------------------------控制块定义-----------------------------
template <typename T, typename Deleter = default_delete<T>>
struct ControlBlock {
    std::atomic<size_t> strong_count;
    std::atomic<size_t> weak_count;
    T* ptr;
    Deleter deleter;

    ControlBlock(T* p, const Deleter& del) noexcept 
        : strong_count(1), weak_count(0), ptr(p), deleter(del) {}

    ~ControlBlock() noexcept {
        if (ptr) {
            deleter(ptr); // 仅由析构函数负责释放托管对象
        }
    }
};

template <typename T, typename Deleter = default_delete<T>>
class SharedPtr {
private:
    T* ptr_ = nullptr;
    ControlBlock<T, Deleter>* control_block_ = nullptr;

public:
    explicit SharedPtr(T* ptr) noexcept 
        : ptr_(ptr) {
        if (ptr) {
            control_block_ = new ControlBlock<T, Deleter>(ptr, Deleter{});
        }
    }

    size_t use_count() const noexcept {
        return control_block_ ? control_block_->strong_count.load() : 0;
    }

    T* get() const noexcept { return ptr_; }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T& operator*() const noexcept { return *ptr_; }

    void reset(T* ptr = nullptr, const Deleter& deleter = Deleter{}) noexcept {
        // 1.处理旧资源：减少强引用计数，必要时销毁控制块（由控制块析构释放对象）
        if (control_block_) {
            if (--(control_block_->strong_count) == 0) {
                ptr_ = nullptr; // 仅清空当前指针，不主动删除对象
                // 弱引用为0时销毁控制块，其析构函数会释放托管对象
                if (control_block_->weak_count == 0) {
                    delete control_block_;
                }
            }
            control_block_ = nullptr;
        }

        // 2.设置新资源
        ptr_ = ptr;
        if (ptr) {
            control_block_ = new ControlBlock<T, Deleter>(ptr, deleter);
        } else {
            control_block_ = nullptr;
        }
    }

    ~SharedPtr() noexcept {
        if (control_block_) {
            reset();
        }
    }
};

// 测试代码
int main() {
    {
        std::cout << "=== 测试：裸指针构造与引用计数 ===" << std::endl;
        int* raw_ptr = new int(100);
        SharedPtr<int> sp1(raw_ptr);

        assert(sp1.get() == raw_ptr);
        assert(sp1.use_count() == 1);
        assert(static_cast<bool>(sp1));
        assert(*sp1 == 100);

        std::cout << "裸指针构造测试通过！" << std::endl;

        // 调用reset，此时仅触发一次资源释放
        sp1.reset();
        assert(sp1.use_count() == 0);
        assert(!static_cast<bool>(sp1));
    }
    std::cout << "所有测试通过，无双重释放问题！" << std::endl;
    return 0;
}
```

### 关键修改点
| 位置 | 原始逻辑 | 修复后逻辑 |
|------|----------|------------|
| `reset` 方法 | 强引用计数为0时，主动调用 `deleter(ptr_)` | 移除主动删除逻辑，仅清空 `ptr_`，由 `ControlBlock` 析构函数统一释放 |

## 四、总结
### 1. 双重释放的根本原因
`reset` 方法和 `ControlBlock` 析构函数**重复执行资源释放逻辑**，导致同一指针被删除两次。

### 2. 修复核心原则
- 资源释放逻辑需“单一入口”：仅由 `ControlBlock` 析构函数负责托管对象的释放；
- 引用计数为0时，只需销毁控制块，无需手动调用删除器。

### 3. 开发注意事项
- 实现智能指针时，需严格区分“清空指针”和“释放资源”的边界；
- 原子操作（如引用计数）需保证线程安全，但核心逻辑仍需避免重复释放；
- 测试时需覆盖 `reset`、析构等所有触发资源释放的场景。
