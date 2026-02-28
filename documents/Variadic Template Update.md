# 重构文档：智能指针实现 - 变参模板改造
## 文档信息
| 项⽬                | 内容                                                                 |
|---------------------|----------------------------------------------------------------------|
| 重构目标            | 将手动实现的 C++ 智能指针（UniquePtr/SharedPtr/WeakPtr）及配套函数改造为**变参模板** |
| 适用仓库            | C++ 智能指针手动实现仓库                                             |
| 核心技术点          | 变参模板（Variadic Templates）、完美转发（Perfect Forwarding）、参数包展开       |
| 重构范围            | 智能指针核心接口（构造/重置）、make_unique/make_shared 等工厂函数               |
| 兼容性              | 保持原有 RAII 语义、线程安全特性、接口行为与标准库一致，仅扩展参数适配能力         |

---

## 1. 重构背景与目标
### 1.1 背景
原有智能指针实现的模板仅支持**固定参数/单参数**的对象构造，例如：
- `make_unique<T>()` 仅支持无参构造，或仅能传递单个参数给 T 的构造函数；
- `UniquePtr::reset()` 无法直接传递多个参数构造新对象，需先手动创建裸指针；
- 上述限制导致智能指针无法适配带多参数构造函数的自定义类型，与标准库 `std::unique_ptr/std::shared_ptr` 的易用性存在差距。

### 1.2 核心目标
1. 基于**变参模板**重构智能指针核心接口，支持任意数量、任意类型的构造参数；
2. 结合 `std::forward` 实现**完美转发**，保证参数的左值/右值属性不丢失，避免不必要的拷贝；
3. 保持原有 RAII 机制、引用计数逻辑、线程安全特性完全不变；
4. 与 C++11 及以上标准兼容，符合标准库智能指针的接口语义。

### 1.3 重构范围
| 模块/函数               | 重构必要性 | 核心改造点                     |
|-------------------------|------------|--------------------------------|
| `UniquePtr`             | 高         | 构造函数、reset() 成员函数     |
| `SharedPtr`             | 高         | 构造函数、reset() 成员函数     |
| `WeakPtr`               | 低         | 无核心改动（依赖 SharedPtr）   |
| `make_unique`           | 最高       | 核心改造为变参模板             |
| `make_shared`           | 最高       | 核心改造为变参模板             |
| 控制块（ControlBlock）| 中         | 适配变参构造的对象创建逻辑     |

---

## 2. 核心概念：变参模板与完美转发
### 2.1 变参模板基础
变参模板允许模板接收**任意数量、任意类型**的参数（参数包），核心语法：
- `template <typename T, typename... Args>`：`Args` 为参数包，`...` 表示参数包展开；
- `Args&&... args`：接收任意数量的左值/右值参数；
- `std::forward<Args>(args)...`：完美转发参数包，保留参数的原始值类型（左值/右值）。

### 2.2 智能指针场景的价值
- 支持自定义类型的**多参数构造函数**（例如 `make_shared<Person>("张三", 20)`）；
- 避免手动创建裸指针（`new Person("张三", 20)`），提升代码安全性；
- 与标准库智能指针接口完全对齐，降低使用成本。

---

## 3. 核心重构方案
### 3.1 改造原则
1. **向下兼容**：原有单参数/无参调用方式完全保留，不破坏现有代码；
2. **语义不变**：RAII、引用计数、线程安全等核心逻辑无改动；
3. **极简扩展**：仅在需要适配多参数的接口中引入变参模板，不冗余改造。

### 3.2 分模块详细改造
#### 3.2.1 `make_unique`：变参模板改造（核心）
##### 原有实现（局限性）
仅支持无参或单参数构造：
```cpp
// 原有实现：仅支持无参构造
template <typename T>
UniquePtr<T> make_unique() {
    return UniquePtr<T>(new T());
}

// 原有实现：仅支持单个参数
template <typename T, typename Arg>
UniquePtr<T> make_unique(Arg&& arg) {
    return UniquePtr<T>(new T(std::forward<Arg>(arg)));
}
```

##### 重构后实现（变参模板）
```cpp
// 重构后：支持任意数量、任意类型的参数
template <typename T, typename... Args>
UniquePtr<T> make_unique(Args&&... args) {
    // 完美转发参数包给 T 的构造函数
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

// 数组版本（保持原有逻辑，无需变参）
template <typename T>
UniquePtr<T[]> make_unique(size_t size) {
    return UniquePtr<T[]>(new typename std::remove_extent<T>::type[size]());
}
```

