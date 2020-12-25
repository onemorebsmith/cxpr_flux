#pragma once

#include <vector>

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	// This header implements a type-erased, bound lambda similar to std::function.
	// bound_function_impl (base interface)
	// bound_lambda (impl)
	// flux_callback (wrapper)

	namespace __detail
	{
		template<typename signature_t>
		struct bound_function_impl;

		template<typename ret_t, typename ... params_t>
		struct bound_function_impl<ret_t(params_t ...)>
		{
			virtual ~bound_function_impl() = default;
			virtual ret_t operator()(params_t... p) = 0;
		};

		//////////////////////////////////////////////////////////////////////////

		template<typename lambda_t, typename signature_t>
		struct bound_lambda;

		template<typename lambda_t, typename ret_t, typename ... params_t>
		struct bound_lambda<lambda_t, ret_t(params_t ...)> : bound_function_impl<ret_t(params_t ...)>
		{
			template <typename ll_t>
			constexpr bound_lambda(ll_t&& lam) : lambda(std::forward<ll_t>(lam)) {}
			virtual ~bound_lambda() = default;

			virtual ret_t operator()(params_t... p) override
			{
				return lambda(perfect_forward(p));
			}

		private:
			lambda_t lambda;
		};
	}

	//////////////////////////////////////////////////////////////////////////
	// flux_callback
	// Wrapper class that implements a type-erased callable for a lambda matching the signature:  ret_t(params_t ...)
	// When bind_lambda is called, the proper derived type (bound_lambda<ret_t(params_t ...)>) is allocated
	// within the internal buffer of the callback. Note: this callback + it's captures must be <= the size of 
	// the internal buffer or it'll throw a compiler error.
	template<size_t capture_size, typename signature_t>
	struct flux_callback_base;

	template<size_t capture_size, typename ret_t, typename ... params_t>
	struct flux_callback_base<capture_size, ret_t(params_t ...)>
	{
	public:
		using my_t = flux_callback_base<capture_size, ret_t(params_t ...)>;
		static constexpr int sentinel_value = 1234567;

		constexpr flux_callback_base() noexcept : impl(nullptr), inline_mem{}, sentinel{ sentinel_value }{}
		~flux_callback_base()
		{
			if (impl != nullptr)
			{
				impl->~internal_t();
			}
		}

		constexpr flux_callback_base(const my_t& other) noexcept
		{
			assign(other);
		}

		flux_callback_base& operator=(const my_t& other) noexcept
		{
			assign(other);
			return *this;
		}

		void assign(const my_t& other)
		{
			sentinel = other.sentinel;
			precondition_check(sentinel == sentinel_value);
			if (impl != nullptr)
			{
				impl->~internal_t();
			}
			if (other.impl != nullptr)
			{
				__int64 offset = ((char*)other.impl) - ((char*)other.inline_mem);
				memcpy(inline_mem, other.inline_mem, sizeof(inline_mem));
				impl = (internal_t*)(inline_mem + offset);
			}
		}

		constexpr flux_callback_base(my_t&& other) noexcept
		{
			sentinel = other.sentinel;
			move_impl(std::move(other));
		}

		constexpr my_t& operator=(my_t&& other) noexcept
		{
			sentinel = other.sentinel;
			move_impl(std::move(other));
			return *this;
		}
		
		// Invokes the wrapped function and returns the result in std::optional<ret_t>
		template <typename ... Ts>
		decltype(auto) operator()(Ts&&... p)
		{
			if constexpr (std::is_same_v<ret_t, void>)
			{
				// void return, no optional
				if (impl != nullptr)
				{
					(*(impl))(perfect_forward(p));
				}
			}
			else
			{
				// non-void return, no optional
				if (impl != nullptr)
				{
					return (*(impl))(perfect_forward(p));
				}

				return std::optional<ret_t>{};
			}
		}

		template <typename ... Ts>
		decltype(auto) operator()(Ts&&... p) const 
		{
			if constexpr (std::is_same_v<ret_t, void>)
			{
				// void return, no optional
				if (impl != nullptr)
				{
					(*(impl))(perfect_forward(p));
				}
			}
			else
			{
				// non-void return, no optional
				if (impl != nullptr)
				{
					return (*(impl))(perfect_forward(p));
				}

				return std::optional<ret_t>{};
			}
		}

		template <typename lambda_t>
		decltype(auto) bind_lambda(lambda_t&& lam)
		{
			precondition_check(sentinel == sentinel_value);
			auto offset = (__int64)(inline_mem) % std::alignment_of_v<lambda_t>;
			auto sz = sizeof(__detail::bound_lambda<lambda_t, ret_t(params_t ...)>);
			static_assert(bound_payload_size<lambda_t>() <= max_internal_sz, "lambda is too large, reduce capture list");
			impl = new (inline_mem) __detail::bound_lambda<lambda_t, ret_t(params_t ...)>(std::forward<lambda_t>(lam));
			precondition_check(sentinel == sentinel_value);
			return impl;
		}

		template <typename obj_t, typename functor_t, typename ... params_t>
		decltype(auto) bind_member(obj_t& obj, functor_t& fun, params_t&&... params)
		{
			precondition_check(sentinel == sentinel_value);
			precondition_check(sentinel == sentinel_value);
			return impl;
		}


		template <typename lambda_t>
		static constexpr size_t bound_payload_size() { return sizeof(__detail::bound_lambda<lambda_t, ret_t(params_t ...)>); };

		constexpr operator bool() const { return impl != nullptr; }

	private:
		static constexpr size_t max_internal_sz = capture_size; // 24 + 8 = 32, which is a nicely aligned size
		using internal_t = __detail::bound_function_impl<ret_t(params_t ...)>;

		void move_impl(my_t&& other)
		{
			if (other.impl != nullptr)
			{
				__int64 offset = ((char*)other.impl) - ((char*)other.inline_mem);
				memcpy(inline_mem, other.inline_mem, sizeof(inline_mem));
				impl = (internal_t*)(inline_mem + offset);
				other.impl = nullptr;
			}
		}

		internal_t* impl = nullptr;
		alignas(std::alignment_of_v<internal_t>) char inline_mem[max_internal_sz];
		int sentinel;
	};

	static constexpr size_t small_callback_size = 24;	// 32 bit total size
	static constexpr size_t big_callback_size = 104;

	template <typename sig_t> // 32 bit total size
	using flux_callback = flux_callback_base<small_callback_size, sig_t>;

	template <typename sig_t>  // 128 bit total size
	using flux_big_callback = flux_callback_base<big_callback_size, sig_t>;


	//////////////////////////////////////////////////////////////////////////
	// callback_list
	// Wrapper for a vector of callbacks. All callbacks will be invoked on a call to ()
	template <size_t lambda_size, typename sig_t, typename allocator_t = std::allocator<void>>
	class callback_list_base
	{
	public:
		template <typename T>
		using rebind_alloc_t =
			typename std::allocator_traits<allocator_t>::template rebind_alloc<T>;

		using callback_t = flux_callback_base<lambda_size, sig_t>;

		constexpr callback_list_base() = default;
		~callback_list_base() = default;

		constexpr callback_list_base(callback_list_base&& other) noexcept 
			: callbacks(std::move(other.callbacks)) {}
		callback_list_base& operator=(callback_list_base&& other) noexcept 
		{ callbacks = std::move(other.callbacks); return *this; }

		template <typename lambda_t>
		void registerCallback(void* owner, lambda_t&& lam)
		{
			using decayed_t = std::decay_t<lambda_t>;
			if constexpr (std::is_same_v<decayed_t, callback_t>)
			{	// no need to wrap, already wrapped in a flux_callback
				callbacks.emplace_back(std::make_pair(owner, std::forward<lambda_t>(lam)));
			}
			else
			{	// naked lambda, need to wrap to stor
				callbacks.emplace_back(std::make_pair(owner, callback_t()))
					.second.bind_lambda(std::forward<lambda_t>(lam));
			}
		}

		template <typename ...  Ts>
		constexpr void call(Ts&& ... params) noexcept
		{
			for (auto& it : callbacks)
			{
				it.second(perfect_forward(params));
			}
		}

		constexpr void clearCallback(void* owner)
		{
			if (callbacks.size() > 0)
			{
				callbacks.erase(std::find_if(std::begin(callbacks), std::end(callbacks),
					[&](const auto& entry)
				{
					return entry.first == owner;
				}));
			}
		}

	private:
		using entry_pair = std::pair<void*, callback_t>;

		std::vector<entry_pair, rebind_alloc_t<entry_pair>> callbacks;
	};

	template <typename sig_t>
	using callback_list = callback_list_base<small_callback_size, sig_t>;

	template <typename sig_t>
	using big_callback_list = callback_list_base<big_callback_size, sig_t>;
}