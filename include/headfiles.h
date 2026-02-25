#define _CRT_SECURE_NO_WARNINGS

#include <atomic>
#include <iostream>
#include <memory>
#include <utility>
#include <cassert>

//-------------------------默认删除器，使用delete操作符销毁对象-----------------------------
template <typename T>
struct default_delete {
	void operator()(T* ptr) const noexcept{
		delete ptr;
	}
};

template <typename T>
struct default_delete<T[]> {
	void operator()(T* ptr) const noexcept{
		delete[] ptr;
	}
};

//-------------------------控制块定义-----------------------------
//统一的控制块，包含强引用计数和弱引用计数
template <typename T, typename Deleter = default_delete<T>>
struct ControlBlock {
	std::atomic<size_t> strong_count; // 强引用计数
	std::atomic<size_t> weak_count;   // 弱引用计数
	T* ptr;                          // 指向托管对象的裸指针
	Deleter deleter;                 // 删除器

	ControlBlock(T* p, const Deleter& del) noexcept
		: strong_count(1), weak_count(0), ptr(p), deleter(del) {}

	~ControlBlock() noexcept = default;
};

