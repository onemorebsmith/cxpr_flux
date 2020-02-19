#pragma once

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	//////////////////////////////////////////////////////////////////////////
	// stl_allocator_helper
	// Helper template for allocating unique_ptr/raw data using a stl-conforming allocator
	template <typename allocator_t>
	struct stl_allocator_helper
	{
		template <typename T>
		using rebind_alloc_t = typename std::allocator_traits<allocator_t>::template rebind_alloc<T>;

		template <typename obj_t, typename ... params_t>
		static constexpr decltype(auto) _allocate_one(allocator_t& allocator, param_pack_t params)
		{
			using rebind_traits_t = typename std::allocator_traits<allocator_t>::template rebind_traits<obj_t>;
			typename rebind_traits_t::allocator_type rebound = allocator;
			auto created = rebind_traits_t::allocate(rebound, 1);
			rebind_traits_t::construct(rebound, created, perfect_forward(params));
			return created;
		}

		template <typename obj_t>
		static constexpr decltype(auto) _deallocate_one(allocator_t& allocator, obj_t* ptr)
		{
			using rebind_traits_t = typename std::allocator_traits<allocator_t>::template rebind_traits<obj_t>;
			typename rebind_traits_t::allocator_type rebound = allocator;
			rebind_traits_t::destroy(rebound, ptr);
			rebind_traits_t::deallocate(rebound, ptr, 1);
		}

		// can't use a lambda as they're not swappable apparently. 
		struct _deleter
		{
			constexpr _deleter(const allocator_t& _allocator = {}) noexcept : allocator(_allocator) {}

			template <typename obj_t>
			constexpr void operator()(obj_t* ptr)
			{
				_deallocate_one<std::remove_pointer_t<decltype(ptr)>>(allocator, ptr);
			}

			allocator_t allocator;
		};

		template <typename interface_t, typename derived_t = interface_t, typename ... params_t>
		static decltype(auto) _allocate_one_uniq(allocator_t& allocator, param_pack_t params)
		{
			return uniq_ptr<interface_t>(_allocate_one<derived_t>(allocator,
				perfect_forward(params)), _deleter(allocator));
		}

		template <typename T>
		using uniq_ptr = std::unique_ptr<T, _deleter>;
	};
}