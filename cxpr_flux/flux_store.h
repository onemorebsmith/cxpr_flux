#pragma once

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	//////////////////////////////////////////////////////////////////////////
	// flux_store
	// base class for all stores
	template <typename derived_t>
	class flux_store
	{
	public:
		constexpr flux_store() noexcept = default;
		constexpr flux_store(flux_store&& other) noexcept : onChanged(std::move(other.onChanged)) {}

		constexpr flux_store& operator=(flux_store&& other) noexcept
		{
			onChanged = std::move(other.onChanged);
			return *this;
		}

		template <typename functor_t>
		void addListener(void* owner, functor_t&& fun) noexcept
		{
			onChanged.registerCallback(owner, std::forward<functor_t>(fun));
		}

	protected:
		void emitChanged() final
		{
			onChanged.call(static_cast<derived_t&>(*this));
		}

	private:
		callback_list<void(const derived_t&)> onChanged;
	};	

	//////////////////////////////////////////////////////////////////////////
	// flux_store_facade
	// Contains logic around creating/destroying stores as well as provides callback functionality
	// for monitoring stores being created/destroyed
	template <typename _store_t, typename context_t>
	class flux_store_facade
	{
	public:
		using store_t = _store_t;
		using allocator_t = typename context_t::allocator_t;
		using callbacks_t = decltype(store_t::GetCallbacks());
		static constexpr callbacks_t callbacks = store_t::GetCallbacks();

		template <typename T>
		using rebind_alloc_t =
			typename std::allocator_traits<allocator_t>::template rebind_alloc<T>;

		constexpr flux_store_facade(context_t& _ctx, const allocator_t& _allocator) noexcept
			: context(_ctx), allocator(_allocator), stores(allocator) {}

		template <typename ... params_t>
		constexpr store_t* CreateStore(param_pack_t params)
		{
			auto& emplaced = stores.emplace_back(perfect_forward(params));
			onCreateCbs.call(emplaced, context);
			return &emplaced;
		}

		constexpr void DestroyStore(store_t* store)
		{
			onDestroyCbs.call(*store, context); // this seems wrong
			stores.erase(std::find_if(std::begin(stores), std::end(stores), [&](const auto& s)
			{
				return store == &s;
			}));
		}

		template <typename signal_t>
		constexpr int dispatch(const signal_t& signal)
		{
			int nHandled = 0;
			cxpr::visit_tuple([&](const auto& cb)
			{
				using payload_t = typename std::decay_t<decltype(cb)>::payload_t;
				if constexpr (std::is_same_v<payload_t, signal_t>)
				{
					for (auto& s : stores)
					{
						cb.notify(s, context, signal);
						nHandled++;
					}
				}

			}, callbacks);

			return nHandled;
		}

		context_t& context;
		allocator_t allocator;
		// deque so we don't invalidate on insert/delete
		std::deque<store_t, rebind_alloc_t<store_t>> stores;
		callback_list<void(store_t&, context_t& ctx)> onCreateCbs;
		callback_list<void(store_t&, context_t& ctx)> onDestroyCbs;
	};
}