#### 3.2.2 `UniquePtr`：成员函数 `reset` 改造
##### 原有实现（局限性）
仅支持传入裸指针，无法直接构造对象：
```cpp
template <typename T, typename Deleter = DefaultDeleter<T>>
class UniquePtr {
public:
    // 原有 reset：仅支持裸指针
    void reset(T* ptr = nullptr) {
        if (m_ptr != ptr) {
            m_deleter(m_ptr);
            m_ptr = ptr;
        }
    }
};
```

##### 重构后实现（新增变参版 reset）
```cpp
template <typename T, typename Deleter = DefaultDeleter<T>>
class UniquePtr {
public:
    // 保留原有裸指针版本（向下兼容）
    void reset(T* ptr = nullptr) {
        if (m_ptr != ptr) {
            m_deleter(m_ptr);
            m_ptr = ptr;
        }
    }

    // 新增：变参模板版 reset，直接构造对象
    template <typename... Args>
    void reset(Args&&... args) {
        // 1. 销毁原有对象
        m_deleter(m_ptr);
        // 2. 用变参构造新对象，完美转发参数
        m_ptr = new T(std::forward<Args>(args)...);
    }

private:
    T* m_ptr = nullptr;
    Deleter m_deleter;
};
```

#### 3.2.3 `make_shared` 与 `SharedPtr` 改造
`make_shared` 的核心改造逻辑与 `make_unique` 一致，重点是控制块适配变参构造：

##### 1. 控制块改造（支持变参构造）
```cpp
// 通用控制块（变参模板版）
template <typename T, typename... Args>
struct ObjectControlBlock : ControlBlockBase {
    T m_object;  // 直接存储对象（make_shared 内存优化）

    // 变参构造：完美转发参数给 T 的构造函数
    ObjectControlBlock(Args&&... args) : m_object(std::forward<Args>(args)...) {}

    void destroy() override {
        // 无需额外操作，析构时自动调用 m_object 的析构函数
    }

    void deallocate() override {
        delete this;
    }
};
```

##### 2. `make_shared` 重构实现
```cpp
// 重构后：变参模板版 make_shared
template <typename T, typename... Args>
SharedPtr<T> make_shared(Args&&... args) {
    // 1. 创建控制块（包含对象），完美转发参数
    auto* cb = new ObjectControlBlock<T, Args...>(std::forward<Args>(args)...);
    // 2. 创建 SharedPtr，关联控制块和对象
    return SharedPtr<T>(&cb->m_object, cb);
}
```

##### 3. `SharedPtr::reset` 改造（同 UniquePtr）
```cpp
template <typename T>
class SharedPtr {
public:
    // 保留原有裸指针版本
    void reset(T* ptr = nullptr) {
        // 原有引用计数逻辑...
    }

    // 新增：变参模板版 reset
    template <typename... Args>
    void reset(Args&&... args) {
        // 1. 释放原有资源（引用计数-1）
        release();
        // 2. 创建新对象，关联新控制块
        auto* cb = new ObjectControlBlock<T, Args...>(std::forward<Args>(args)...);
        m_ptr = &cb->m_object;
        m_control_block = cb;
        // 3. 初始化引用计数
        m_control_block->ref_count = 1;
        m_control_block->weak_count = 0;
    }

private:
    T* m_ptr = nullptr;
    ControlBlockBase* m_control_block = nullptr;
};
```

---

## 4. 重构后使用示例
### 4.1 自定义多参数构造类型
```cpp
// 示例：带多参数构造函数的自定义类型
struct Person {
    std::string name;
    int age;
    std::string address;

    // 三参数构造函数
    Person(std::string&& n, int a, std::string&& addr) 
        : name(std::move(n)), age(a), address(std::move(addr)) {}
};
```

### 4.2 变参模板版智能指针使用
```cpp
// 1. make_unique 支持多参数
auto up = make_unique<Person>("张三", 25, "北京市朝阳区");

// 2. make_shared 支持多参数
auto sp = make_shared<Person>("李四", 30, "上海市浦东新区");

// 3. UniquePtr::reset 直接构造对象
UniquePtr<Person> up2;
up2.reset("王五", 28, "广州市天河区");  // 无需手动 new

// 4. SharedPtr::reset 直接构造对象
SharedPtr<Person> sp2;
sp2.reset("赵六", 35, "深圳市南山区");
```

---

