#include <iostream>

#include "gtest/gtest.h"

#include <cxpr_flux.h>
#include <thread>

//////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4307) // integer overflow during hashing

//////////////////////////////////////////////////////////////////////////

using namespace cxpr;

//////////////////////////////////////////////////////////////////////////

namespace __arena_tests
{
	static int destructorCounter = 0;
	struct destructor_test
	{
		int i = 0;
		destructor_test(int z) : i(z) {}

		~destructor_test()
		{
			destructorCounter++;
		}
	};
}

//////////////////////////////////////////////////////////////////////////

TEST(arena_allocator_tests, arena_allocator_raw_alloc)
{
	using namespace __arena_tests;
	using allocator_t = cxpr_flux::arena_allocator<std::allocator<void>, 1024 * 8>;
	static constexpr int nObjects = 64;
	static constexpr int nIterations = 1000;

	destructorCounter = 0;

	{	// implicit deletion via destructor
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();
		for (int i = 0; i < nObjects; i++)
		{
			auto d = allocator->construct<destructor_test>("", i);
			EXPECT_EQ(d->i, i);
		}
	}
	EXPECT_EQ(destructorCounter, nObjects);
	destructorCounter = 0;
	{	// explicit deletion via purge
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();
		for (int i = 0; i < nObjects; i++)
		{
			auto d = allocator->construct<destructor_test>("", i);
			EXPECT_EQ(d->i, i);
		}
		allocator->purge();
		EXPECT_EQ(destructorCounter, nObjects);
	}

	destructorCounter = 0;
	{	// stress the allocator, alloc objects, purge, and repeat using the same allocator
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();

		for (int iter = 0; iter < nIterations; iter++)
		{
			std::vector<allocator_t::allocated_t<destructor_test>> v;
			for (int i = 0; i < nObjects; i++)
			{
				auto index_val = i + (iter * nObjects);
				v.push_back(allocator->construct<destructor_test>("", index_val));
				EXPECT_EQ(v.back()->i, index_val);
			}
			allocator->purge();
		}
		EXPECT_EQ(destructorCounter, nObjects* nIterations);
	}
}


//////////////////////////////////////////////////////////////////////////

TEST(arena_allocator_tests, saturate_test)
{
	using namespace __arena_tests;
	using allocator_t = cxpr_flux::arena_allocator<std::allocator<void>, 1024 * 8>;
	{	// saturate using type with a non-trivial destructor
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();
		// we should not overflow the allocator here, it should start chaining
		for (int i = 0; i < 100000; i++)
		{
			auto d = allocator->construct<destructor_test>("", i);
			EXPECT_EQ(d->i, i);
		}
	}

	{	// saturate using type with a trivial destructor
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();
		// we should not overflow the allocator here, it should start chaining
		for (int i = 0; i < 100000; i++)
		{
			const float val = static_cast<float>(i);
			auto d = allocator->construct<float>("", val);
			EXPECT_EQ(*d, val);
		}
	}
}

TEST(arena_allocator_tests, saturate_threaded_test)
{
	using namespace __arena_tests;
	using allocator_t = cxpr_flux::arena_allocator<std::allocator<void>, 1024 * 8>;
	{
		destructorCounter = 0;
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();
		std::thread t[16];
		std::atomic_int created{ 0 };
		const auto nJobs = 10000;
		// make 16 threads all sharing the allocator, each thread will create objects at the same time
		// Once it makes it's objects, the thread will go through and make sure the created data matches
		// the expected values to verify that data hasn't been stomped/overwritten
		for (int thread_idx = 0; thread_idx < 16; thread_idx++) {
			t[thread_idx] = std::thread([&allocator, &created, thread_idx, nJobs] {
				std::vector<destructor_test*> objs = {};
				std::vector<double*> trivials = {};
				objs.reserve(nJobs);
				// Alternate making trivial objects and objects with destructors
				for (int job = 0; job < nJobs; job++) {
					if (job % 4 == 0) {
						trivials.push_back(allocator->construct<double>("", static_cast<double>(thread_idx) * 16 + job));
						continue;
					}

					objs.push_back(allocator->construct<destructor_test>("", (thread_idx * 16) + job));
					std::this_thread::yield();
					created++;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				// check work (make sure nothing was stomped)
				int trivial_idx = 0;
				int job_idx = 0;
				for (int job = 0; job < nJobs; job++) {
					if (job % 4 == 0) {
						EXPECT_DOUBLE_EQ(*trivials[trivial_idx], static_cast<double>((thread_idx * 16) + job));
						trivial_idx++;
						continue;
					}

					EXPECT_EQ(objs[job_idx]->i, (thread_idx * 16) + job);
					job_idx++;
				}
			});
		}

		for (auto& tt : t) {
			if (tt.joinable()) {
				tt.join();
			}
		}

		// purge the original and verify the test destructor was called the correct number of times
		allocator->purge();
		EXPECT_EQ(destructorCounter, created);
	}
}

//////////////////////////////////////////////////////////////////////////

TEST(arena_allocator_tests, move_semantics_test)
{
	using namespace __arena_tests;
	using allocator_t = cxpr_flux::arena_allocator<std::allocator<void>, 1024 * 8>;
	static constexpr int nObjects = 64;

	destructorCounter = 0;
	{	// explicit deletion via purge
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();
		for (int i = 0; i < nObjects; i++)
		{
			auto d = allocator->construct<destructor_test>("", i);
			EXPECT_EQ(d->i, i);
		}

		// explicitly move the allocator
		auto moved = std::move(*allocator);
		// purge the original
		allocator->purge();
		// nothing should have changed
		EXPECT_EQ(destructorCounter, 0);
		// now purge the moved one 
		moved.purge();
		EXPECT_EQ(destructorCounter, nObjects);
	}
}