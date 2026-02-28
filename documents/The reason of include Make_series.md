你想弄明白为什么C++中使用智能指针（`unique_ptr`/`shared_ptr`）时，业界和标准库都推荐用`make_unique`/`make_shared`模板函数创建，而非直接通过构造函数（比如`std::unique_ptr<T>(new T)`），我会从**直接构造的核心问题**和**make系列的优势**两方面，结合实际场景讲清楚背后的原因。

### 一、先看直接构造智能指针的核心问题
直接通过`std::unique_ptr<T>(new T)`或`std::shared_ptr<T>(new T)`构造，会暴露三个关键问题，其中**异常安全**是最致命的。

#### 1. 最核心：异常安全问题（内存泄漏风险）
这是`make`系列函数存在的首要原因。C++编译器对函数参数的求值顺序没有严格规定，直接构造可能导致“new出的内存无人接管”。

举个典型例子：
```cpp
#include <memory>
#include <stdexcept>

// 模拟一个可能抛异常的函数
int compute_value() {
    throw std::runtime_error("计算出错！");
}

// 接收智能指针和整数的函数
void func(std::unique_ptr<int> ptr, int value) {}

int main() {
    // 直接构造智能指针作为函数参数
    func(std::unique_ptr<int>(new int(10)), compute_value());
    return 0;
}
```
你可能以为执行顺序是：
`new int(10)` → 构造`unique_ptr`接管内存 → 调用`compute_value()` → 调用`func()`

但编译器可能重排为：
`new int(10)` → 调用`compute_value()`（抛异常） → 构造`unique_ptr`（永远执行不到）

此时，`new int(10)`分配的内存没有被智能指针接管，也没有任何代码能释放它，直接导致**内存泄漏**。

而用`make_unique`则完全规避这个问题：
```cpp
// 安全写法：make_unique内部完成new+构造，不会被重排
func(std::make_unique<int>(10), compute_value());
```
`make_unique`的逻辑是在函数内部完成“new对象 + 构造智能指针”，这两步是原子性的（相对于参数求值）。即使`compute_value()`抛异常，`make_unique`创建的智能指针会在函数调用栈回退时析构，自动释放内存。

#### 2. 代码冗余且不优雅
直接构造需要手动写`new`，代码重复且繁琐，尤其是类型名较长时：
```cpp
// 直接构造：重复写类型、手动new
std::unique_ptr<MyComplexClass> p1(new MyComplexClass(10, "test"));
std::shared_ptr<MyComplexClass> p2(new MyComplexClass(10, "test"));

// make系列：简洁，无需new，auto推导类型
auto p1 = std::make_unique<MyComplexClass>(10, "test");
auto p2 = std::make_shared<MyComplexClass>(10, "test");
```

#### 3. make_shared专属问题：内存分配效率低
`shared_ptr`的内部结构包含两部分：
- 指向对象的指针（对象内存）；
- 控制块（存储引用计数、弱引用计数、删除器等）。

直接构造`shared_ptr`会触发**两次内存分配**：
1. `new MyClass()`：分配对象内存；
2. 标准库内部：为控制块分配内存。

而`make_shared`会将“对象内存 + 控制块”分配在**同一块连续内存**中，仅触发**一次内存分配**，带来两个好处：
- 减少`malloc/free`系统调用的开销，提升性能；
- 减少内存碎片（连续内存比分散内存更高效）。

### 二、make_unique/make_shared的核心优势
| 优势                | 说明                                                                 |
|---------------------|----------------------------------------------------------------------|
| 异常安全（核心）| 避免参数求值顺序导致的内存泄漏，是最核心的价值                       |
| 代码简洁            | 无需手动写`new`，减少冗余，提升可读性                               |
| 内存优化（仅make_shared） | 一次分配对象+控制块，减少内存分配次数和碎片                         |
| 符合RAII最佳实践    | 完全封装裸指针（`new`），避免裸指针暴露带来的误操作（比如重复释放） |

### 三、什么时候必须直接构造（make系列的局限性）
`make`系列并非万能，以下场景只能用直接构造：

#### 1. 需要自定义删除器时
`make_unique`/`make_shared`不支持传入自定义删除器，比如释放文件指针、自定义内存池对象：
```cpp
// 自定义删除器：关闭文件指针
auto file_deleter = [](FILE* fp) { fclose(fp); };

// 必须直接构造（make_unique无法传入删除器）
std::unique_ptr<FILE, decltype(file_deleter)> fp_ptr(fopen("test.txt", "r"), file_deleter);
```

#### 2. 类的构造函数是私有/保护的
`make`系列函数内部会调用类的构造函数，若构造函数是私有的，`make`系列无法访问，只能直接构造（通常配合友元或工厂函数）。

#### 3. 分阶段初始化对象
极少数场景下，需要先分配内存、做初始化操作，再交给智能指针接管，此时只能直接构造。

### 四、补充说明
- `make_unique`是C++14才引入的（C++11遗漏了这个函数），C++11中若要使用类似功能，需要自己实现简易版：
  ```cpp
  template<typename T, typename... Args>
  std::unique_ptr<T> make_unique(Args&&... args) {
      return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
  }
  ```
- `make_shared`的小缺点：由于对象和控制块绑定，即使最后一个`shared_ptr`销毁，只要还有`weak_ptr`指向控制块，对象内存就无法释放（延迟释放）；而直接构造的`shared_ptr`，对象内存会在最后一个`shared_ptr`销毁时释放。

### 总结
1. `make_unique`/`make_shared`的核心价值是**异常安全**，能避免直接构造时因参数求值顺序导致的内存泄漏；
2. 次要优势是代码简洁，`make_shared`还能优化内存分配，符合RAII最佳实践；
3. 仅在需要自定义删除器、访问私有构造函数等场景下，才需要直接构造智能指针。