## 5. 测试验证方案
### 5.1 核心测试点
| 测试场景                | 验证内容                                                                 |
|-------------------------|--------------------------------------------------------------------------|
| 无参构造                | `make_unique<Person>()` （若 Person 有无参构造）正常工作                 |
| 单参数构造              | `make_unique<Person>("张三")` 兼容原有逻辑                               |
| 多参数构造              | `make_shared<Person>("张三", 25, "北京")` 正确构造对象                   |
| 右值参数转发            | 验证移动语义：`make_unique<Person>(std::string("张三"), 25)` 无多余拷贝  |
| reset 变参调用          | `up.reset("李四", 30, "上海")` 正确销毁旧对象、创建新对象                |
| 引用计数正确性          | 多参数构造的 SharedPtr 引用计数仍准确（线程安全）                       |
| 异常安全                | 构造函数抛异常时，无内存泄漏（控制块正确释放）                           |

### 5.2 测试代码示例
```cpp
// 测试多参数构造与完美转发
TEST(SharedPtrTest, VariadicMakeShared) {
    // 构造带右值参数的对象
    auto sp = make_shared<Person>(std::string("张三"), 25, std::string("北京"));
    
    // 验证对象属性
    ASSERT_EQ(sp->name, "张三");
    ASSERT_EQ(sp->age, 25);
    ASSERT_EQ(sp->address, "北京");
    
    // 验证引用计数
    ASSERT_EQ(sp.use_count(), 1);
}

// 测试 reset 变参调用
TEST(UniquePtrTest, VariadicReset) {
    UniquePtr<Person> up;
    up.reset("李四", 30, "上海");
    
    ASSERT_EQ(up->name, "李四");
    ASSERT_EQ(up->age, 30);
    
    // 再次 reset，验证旧对象销毁
    up.reset("王五", 28, "广州");
    ASSERT_EQ(up->name, "王五");
}
```

---

## 6. 注意事项与最佳实践
### 6.1 关键注意事项
1. **完美转发的坑**：参数包展开时必须使用 `std::forward<Args>(args)...`，而非 `std::move`，否则会丢失左值属性；
2. **模板参数推导**：变参模板会自动推导参数类型，无需手动指定 `Args`，例如 `make_shared<Person>("张三", 25)` 无需写 `make_shared<Person, const char*, int>("张三", 25)`；
3. **C++ 版本兼容**：变参模板是 C++11 特性，需确保编译选项开启 `-std=c++11` 或更高；
4. **数组类型排除**：`make_unique<T[]>` 仅支持 size 参数，无需变参，需通过 `std::enable_if` 区分数组/非数组类型（避免歧义）。

### 6.2 最佳实践
1. **优先使用 make 函数**：重构后，所有对象创建优先使用 `make_unique/make_shared`（变参版），避免手动 `new`；
2. **避免过度转发**：仅在构造对象的接口中使用变参模板，非核心接口（如 `get()`/`use_count()`）无需改造；
3. **异常安全**：变参构造时，若对象构造抛异常，控制块会自动释放，无需额外处理（保持原有 RAII 语义）。

---

## 7. 重构后代码结构
重构后仓库结构与原仓库一致，仅修改以下文件的核心逻辑：
| 文件路径                  | 改动内容                                   |
|---------------------------|--------------------------------------------|
| `total_sources/UniquePtr.hpp` | 新增变参版 reset()、改造 make_unique       |
| `total_sources/SharedPtr.hpp` | 新增变参版 reset()、改造 make_shared       |
| `total_sources/ControlBlock.hpp` | 新增变参版 ObjectControlBlock 模板       |
| `tests/SharedPtrTest.cpp`  | 新增多参数构造、变参 reset 测试用例        |
| `tests/UniquePtrTest.cpp`  | 新增多参数构造、变参 reset 测试用例        |

---

### 总结
1. 本次重构核心是通过**变参模板 + 完美转发**，让智能指针支持任意参数的对象构造，对齐标准库接口；
2. 重构保持原有 RAII、引用计数、线程安全等核心逻辑不变，仅扩展参数适配能力；
3. 关键改动点为 `make_unique/make_shared` 工厂函数、`reset` 成员函数，以及控制块的变参构造适配。

### 后续建议
1. 完成重构后，补充多参数构造的单元测试，覆盖边界场景（如空参数、右值参数、抛异常的构造函数）；
2. 在 README 中新增“变参模板使用示例”章节，说明重构后的扩展能力；
3. 若需兼容 C++03，可通过宏控屏蔽变参模板逻辑（但不推荐，变参模板是现代 C++ 核心特性）。
