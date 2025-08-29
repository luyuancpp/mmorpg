template<typename T>
class WarnOnSavePtr {
public:
	explicit WarnOnSavePtr(T* ptr) : ptr_(ptr) {}

	// 解引用支持
	T* operator->() const { return ptr_; }
	T& operator*()  const { return *ptr_; }

	// ✅ 判断空指针
	explicit operator bool() const { return ptr_ != nullptr; }

	// ✅ 支持与 nullptr 比较（不会触发 warning）
	friend bool operator==(const WarnOnSavePtr& lhs, std::nullptr_t) {
		return lhs.ptr_ == nullptr;
	}

	friend bool operator!=(const WarnOnSavePtr& lhs, std::nullptr_t) {
		return lhs.ptr_ != nullptr;
	}

	friend bool operator==(std::nullptr_t, const WarnOnSavePtr& rhs) {
		return rhs.ptr_ == nullptr;
	}

	friend bool operator!=(std::nullptr_t, const WarnOnSavePtr& rhs) {
		return rhs.ptr_ != nullptr;
	}

	// 🚨 真正用于保存的隐式转换（触发警告）
	[[deprecated("🚫 不要保存这个指针，它只在当前作用域有效。热更后使用会导致崩溃。")]]
	operator T* () const { return ptr_; }

	[[deprecated("🚫 不要保存这个指针，它只在当前作用域有效。热更后使用会导致崩溃。")]]
	T* Get() const { return ptr_; }

private:
	T* ptr_;
};
