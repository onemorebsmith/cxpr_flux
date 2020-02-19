#pragma once

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	//////////////////////////////////////////////////////////////////////////
	// Data structure that is implemented as a variant containing either nothing (monostate)
	// an std::array, or an std::vector depending on the number of elements
	//
	// !! Note: designed only to hold pointer types !!
	//
	// For 1 - 4 (usually) entries the underlying datatype will be a std::array inline with the class
	// For 4+ entries the underlying datatype will be a std::vector 
	template <typename data_t>
	struct variant_list
	{
	public:
		using vec_t = vector_t<data_t>;

		static constexpr size_t flat_size = sizeof(vec_t) / sizeof(data_t);
		using flat_t = std::array<data_t, flat_size>;
		using storage_t = std::variant<std::monostate, flat_t, vec_t>;

		constexpr variant_list(generic_allocator& all) noexcept : _allocator(all), variantList{ std::monostate{} }
		{
			static_assert(std::is_pointer_v<data_t>, "variant_list should only be used with pointers");
		}

		constexpr variant_list(variant_list&& other) noexcept 
			: _allocator(other._allocator), variantList{ std::move(other.variantList) } {}

		void push_back(data_t val)
		{
			std::visit([&](auto& store_type)	// might be slow?
			{
				push_back(val, store_type);
			}, variantList);
		}

		template <typename functor_t>
		void for_each(functor_t functor)
		{
			// jump table instead of visit
			switch (variantList.index())
			{
			case variant_type::flat_idx:
			{
				auto& asFlat = std::get<flat_t>(variantList);
				std::for_each(std::begin(asFlat), std::end(asFlat), [&](auto& it)
				{
					if (it != nullptr)
					{
						functor(it);
					}
				});

				break;
			}
			case variant_type::vector_idx:
			{
				auto& asVector = std::get<vec_t>(variantList);
				std::for_each(std::begin(asVector), std::end(asVector), [&](auto& it)
				{
					functor(it);
				});
				break;
			}
			case variant_type::monostate_idx: // intentional fallthrough
			default:
				break;
			}
		}

	private:
		generic_allocator& _allocator;
		storage_t variantList;
		enum variant_type
		{
			monostate_idx = 0,
			flat_idx = 1,
			vector_idx = 2
		};


		void push_back(data_t val, std::monostate& empty)
		{
			// currently empty
			flat_t arr = { val };
			variantList = std::move(arr);
		}

		void push_back(data_t val, flat_t& flat)
		{
			for (size_t i = 0; i < flat_size; i++)
			{
				if (flat[i] == nullptr)
				{
					flat[i] = val;
					return;
				}
			}

			// failed to insert, alloc a vector
			flat_t cpy = flat;
			variantList = vec_t{ _allocator };
			auto& v = std::get<vec_t>(variantList);
			v.reserve(flat_size * 2);
			std::copy(std::begin(cpy), std::end(cpy), std::back_inserter(v));
			v.push_back(val);
		}

		void push_back(data_t val, vec_t& v)
		{
			v.push_back(val);
		}
	};
}