#pragma once

//////////////////////////////////////////////////////////////////////////

#include "Kinetic3DPch.h"

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	//////////////////////////////////////////////////////////////////////////

	template <typename node_t>
	struct node_entry
	{
		using my_t = node_entry<node_t>;

		template <typename ... params_t>
		node_entry(param_pack_t params) : node(perfect_forward(params)), next(nullptr) {}

		my_t* next;
		node_t node;
	};

	//////////////////////////////////////////////////////////////////////////

	template <typename context_t>
	struct node_ll
	{
		using node_t = node_entry<context_t>;

		constexpr node_ll() : head(nullptr), tail(nullptr) {}

		template <typename allocator_t, typename ... params_t>
		constexpr context_t* create_insert(allocator_t& allocator, param_pack_t params)
		{
			using namespace BSTL::Memory::Allocators;
			auto created = object_allocator::allocRaw<node_t>(allocator, perfect_forward(params));
			if (head != nullptr)
			{
				tail->next = created;
				tail = created;
			}
			else
			{
				head = created;
				tail = head;
			}

			return &created->node;
		}

		template <typename functor_t>
		void for_each(functor_t ff)
		{
			auto node = head;
			while (node != nullptr)
			{
				ff(&node->node);
				node = node->next;
 			}
		}

		node_t* head = nullptr;
		node_t* tail = nullptr;
	};
}