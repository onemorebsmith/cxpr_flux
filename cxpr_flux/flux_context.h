#pragma once

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	//////////////////////////////////////////////////////////////////////////

	template <typename derived_t>
	class flux_store
	{
	public:
		constexpr flux_store() noexcept = default;
		constexpr flux_store(flux_store&& other) noexcept : onChanged(std::move(other.onChanged)) {}

		flux_store& operator=(flux_store&& other)
		{
			onChanged = std::move(other.onChanged);
			return *this;
		}

		template <typename functor_t>
		void addListener(void* owner, functor_t&& fun) const noexcept
		{
			onChanged.registerCallback(owner, std::forward<functor_t>(fun));
		}

	protected:
		void emitChanged()
		{
			onChanged.call(static_cast<derived_t&>(*this));
		}

	private:
		mutable callback_list<void(const derived_t&)> onChanged;
	};	

	//////////////////////////////////////////////////////////////////////////

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

	//////////////////////////////////////////////////////////////////////////

	template <typename context_t, typename ... stores_t>
	class static_store_collection
	{
	public:
		using allocator_t = typename context_t::allocator_t;
		using my_t = static_store_collection<allocator_t, stores_t...>;

		template <typename store_t>
		using facade_t = flux_store_facade<store_t, context_t>;

		using stores_tuple_t = std::tuple<flux_store_facade<stores_t, context_t>...>;

		constexpr static_store_collection(context_t& _ctx, const allocator_t& allocator) noexcept
			: stores{ stores_tuple_t{ flux_store_facade<stores_t, context_t>( _ctx, allocator)... } }
		{}

		template <typename store_t, typename ... params_t>
		constexpr decltype(auto) createStore(param_pack_t params) noexcept
		{
			return cxpr::find_tuple_type<facade_t<store_t>>(stores)
				.CreateStore(perfect_forward(params));
		}

		template <typename store_t>
		constexpr decltype(auto) destroyStore(store_t* store) noexcept
		{
			return cxpr::find_tuple_type<facade_t<store_t>>(stores)
				.DestroyStore(store);
		}

		template <typename store_t, typename callback_t>
		constexpr void onCreate(void* owner, callback_t&& cb)
		{
			cxpr::find_tuple_type<facade_t<store_t>>(stores)
				.onCreateCbs.registerCallback(owner, std::forward<callback_t>(cb));
		}
		template <typename store_t, typename callback_t>
		constexpr void onDestroy(void* owner, callback_t&& cb)
		{
			cxpr::find_tuple_type<facade_t<store_t>>(stores)
				.onDestroyCbs.registerCallback(owner, std::forward<callback_t>(cb));
		}

		template <typename signal_t>
		constexpr int dispatchSignal(const signal_t& signal)
		{
			int ndispatched = 0;
			cxpr::visit_tuple([&](auto& store) constexpr
			{
				ndispatched += store.dispatch(signal);
			}, stores);

			return ndispatched;
		}

		stores_tuple_t stores;
	};

	namespace __detail
	{
		//////////////////////////////////////////////////////////////////////////
		// required helper-function to infer the decayed types we need to fold over
		template <typename store_facade_t, typename ... messages_t>
		constexpr decltype(auto) dispatch_table_impl(cxpr::typeset<messages_t...> token)
		{
			// Generate a static lookup map of all signals we care about
			using functor_t = int(*)(store_facade_t & ctx, const flux_signal & var);
			return cxpr::make_static_map<cxpr::hash_t, functor_t>(
				{
					{	// pair start
						// folding is fun. Generate a functor that takes in our untyped signal, up casts it,
						// and the passes the typed signal to the store facade to dispatch correctly
						cxpr::typehash_v<std::decay_t<messages_t>>,		   // first
						
						[](store_facade_t& ctx, const flux_signal& signal) // second
						{
							using payload_t = std::decay_t<messages_t>;
							const auto& typed = *static_cast<const payload_t*>(signal.payload());
							return ctx.dispatchSignal(typed);
						}
					}...
				});
		}

		template <typename T>
		using fetch_payload_t = typename T::payload_t;

		//////////////////////////////////////////////////////////////////////////

		template <typename store_facade_t, typename ... stores_t>
		constexpr decltype(auto) generate_dispatch_table()
		{
			// get a list of all callbacks for all the stores, and collapse to a single tuple
			using callbacks_t = cxpr::tuple_unique_t<cxpr::collapse_tuples_t<decltype(stores_t::GetCallbacks())...>>;
			// reduce the callbacks to just their payload type
			using payloads_t = cxpr::mutate_types_t<callbacks_t, fetch_payload_t>;
			// generate a token to infer types in the next call
			constexpr payloads_t token = {};
			return dispatch_table_impl<store_facade_t>(token);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// std::allocator aware flat flux context
	template <typename _allocator_t = std::allocator<void>, typename ... stores_t>
	class flux_static_context
	{
	public:
		using allocator_t = _allocator_t;
		using my_t = flux_static_context<allocator_t, stores_t...>;
		using dispatcher_t = flux_dispatcher<allocator_t>;
		using store_facade_t = static_store_collection<my_t, stores_t...>;

		using allocator_wrapper_t = stl_allocator_helper<allocator_t>;

		template <typename T>
		using uniq_ptr = typename allocator_wrapper_t::template uniq_ptr<T>;

	public:
		constexpr flux_static_context() noexcept(std::is_nothrow_default_constructible_v<allocator_t>)
			: flux_static_context(allocator_t{}) {}

		constexpr flux_static_context(const allocator_t& _alloc)
			: allocator(_alloc), dispatcher(nullptr), stores(nullptr)
		{
			dispatcher = allocator_wrapper_t::template _allocate_one_uniq<dispatcher_t>(allocator, allocator);
			stores = allocator_wrapper_t::template _allocate_one_uniq<store_facade_t>(allocator, *this, allocator);
		}

		virtual dispatcher_t& getDispatcher() noexcept{ return *dispatcher; }
		constexpr store_facade_t& getStores() noexcept { return *stores; }

		decltype(auto) processSignals()
		{
			constexpr auto dispatchTable = __detail::generate_dispatch_table<store_facade_t, stores_t...>();
			return dispatcher->processSignals([&](const auto& signal)
			{
				int nHandled = 0;
				// find the entry in the map, validate it, and call
				auto [found, entry] = dispatchTable.get_entry(signal.hash());
				if (found)
				{
					nHandled = std::invoke(*entry, *stores, signal);
				}
				return nHandled;
			});
		}

	private:
		allocator_t allocator; // must be declared first
		uniq_ptr<dispatcher_t> dispatcher;
		uniq_ptr<store_facade_t> stores;
	};
}