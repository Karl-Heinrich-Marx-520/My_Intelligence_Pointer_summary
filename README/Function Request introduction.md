智能指针实现
题目描述
实现 C++ 标准库中的智能指针，深入理解 RAII 机制和现代 C++ 内存管理。

【基础要求】
[] 实现 UniquePtr独占所有权语义 禁止拷贝，支持移动 支持 release(), reset(), get(), operator*, operator-> 支持自定义删除器
[] 实现 SharedPtr共享所有权语义 引用计数管理 支持拷贝和移动 支持 use_count(), reset(), get(), operator*, operator->

【进阶要求】
[] 实现 WeakPtr打破循环引用 支持 lock(), expired(), use_count()
[] 实现 make_unique和 make_shared函数模板
[] 支持数组类型 UniquePtr和 SharedPtr
[] 引用计数的线程安全（使用原子操作）

【挑战要求】
[] 实现类型擦除的删除器（SharedPtr）
[] 实现 enable_shared_from_this
[] 支持别名构造（aliasing constructor）
[] 控制块单次分配优化（make_shared 优化）

技术要求
- 禁止使用标准库智能指针
- 需要正确处理异常安全
- 理解并实现控制块（Control Block）结构

接口示例
// UniquePtr
auto up1 = make_unique<int>(42);
UniquePtr<int> up2 = std::move(up1);
std::cout << *up2 << std::endl;  // 42

// SharedPtr
auto sp1 = make_shared<std::string>("hello");
SharedPtr<std::string> sp2 = sp1;
std::cout << sp1.use_count() << std::endl;  // 2

// WeakPtr
WeakPtr<std::string> wp = sp1;
if (auto sp3 = wp.lock()) {
    std::cout << *sp3 << std::endl;  // "hello"
}

// 自定义删除器
UniquePtr<FILE, decltype(&fclose)> file(fopen("test.txt", "r"), &fclose);

测试要求
1. 基本生命周期测试
2. 引用计数正确性测试
3. 循环引用与 WeakPtr 测试
4. 异常安全测试
5. 移动语义测试
