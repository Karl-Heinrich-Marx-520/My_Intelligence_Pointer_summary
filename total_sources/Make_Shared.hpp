#include "SharedPtr.hpp"

//------------------------------- Make_Shared 实现 -------------------------------
template <typename T, typename... Args>
SharedPtr<T> Make_Shared(Args&&... args) {
	//1.创建对象
	T* obj = new T(std::forward<Args>(args)...);
	//2.创建控制块，使用默认删除器
	auto control_block = new ControlBlock<T>(obj, default_delete<T>());
	//3.返回 SharedPtr，传入对象指针和控制块指针
	return SharedPtr<T>(obj, control_block);
}
