#include "headfiles.h"

//------------------------------- UniquePtr 实现 ------------------------------
template <typename T, typename Deleter = default_delete<T>>
class UniquePtr {
private:
	T* ptr_;
	Deleter deleter_;

public:
	//默认构造： 空指针，默认删除器
	UniquePtr() noexcept : ptr_(nullptr), deleter_() {};

	//从裸指针构造（接受自定义删除器）
	explicit UniquePtr(T* ptr, Deleter d = Deleter()) noexcept
		:ptr_(ptr), deleter_(std::move(d)) {
	};

	//禁止拷贝构造和拷贝赋值
	UniquePtr(const UniquePtr&) = delete;
	UniquePtr& operator=(const UniquePtr&) = delete;

	//移动构造
	UniquePtr(UniquePtr&& other) noexcept
		:ptr_(other.release()), deleter_(std::move(other.deleter_)) {
	};

	//移动赋值
	UniquePtr& operator=(UniquePtr&& other) noexcept {
		if (this != &other) {
			this->reset(other.release());
			this->deleter_ = std::move(other.deleter_);
		}
		return *this;
	}

	//析构： 调用删除器销毁对象
	~UniquePtr() {
		reset();
	}

	//释放所有权，返回裸指针
	T* release() noexcept {
		T* p = ptr_;
		ptr_ = nullptr;
		return p;
	}

	//重置指针，销毁原对象
	void reset(T* p = nullptr) noexcept {
		T* old_ptr = ptr_;
		ptr_ = p;
		if (old_ptr != nullptr) {
			deleter_(old_ptr);
		}
	}

	//获取裸指针
	T* get() const noexcept {
		return ptr_;
	}

	//重载*和->运算符
	T& operator* () const {
		assert(ptr_ != nullptr); // 确保不解引用空指针
		return *ptr_;
	}

	T* operator->() const {
		assert(ptr_ != nullptr); // 确保不访问空指针
		return ptr_;
	}

	//获取删除器
	const Deleter& get_deleter() const noexcept {
		return deleter_;
	}

	//布尔转换，判断是否持有对象
	explicit operator bool() const noexcept {
		return ptr_ != nullptr;
	}
};

//UniquePtr 数组特化 （偏特化）
template <typename T, typename Deleter>
class UniquePtr<T[], Deleter> : public UniquePtr<T, Deleter> {
public:
	using UniquePtr<T, Deleter> ::UniquePtr; // 继承构造函数

	//重载[]运算符
	T& operator[](std::size_t idx) const {
		return this->get()[idx];
	}

	//隐藏基类的*和->运算符，数组不支持这些操作
	T& operator*() const = delete;
	T* operator->() const = delete;
};
