#pragma once

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	struct deallactor_entry_node
	{
		__int64 nextOffset = 0;	// intentionally using offset instead of pointers so we're movable
		virtual void destruct() = 0;
	};

	//////////////////////////////////////////////////////////////////////////

	template <typename obj_t>
	struct deallactor_entry : public deallactor_entry_node
	{
		template <typename ... params_t>
		constexpr deallactor_entry(param_pack_t params) : obj(perfect_forward(params)) {}

		virtual void destruct() override { obj.~obj_t(); }

		obj_t obj;
	};

	//////////////////////////////////////////////////////////////////////////
	// arena_allocator
	// Allocator class that allocates entries within a fixed-size internal buffer
	// Allocator explicitly owns all objects allocated with it and will destroy
	// owned objects on a call to purge. Allocator is expected to be reused 
	// with usage being similar to:
	//		do work (alloc entries) -> process work -> purge -> repeat
	template <typename allocator_t, size_t slab_size = 1024 * 8, bool throwOOM = true>
	struct arena_allocator
	{
	public:
		static constexpr auto max_sz = slab_size;
		static constexpr auto max_allocation_sz = max_sz;

		template <typename obj_t> using allocated_t = obj_t*;
		using allocator_wrapper_t = stl_allocator_helper<allocator_t>;
		template <typename T>
		using uniq_ptr = typename allocator_wrapper_t::template uniq_ptr<T>;

		constexpr arena_allocator(const allocator_t& _alloc = allocator_t{}) noexcept : allocator(_alloc), control{}
		{
			control.currentSize = sizeof(control_block);
		}

		//constexpr arena_allocator(arena_allocator&& other) noexcept
		//	: control{}
		//{
		//	memcpy(_mem, other._mem, max_sz); // this copies the control block also
		//	memset(other._mem, 0, max_sz);  // this zeros the size and nulls out control block also 
		//}

		~arena_allocator() { purge(); }

		constexpr decltype(auto) alloc(size_t sz, const char* tag = nullptr, int alignment = 8)
		{
			if(sz <= max_allocation_sz)
			{
				const __int64 memEnd = ((__int64)(&control)) + max_sz;
				const __int64 currentHead = ((__int64)(&control.memstart) + control.currentSize);
				// calc the span we need to advance to be properly aligned by masking off the bottom bits of the current address
				const __int64 alignmentOffset = currentHead - (currentHead & (~(alignment - 1)));
				// generate our new aligned head
				const __int64 alignedHead = currentHead + alignmentOffset;
				// adjust size for the alignment changes
				const __int64 totalSize = sz + alignmentOffset;

				if ((alignedHead + totalSize) < memEnd)
				{
					control.currentAllocations++;
					control.currentSize += static_cast<unsigned int>(totalSize);
					return reinterpret_cast<void*>(alignedHead);
				}
				else
				{
					// we're saturated, chain up another allocator to delegate to
					if (chain == nullptr)
					{
						chain = allocator_wrapper_t::template _allocate_one_uniq<arena_allocator>(allocator);
					}

					return chain->alloc(sz, tag, alignment);
				}
			}
			else
			{
				throw std::bad_alloc();
			}
		}

		template <typename obj_t, typename ... params_t>
		obj_t* alloc_construct(param_pack_t params)
		{
			auto ll = lock.scoped_lock();
			void* mem = alloc(sizeof(obj_t), "arena", static_cast<int>(std::alignment_of_v<obj_t>));
			return new(mem) obj_t(perfect_forward(params));
		}

		template <typename obj_t, typename ... params_t>
		obj_t* construct(const char* tag, param_pack_t params)
		{
			if constexpr (std::is_trivially_destructible_v<obj_t>)
			{
				// type doesnt need to be destructed later, simply zeroing it's mem will suffice
				return alloc_construct<obj_t>(perfect_forward(params));
			}
			else
			{
				// type needs an explicit destructor call, wrap it in a node and add it to the chain
				using wrapped_t = deallactor_entry<obj_t>;
				auto created = alloc_construct<wrapped_t>(perfect_forward(params));

				if (created == nullptr)
				{
					return nullptr;
				}

				auto currentHead = control.currentHeadOffset.load(std::memory_order::memory_order_relaxed);
				while (true)
				{
					__int64 offset = currentHead;
					__int64 myOffset = ((__int64)created) - ((__int64)(&control.memstart));
					created->nextOffset = offset;

					if (control.currentHeadOffset.compare_exchange_strong(currentHead, myOffset))
					{
						break;
					}
				}
				
				return &created->obj;
			}
		}

		void purge()
		{
			auto ll = lock.scoped_lock();
			if (control.currentHeadOffset > 0)
			{
				deallactor_entry_node* node = reinterpret_cast<deallactor_entry_node*>(
					((char*)(&control.memstart)) + static_cast<int>(control.currentHeadOffset));
				while (true)
				{
					node->destruct();
					if (node->nextOffset > 0)
					{
						node = reinterpret_cast<deallactor_entry_node*>(((char*)(&control.memstart)) + node->nextOffset);
					}
					else
					{
						break;
					}
				}

			}

			memset(_mem, 0, max_sz); // this zeros the size and nulls out head also 
			control.currentSize = sizeof(control_block);
			control.currentAllocations = 0;

			if (chain != nullptr)
			{
				chain->purge();
			}
		}

	private:
		struct control_block
		{
			std::atomic<__int64> currentHeadOffset = 0;
			unsigned int currentSize = 0;
			unsigned int currentAllocations	= 0;
			unsigned char memstart;
		};

		union 
		{
			struct  
			{
				allocator_t allocator;
				flux_spinlock lock;
				control_block control;
			};
			unsigned char __declspec(align(16)) _mem[max_sz];
		};

		uniq_ptr<arena_allocator> chain;
	};
}