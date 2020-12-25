#pragma once

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	namespace __detail
	{
		//////////////////////////////////////////////////////////////////////////
		// Internal state for a container. Must be heap-allocated so that if the container
		// is moved the bound lambdas don't lose a good this capture
		template <typename ... Ts>
		struct flux_container_state
		{
			bool isDirty = false;
			bool isReady = false;
			std::tuple<typename Ts::state_t...> states = {};

			template <typename ... Ts>
			flux_container_state(Ts&&... stores) : states{}
			{
				// giant fold statement
				(stores.addListener(this, [this](const auto& newState)
				{
					isReady = true;
					using payload_t = typename std::decay_t<decltype(newState)>::state_t;
					cxpr::find_tuple_type<payload_t>(states) = newState.getState();
					isDirty = true;
					onChanged.call();
				}), ...);
			}

			cxpr_flux::callback_list<void()> onChanged;
		};
	}

	//////////////////////////////////////////////////////////////////////////
	// flux_container
	// Binding class between the flux context and the view. Listens to all states in (Ts...) for changes
	// and updates it's internal state accordingly.
	template <typename ... Ts>
	struct flux_container
	{
		using state_t = __detail::flux_container_state<Ts...>;

		constexpr flux_container() noexcept : state{ nullptr } {}
		constexpr flux_container(flux_container&& other) noexcept : state{ std::move(other.state) } {}

		template <typename ctx_t>
		void bind(ctx_t& context)
		{
			// create our internal state as well as create & bind to context states
			state = std::make_unique<state_t>(*context.getStores().template createStore<Ts>()...);
		}

		template <typename ... Ts>
		void bindExisting(Ts&&... params)
		{
			// create our internal state as well as create & bind to context states
			state = std::make_unique<state_t>(perfect_forward(params));
		}

		template <typename store_t>
		const typename store_t::state_t& getState() const {
			// fetches the current state out of the state tuple
			return cxpr::find_tuple_type<typename store_t::state_t>(state->states);
		}

		bool isReady() const {
			return (state != nullptr) && (state->isReady);
		}


		template <typename functor_t>
		void addListener(void* owner, functor_t&& fun) const noexcept
		{
			state->onChanged.registerCallback(owner, std::forward<functor_t>(fun));
		}

		bool getResetDirty() { auto dirty = state->isDirty;  state->isDirty = false; }

		std::unique_ptr<state_t> state;
	};

	////////////////////////////////////////////////////////////////////////////
	//// Helper to infer context type when creating a container 
	//template <template <typename, typename> class container_t, typename view_t, typename context_t>
	//decltype(auto) create_container(context_t& ctx)
	//{
	//	return container_t<context_t, view_t>(ctx);
	//}

	//////////////////////////////////////////////////////////////////////////
	// Helper to infer context type when creating a container 
	template <template <typename, typename> class container_t, typename view_t, typename context_t, typename ... Ts>
	decltype(auto) create_container_view(context_t& ctx, Ts&&... params)
	{
		return container_t<context_t, view_t>(ctx, perfect_forward(params));
	}

	//////////////////////////////////////////////////////////////////////////
	// Helper to infer context type when creating a container 
	template <template <typename, typename> class container_t, typename context_t, typename ... Ts>
	decltype(auto) create_container(context_t& ctx, Ts&&... params)
	{
		return container_t<context_t>(ctx, perfect_forward(params));
	}
}