#pragma once

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
	template<typename signature_t>
	struct flux_callback;

	template<typename ret_t, typename ... params_t>
	struct flux_callback<ret_t(params_t ...)>
	{
	public:
		using my_t = flux_callback<ret_t(params_t ...)>;

		constexpr flux_callback() noexcept : impl(nullptr), inline_mem{} {}
		~flux_callback()
		{
			if (impl != nullptr)
			{
				impl->~internal_t();
			}
		}

		constexpr flux_callback(const my_t& other) noexcept
		{
			assign(other);
		}

		flux_callback& operator=(const my_t& other) noexcept
		{
			assign(other);
			return *this;
		}

		void assign(const my_t& other)
		{
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

		constexpr flux_callback(my_t&& other) noexcept
		{
			move_impl(std::move(other));
		}

		constexpr my_t& operator=(my_t&& other) noexcept
		{
			move_impl(std::move(other));
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

		template <typename lambda_t>
		decltype(auto) bind_lambda(lambda_t&& lam)
		{
			static_assert(sizeof(lam) <= max_internal_sz, "lambda is too large, reduce capture list");
			impl = new (inline_mem) __detail::bound_lambda<lambda_t, ret_t(params_t ...)>(std::forward<lambda_t>(lam));
			return impl;
		}

		constexpr operator bool() const { return impl != nullptr; }

	private:
		static constexpr size_t max_internal_sz = 24; // 24 + 8 = 32, which is a nicely aligned size
		using internal_t = __detail::bound_function_impl<ret_t(params_t ...)>;

		void move_impl(my_t&& other)
		{
			if (other.impl != nullptr)
			{
				__int64 offset = ((char*)other.impl) - ((char*)other.inline_mem);
				memcpy(inline_mem, other.inline_mem, sizeof(inline_mem));
				impl = (internal_t*)(inline_mem + offset);
				other.impl = nullptr;

				other.impl = nullptr;
			}
		}

		internal_t* impl = nullptr;
		alignas(std::alignment_of_v<internal_t>) char inline_mem[24];
	};

	//////////////////////////////////////////////////////////////////////////
	// callback_list
	// Wrapper for a vector of callbacks. All callbacks will be invoked on a call to ()
	template <typename sig_t, typename allocator_t = std::allocator<void>>
	class callback_list
	{
	public:
		template <typename T>
		using rebind_alloc_t =
			typename std::allocator_traits<allocator_t>::template rebind_alloc<T>;

		constexpr callback_list() = default;
		~callback_list() = default;

		constexpr callback_list(callback_list&& other) noexcept : callbacks(std::move(other.callbacks)) {}
		callback_list& operator=(callback_list&& other) noexcept { callbacks = std::move(other.callbacks); return *this; }

		template <typename lambda_t>
		void registerCallback(void* owner, lambda_t&& lam)
		{
			callbacks.emplace_back(std::make_pair(owner, flux_callback<sig_t>()))
				.second.bind_lambda(std::forward<lambda_t>(lam));
		}

		template <typename ...  Ts>
		constexpr void call(Ts&& ... params) noexcept
		{
			for (auto& it : callbacks)
			{
				it.second(perfect_forward(params));
			}
		}

	private:
		using entry_pair = std::pair<void*, flux_callback<sig_t>>;

		std::vector<entry_pair, rebind_alloc_t<entry_pair>> callbacks;
	};
}