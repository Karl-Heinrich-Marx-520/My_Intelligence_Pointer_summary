#include "SharedPtr.hpp"

struct TestObj {
    int val;
    void print() { std::cout << "TestObj: " << val << std::endl; }
};

struct CustomDeleter {
    void operator()(int* ptr) const noexcept {
        std::cout << "自定义删除器：销毁int对象，值为" << *ptr << std::endl;
        delete ptr;
    }
};

int main() {
    {
        // ------------------------------- 测试块1：默认构造函数 -------------------------------
        std::cout << "=== 测试1：默认构造函数 ===" << std::endl;
        // 默认构造的SharedPtr应为空
        SharedPtr<int> sp1;
        assert(sp1.get() == nullptr);       // 裸指针为空
        assert(sp1.use_count() == 0);       // 引用计数为0
        assert(!static_cast<bool>(sp1));    // bool转换为false

        std::cout << "默认构造测试通过！" << std::endl;
    }

    // ------------------------------- 测试块2：裸指针构造与引用计数 -------------------------------
    {
        std::cout << "=== 测试2：裸指针构造与引用计数 ===" << std::endl;
        // 构造托管int对象的SharedPtr
        int* raw_ptr = new int(100);
        SharedPtr<int> sp1(raw_ptr);

        assert(sp1.get() == raw_ptr);       // 裸指针匹配
        assert(sp1.use_count() == 1);       // 初始强引用计数为1
        assert(static_cast<bool>(sp1));     // bool转换为true
        assert(*sp1 == 100);               // 解引用正确

        std::cout << "裸指针构造测试通过！" << std::endl;
    }

    // ------------------------------- 测试块3：拷贝构造与拷贝赋值 -------------------------------
    {
        std::cout << "=== 测试3：拷贝构造与拷贝赋值 ===" << std::endl;
        SharedPtr<int> sp1(new int(200));

        // 拷贝构造
        SharedPtr<int> sp2(sp1);
        assert(sp1.use_count() == 2);
        assert(sp2.use_count() == 2);
        assert(*sp1 == *sp2 && *sp2 == 200);

        // 拷贝赋值
        SharedPtr<int> sp3;
        sp3 = sp1;
        assert(sp1.use_count() == 3);
        assert(sp3.use_count() == 3);

        // 自赋值（无副作用）
        sp1 = sp1;
        assert(sp1.use_count() == 3);

        std::cout << "拷贝构造/赋值测试通过！" << std::endl;
    }

    // ------------------------------- 测试块4：移动构造与移动赋值 -------------------------------
    {
        std::cout << "=== 测试4：移动构造与移动赋值 ===" << std::endl;
        SharedPtr<int> sp1(new int(300));
        size_t old_count = sp1.use_count();

        // 移动构造：原对象资源被转移，置空
        SharedPtr<int> sp2(std::move(sp1));
        assert(sp1.get() == nullptr);       // 原对象裸指针为空
        assert(sp1.use_count() == 0);       // 原对象计数为0
        assert(sp2.use_count() == old_count); // 新对象计数继承
        assert(*sp2 == 300);

        // 移动赋值
        SharedPtr<int> sp3;
        sp3 = std::move(sp2);
        assert(sp2.get() == nullptr);
        assert(*sp3 == 300);

        std::cout << "移动构造/赋值测试通过！" << std::endl;
    }

    // ------------------------------- 测试块5：reset方法 -------------------------------
    {
        std::cout << "=== 测试5：reset方法 ===" << std::endl;
        SharedPtr<int> sp1(new int(400));
        SharedPtr<int> sp2(sp1);

        // reset空指针：释放旧资源
        sp1.reset();
        assert(sp1.get() == nullptr);
        assert(sp2.use_count() == 1);       // 剩余一个引用

        // reset新资源：接管新对象
        sp2.reset(new int(500));
        assert(sp2.use_count() == 1);
        assert(*sp2 == 500);

        std::cout << "reset方法测试通过！" << std::endl;
    }

    // ------------------------------- 测试块6：运算符重载 -------------------------------
    {
        std::cout << "=== 测试6：运算符重载 ===" << std::endl;
        SharedPtr<TestObj> sp1(new TestObj{ 600 });

        // 解引用运算符*
        assert((*sp1).val == 600);

        // 成员访问运算符->
        sp1->print();
        assert(sp1->val == 600);

        // bool运算符
        assert(static_cast<bool>(sp1));
        sp1.reset();
        assert(!static_cast<bool>(sp1));

        std::cout << "运算符重载测试通过！" << std::endl;
    }

    // ------------------------------- 测试7：自定义删除器 -------------------------------
    {
        std::cout << "=== 测试7：自定义删除器 ===" << std::endl;
        // 使用自定义删除器构造SharedPtr
        SharedPtr<int, CustomDeleter> sp1(new int(700), CustomDeleter{});
        assert(*sp1 == 700);

        // reset触发删除器
        sp1.reset(); // 控制台会输出自定义删除器的打印信息

        std::cout << "自定义删除器测试通过！" << std::endl;
    }

    return 0;
}
