#pragma once

#include <cstdint>
#include <xtr1common>
#include <type_traits>

#define NOT_DERIVED_ERROR() static_assert(std::is_base_of<T, T2>::value, "T2 is not derived from T");

namespace Eden
{
	template<class T>
	class SharedPtr
	{
	public:
		SharedPtr() = default;

		SharedPtr(std::nullptr_t nType)
			: m_Instance(nullptr)
		{
		}

		SharedPtr(T* instance)
			: m_Instance(instance)
			, m_RefCount(enew uint32_t(0))
		{
			IncRef();
		}

		~SharedPtr()
		{
			DecRef();
		}

		// Copy semantics
		SharedPtr(const SharedPtr& other) // Copy constructor
		{
			m_Instance = other.m_Instance;
			m_RefCount = other.m_RefCount;

			if (other.m_Instance != nullptr)
				IncRef();
		}

		SharedPtr& operator=(const SharedPtr& other) // Copy assignment operator
		{
			DecRef();

			m_Instance = other.m_Instance;
			m_RefCount = other.m_RefCount;

			if (other.m_Instance != nullptr)
				IncRef();

			return *this;
		}

		template<class T2>
		SharedPtr(const SharedPtr<T2>& other)
		{
			NOT_DERIVED_ERROR();

			m_Instance = other.m_Instance;
			m_RefCount = other.m_RefCount;

			if (other.m_Instance != nullptr)
				IncRef();
		}

		template<class T2>
		SharedPtr& operator=(const SharedPtr<T2>& other)
		{
			NOT_DERIVED_ERROR();

			DecRef();

			m_Instance = other.m_Instance;
			m_RefCount = other.m_RefCount;

			if (other.m_Instance != nullptr)
				IncRef();

			return *this;
		}

		// Move semantics
		SharedPtr(SharedPtr&& other) noexcept // Move constructor
		{
			m_Instance = other.m_Instance;
			m_RefCount = other.m_RefCount;

			other.m_Instance = nullptr;
			other.m_RefCount = nullptr;
		}

		SharedPtr& operator=(SharedPtr&& other) // Move assignment operator
		{
			DecRef();

			m_Instance = other.m_Instance;
			m_RefCount = other.m_RefCount;

			other.m_Instance = nullptr;
			other.m_RefCount = nullptr;

			return *this;
		}

		template<class T2>
		SharedPtr(SharedPtr<T2>&& other)
		{
			NOT_DERIVED_ERROR();

			m_Instance = other.m_Instance;
			m_RefCount = other.m_RefCount;

			other.m_Instance = nullptr;
			other.m_RefCount = nullptr;
		}

		template<class T2>
		SharedPtr& operator=(SharedPtr<T2>&& other)
		{
			NOT_DERIVED_ERROR();

			DecRef();

			m_Instance = other.m_Instance;
			m_RefCount = other.m_RefCount;

			other.m_Instance = nullptr;
			other.m_RefCount = nullptr;

			return *this;
		}

		// null assignment operator
		SharedPtr& operator=(std::nullptr_t)
		{
			DecRef();
			m_Instance = nullptr;
			return *this;
		}

		operator bool() const { return m_Instance != nullptr; }

		T* operator->() { return m_Instance; }
		const T* operator->() const { return m_Instance; }

		template <class T2 = T, std::enable_if_t<!std::disjunction_v<std::is_array<T2>, std::is_void<T2>>, int> = 0>
		T2& operator*() const { return *m_Instance; }

		T* Get() const { return m_Instance; }

		void Reset(T* instance = nullptr)
		{
			DecRef();
			m_Instance = instance;
		}

		template<typename T2>
		SharedPtr<T2> As() const
		{
			return SharedPtr<T2>(*this);
		}

		template<typename... Args>
		static SharedPtr<T> Create(Args&&... args)
		{
#if ED_TRACK_MEMORY
			return SharedPtr<T>(new(typeid(T).name()) T(std::forward<Args>(args)...));
#else
			return SharedPtr<T>(new T(std::forward<Args>(args)...));
#endif
		}

		bool operator==(const SharedPtr<T>& other) const
		{
			return m_Instance == other.m_Instance;
		}

		bool operator!=(const SharedPtr<T>& other) const
		{
			return !(*this == other);
		}

		const bool IsValid() const
		{
			return m_Instance != nullptr;
		}

	private:
		void IncRef() const
		{
			if (m_Instance)
			{
				++(*m_RefCount);
			}
		}

		void DecRef() const
		{
			if (m_Instance)
			{
				--(*m_RefCount);
				if (*m_RefCount == 0)
				{
					delete m_Instance;
					m_Instance = nullptr;

					edelete m_RefCount;
					m_RefCount = nullptr;
				}
			}
		}

	private:
		template<class T2>
		friend class SharedPtr;

		mutable T* m_Instance = nullptr;
		mutable uint32_t* m_RefCount = nullptr;
	};

	// Helper
	template<class T, typename... Args>
	SharedPtr<T> MakeShared(Args&&... args)
	{
		return SharedPtr<T>::Create(std::forward<Args>(args)...);
	}
}

