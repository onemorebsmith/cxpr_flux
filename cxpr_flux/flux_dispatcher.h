#pragma once

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	template <typename allocator_t>
	class flux_dispatcher
	{
	public:
		using my_t = flux_dispatcher<allocator_t>;
		using signal_t = flux_signal_node;
		template <typename T>
		using signal_impl_t = flux_signal_impl<my_t,T>;
		using allocator_wrapper_t = stl_allocator_helper<allocator_t>;
		using arena_t = cxpr_flux::arena_allocator<1024 * 32>;

		template <typename T>
		using uniq_ptr = typename allocator_wrapper_t::template uniq_ptr<T>;

		constexpr flux_dispatcher(const allocator_t& _alloc)
			:	allocator(_alloc),
				arenaA(allocator_wrapper_t::template _allocate_one_uniq<arena_t>(allocator)),
				arenaB(allocator_wrapper_t::template _allocate_one_uniq<arena_t>(allocator)),
				currentAllocator(arenaA.get()),
				head(nullptr),
				tail(nullptr)
		{
		}

		template <typename payload_t>
		void signal(payload_t&& payload)
		{
			try
			{
				auto created = currentAllocator->construct<signal_impl_t<payload_t>>
					("Signal", std::move(payload));

				//BSTL::Threading::RAIISpinlock spinlock(lock);
				if (head == nullptr)
				{
					head = created;
					tail = head;
				}
				else
				{
					tail->next = created;
					tail = created;
				}

			}
			catch (std::bad_alloc)
			{

			}
		}

		template <typename func_t>
		std::pair<int, int> processSignals(func_t&& functor)
		{
			auto dispatcherState = swap_state();
			int nDispatched = 0;

			int nHandled = 0;
			auto currentSignal = dispatcherState.head;
			while (currentSignal != nullptr)
			{
				nHandled += functor(*currentSignal);
				//nHandled += currentSignal->dispatch(this);

				//nHandled += functor(*currentSignal);
				currentSignal = currentSignal->next;
				nDispatched++;
			}

			dispatcherState.allocator->purge();

			return std::make_pair(nDispatched, nHandled);
		}

	private:
		struct dispatcher_context
		{
			signal_t* head = nullptr;
			signal_t* tail = nullptr;
			arena_t* allocator = nullptr;
		};

		[[nodiscard]] dispatcher_context swap_state()
		{
			//BSTL::Threading::RAIISpinlock spinlock(lock);
			dispatcher_context contextOut = {};
			contextOut.head = head;
			contextOut.tail = tail;
			contextOut.allocator = currentAllocator;

			// swap allocators
			if (currentAllocator == arenaA.get())
			{
				currentAllocator = arenaB.get();
			}
			else
			{
				currentAllocator = arenaA.get();
			}

			head = nullptr;
			tail = nullptr;

			return std::move(contextOut);
		}


		allocator_t allocator;
		uniq_ptr<arena_t> arenaA;
		uniq_ptr<arena_t> arenaB;
		arena_t* currentAllocator;

		//BSTL::Threading::SpinlockT lock;
		signal_t* head;
		signal_t* tail;
	};
}