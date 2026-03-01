#include "SharedPtr.hpp"

template <typename T, typename Deleter = default_delete<T>>
class WeakPtr {
private:
	T* ptr_;
	ControlBlock<T>* control_block_;  //共享的控制块指针

public:
	// 友元声明：SharedPtr 需要访问WeakPtr的control_block_成员
	template <typename U, typename D>
	friend class SharedPtr;

	//默认构造
	WeakPtr() noexcept : ptr_(nullptr), control_block_(nullptr) {};

	//从SharedPtr构造
	WeakPtr(const SharedPtr<T, Deleter>& shared_ptr) noexcept
		: ptr_(shared_ptr.ptr_), control_block_(shared_ptr.control_block_) {
		if (control_block_) {
			++(control_block_->weak_count); //增加弱引用计数
		}
	}

	//拷贝构造
	WeakPtr(const WeakPtr& other) noexcept
		: ptr_(other.ptr_), control_block_(other.control_block_) {
		if (control_block_) {
			++(control_block_->weak_count); //增加弱引用计数
		}
	}

	//移动构造
	WeakPtr(WeakPtr&& other) noexcept
		: ptr_(other.ptr_), control_block_(other.control_block_) {
		other.ptr_ = nullptr;
		other.control_block_ = nullptr;
	}

	//拷贝赋值
	WeakPtr& operator=(const WeakPtr& other) noexcept {
		if (this != &other) {
			reset();
			ptr_ = other.ptr_;
			control_block_ = other.control_block_;
			if (control_block_) {
				++(control_block_->weak_count); //增加弱引用计数
			}
		}
		return *this;
	}

	//移动赋值
	WeakPtr& operator=(WeakPtr&& other) noexcept {
		if (this != &other) {
			reset();
			ptr_ = other.ptr_;
			control_block_ = other.control_block_;
			other.ptr_ = nullptr;
			other.control_block_ = nullptr;
		}
		return *this;
	}

	// 从 SharedPtr 赋值
	WeakPtr& operator=(const SharedPtr<T, Deleter>& shared_ptr) noexcept {
		reset(); //释放弱引用
		ptr_ = shared_ptr.ptr_;
		control_block_ = shared_ptr.control_block_;
		if (control_block_) {
			++(control_block_->weak_count);
		}
		return *this;
	}

	//析构函数
	~WeakPtr() noexcept {
		reset();
	}

	// 核心：检测指针是否过期（强引用计数为 0 则过期）
	bool expired() const noexcept {
		return (!control_block_) || control_block_->strong_count.load() == 0;
	}

	// 核心：生成可用的 SharedPtr（原子性检查，线程安全）
	SharedPtr<T, Deleter> lock() const noexcept {
		if (expired()) return SharedPtr<T, Deleter>(); //返回空的 SharedPtr

		// 强引用计数+1，生成有效的 SharedPtr
		SharedPtr<T, Deleter> shared_ptr;
		shared_ptr.ptr_ = ptr_;
		shared_ptr.control_block_ = control_block_;
		++(control_block_->strong_count);
		return shared_ptr;
	}

	// 获取强引用计数（仅用于查看，不保证实时性）
	size_t use_count() const noexcept {
		return expired() ? 0 : control_block_->strong_count.load();
	}

	//重置弱引用
	void reset() noexcept {
		if (control_block_) {
			if (--(control_block_->weak_count) == 0 &&
				control_block_->strong_count.load() == 0) {
				delete control_block_;
			}
			control_block_ = nullptr;
		}
		ptr_ = nullptr;
	}

	// 交换两个 WeakPtr
	void swap(WeakPtr& other) noexcept {
		std::swap(ptr_, other.ptr_);
		std::swap(control_block_, other.control_block_);
	}
};
