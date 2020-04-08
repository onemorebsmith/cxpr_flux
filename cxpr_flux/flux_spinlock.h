#pragma once

//////////////////////////////////////////////////////////////////////////

namespace cxpr_flux
{
	static constexpr bool SpinlockUnlocked = false;
	static constexpr bool SpinlockLocked = true;

	struct flux_spinlock
	{
		constexpr flux_spinlock(bool initial_state = SpinlockUnlocked) : lock(initial_state) {}
		std::atomic_bool lock;

		struct scoped_spinlock
		{
			scoped_spinlock(flux_spinlock& _flagRef) : flagRef(_flagRef)
			{
				while (flagRef.lock.exchange(true) == true) {}
			}

			~scoped_spinlock() { flagRef.lock.store(false); }

		private:
			flux_spinlock& flagRef;
		};

		[[nodiscard]] decltype(auto) scoped_lock() { return scoped_spinlock(*this); }
	};

	
}