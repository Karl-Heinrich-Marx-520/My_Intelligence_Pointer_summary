#include "headfiles.h"

//------------------------------- SharedPtr 实现 -------------------------------
template <typename T, typename Deleter = default_delete<T>>
class SharedPtr {
private:
	T* ptr_;
	ControlBlock<T>* control_block_; //共享的控制块指针

public:
	// 友元声明：WeakPtr 需要访问SharedPtr的control_block_成员
	template <typename U, typename D>
	friend class WeakPtr;

	//默认构造
	SharedPtr() noexcept : ptr_(nullptr), control_block_(nullptr) {};

	//从裸指针构造
	explicit SharedPtr(T* ptr, const Deleter& deleter = Deleter{}) noexcept
		: ptr_(ptr) {
		if (ptr) {
			control_block_ = new ControlBlock<T>(ptr, deleter);
		}
		else {
			control_block_ = nullptr;
		}
	}

	//拷贝构造
	SharedPtr(const SharedPtr& other) noexcept
		: ptr_(other.ptr_), control_block_(other.control_block_) {
		if (control_block_) {
			++(control_block_->strong_count); // 增加引用计数
		}
	}

	//移动构造
	SharedPtr(SharedPtr&& other) noexcept
		: ptr_(other.ptr_), control_block_(other.control_block_) {
		other.ptr_ = nullptr;
		other.control_block_ = nullptr;
	}

	//拷贝赋值
	SharedPtr& operator=(const SharedPtr& other) noexcept {
		if (this != &other) {
			reset();
			ptr_ = other.ptr_;
			control_block_ = other.control_block_;
			if (control_block_) {
				++(control_block_->strong_count); // 增加引用计数
			}
		}
		return *this;
	}

	//移动赋值
	SharedPtr& operator=(SharedPtr&& other) noexcept {
		if (this != &other) {
			reset();
			ptr_ = other.ptr_;
			control_block_ = other.control_block_;
			other.ptr_ = nullptr;
			other.control_block_ = nullptr;
		}
		return *this;
	}

	//析构函数
	~SharedPtr() noexcept {
		reset();
	}

	//获取裸指针
	T* get() const noexcept {
		return ptr_;
	}

	//解引用和成员访问运算符
	T& operator*() const noexcept {
		return *ptr_;
	}
	T* operator->() const noexcept {
		return ptr_;
	}

	//获取强引用计数
	size_t use_count() const noexcept {
		return control_block_ ? control_block_->strong_count.load() : 0;
	}

	//重置指针
	void reset(T* ptr = nullptr, const Deleter& deleter = Deleter{}) noexcept {
		//1.处理旧资源：减少强引用计数，必要时销毁对象
		if (control_block_) {
			if (--(control_block_->strong_count) == 0) {
				control_block_->destroy_object(); //销毁托管对象
				//ptr_ = nullptr; //control_block_中destroy_object已置空裸指针，故此无需再置空

				if (control_block_->weak_count == 0) {
					delete control_block_;
				}
			}
			control_block_ = nullptr;
		}

		//2.设置新资源
		ptr_ = ptr;
		if (ptr) {
			control_block_ = new ControlBlock<T>(ptr, deleter);
		}
		else {
			control_block_ = nullptr;
		}
	}

	explicit operator bool() const noexcept {
		return ptr_ != nullptr;
	}

	//交换函数
	void swap(SharedPtr& other) noexcept {
		std::swap(ptr_, other.ptr_);
		std::swap(control_block_, other.control_block_);
	}
};

//SharedPtr 数组特化 （偏特化）
template <typename T, typename Deleter>
class SharedPtr<T[], Deleter> : public SharedPtr<T, Deleter> {
public:
	using SharedPtr<T, Deleter>::SharedPtr; // 继承构造函数
	//重载[]运算符
	T& operator[](std::size_t idx) const {
		return this->get()[idx];
	}

	//隐藏基类的*和->运算符，数组不支持这些操作
	T& operator*() const = delete;
	T* operator->() const = delete;
};
