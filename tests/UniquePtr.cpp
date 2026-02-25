#include "headfiles.h"
#include "UniquePtr.cpp"

//-----------------------------------------------测试代码-------------------------------
struct Test {
	int x;
	Test(int v) : x(v) { std::cout << "Test constructed (x= " << x << ")\n"; }
	~Test() { std::cout << "Test destructed (x= " << x << ")\n"; }
};

//自定义删除器
template <bool IsArray>
struct CustomDeleter {
	void operator()(Test* ptr) const noexcept {
		if (IsArray) {
			std::cout << "CustomDeleter: Deleting array of Test\n";
			delete[] ptr;
		}
		else {
			std::cout << "CustomDeleter: Deleting single Test\n";
			delete ptr;
		}
	}
};


//------------------------------- main函数测试 -------------------------------
int main() {
	std::cout << "========测试1：单个对象的基本操作（构造，移动，->，*）========\n";
	{
		UniquePtr<Test> p1(new Test(42));
		assert(p1);
		std::cout << "p1->x = " << p1->x << std::endl;
		std::cout << "p1.x = " << (*p1).x << std::endl;

		//移动构造
		UniquePtr<Test> p2 = std::move(p1);
		std::cout << "p2->x = " << p2->x << std::endl;

		//移动赋值
		UniquePtr<Test> p3;
		p3 = std::move(p2);
		std::cout << "p3->x = " << p3->x << std::endl;
	}

	std::cout << "\n========测试2：数组版本========\n";
	{
		UniquePtr<Test[]> arr(new Test[3]{ Test(10), Test(20), Test(30) });
		assert(arr);
		std::cout << "arr[0].x = " << arr[0].x << std::endl;
		std::cout << "arr[1].x = " << arr[1].x << std::endl;
		std::cout << "arr[2].x = " << arr[2].x << std::endl;
	}

	std::cout << "\n===== 测试3：reset操作（销毁原对象，指向新对象）=====\n";
	{
		UniquePtr<Test> p(new Test(50));
		std::cout << "Before reset: p->x = " << p->x << std::endl;
		p.reset(new Test(60));
		std::cout << "After reset: p->x = " << p->x << std::endl;
	}

	std::cout << "\n===== 测试4：release操作（释放所有权）=====\n";
	{
		UniquePtr<Test> p(new Test(70));
		Test* raw_ptr = p.release();
		assert(!p);
		std::cout << "After release: raw_ptr->x = " << raw_ptr->x << std::endl;
		delete raw_ptr; // 需要手动删除
	}

	std::cout << "\n===== 测试5：自定义删除器 =====\n";
	{
		UniquePtr<Test, CustomDeleter<false>> p(new Test(80), CustomDeleter<false>());
		std::cout << "p->x = " << p->x << std::endl;
	}

	std::cout << "\n===== 测试6：空指针测试（默认构造、reset(nullptr)）=====\n";
	{
		UniquePtr<Test> p_null;

		p_null.reset();

		UniquePtr<Test[]> p_arr_null;

		p_arr_null.reset();

		UniquePtr<Test> p_temp(new Test(90));

		p_temp.reset();
	}

	std::cout << "\n===== 测试7：验证拷贝构造/赋值被禁用（取消注释会编译错误）=====\n";
	{
		//UniquePtr<Test> p1(new Test(100));
		//UniquePtr<Test> p2(p1); // 错误：拷贝构造被删除
		//UniquePtr<Test> p3;
		//p3 = p1; // 错误：拷贝赋值被删除
	}

	std::cout << "\n所有测试完成！\n";
	return 0;
}
