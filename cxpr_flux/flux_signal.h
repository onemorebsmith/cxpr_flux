#pragma once

using namespace cxpr;

namespace cxpr_flux
{
	struct flux_signal
	{
		virtual ~flux_signal() = default;
		virtual const void* payload() const = 0;
		virtual cxpr::hash_t hash() const = 0;
	};

	//////////////////////////////////////////////////////////////////////////

	struct flux_signal_node : public flux_signal
	{
		flux_signal_node* next = nullptr;
	};

	//////////////////////////////////////////////////////////////////////////

	template <typename dispatcher_t, typename payload_t>
	struct flux_signal_impl : public flux_signal_node
	{
		template <typename ... params_t>
		flux_signal_impl(params_t&& ... params) : data(std::forward<params_t>(params)...) {}

		virtual ~flux_signal_impl() = default;
		virtual const void* payload() const override { return &data; }
		virtual cxpr::hash_t hash() const override { return cxpr::typehash_v<payload_t>; }

		payload_t data;
	};

	//////////////////////////////////////////////////////////////////////////

	template <typename _payload_t, typename functor_t>
	struct signal_functor_callback 
	{
		using payload_t = _payload_t;

		constexpr signal_functor_callback(signal_functor_callback&& other) noexcept
			: functor(std::move(other.functor)) {}

		constexpr signal_functor_callback(functor_t&& _functor) noexcept
			: functor(std::forward<functor_t>(_functor))  {}

		template <typename store_t, typename context_t>
		constexpr void notify(store_t& store, context_t& ctx, const payload_t& changes) const
		{
			functor(store, changes, ctx);
		}

		functor_t functor;
	};


	template <typename payload_t, typename functor_t>
	constexpr decltype(auto) make_callback(functor_t&& fun)
	{
		return signal_functor_callback<payload_t, functor_t>(std::forward<functor_t>(fun));
	}
}

