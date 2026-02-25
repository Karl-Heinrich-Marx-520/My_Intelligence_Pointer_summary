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

~ControlBlock() noexcept {
		if (ptr) {
			deleter(ptr);
		}
	}
};

  
  //重置指针
	void reset(T* ptr = nullptr, const Deleter& deleter = Deleter{}) noexcept {
		//1.处理旧资源：减少强引用计数，必要时销毁对象
		if (control_block_) {
			if (--(control_block_->strong_count) == 0) {
				if (ptr_) {
					control_block_->deleter(ptr_);
				}
				ptr_ = nullptr;
				//检查弱引用是否为0，销毁控制块
				if (control_block_->weak_count == 0) {
					delete control_block_;
				}
			}
			control_block_ = nullptr;
		}

//2.设置新资源
		ptr_ = ptr;
		if (ptr) {
			control_block_ = new ControlBlock<T, Deleter>(ptr, deleter);
		}
		else {
			control_block_ = nullptr;
		}
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
  这里存在一处相当细节的双重释放问题，长经验了
