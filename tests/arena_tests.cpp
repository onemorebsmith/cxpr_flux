#include <iostream>

#include "gtest/gtest.h"

#include <cxpr_flux.h>

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
	using allocator_t = cxpr_flux::arena_allocator<1024 * 8>;
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
	using allocator_t = cxpr_flux::arena_allocator<1024 * 8>;
	{	// saturate using type with a non-trivial destructor
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();
		try
		{
			// we should overflow the allocator here
			for (int i = 0; i < 100000; i++)
			{
				auto d = allocator->construct<destructor_test>("", i);
				EXPECT_EQ(d->i, i);
			}

			FAIL() << "arena_allocator didn't throw as expected ";
		}
		catch (const std::bad_alloc&)
		{
			SUCCEED();
		}
	}

	{	// saturate using type with a trivial destructor
		std::unique_ptr<allocator_t> allocator = std::make_unique<allocator_t>();
		try
		{
			// we should overflow the allocator here
			for (int i = 0; i < 100000; i++)
			{
				const float val = static_cast<float>(i);
				auto d = allocator->construct<float>("", val);
				EXPECT_EQ(*d, val);
			}

			FAIL() << "arena_allocator didn't throw as expected ";
		}
		catch (const std::bad_alloc&)
		{
			SUCCEED();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

TEST(arena_allocator_tests, move_semantics_test)
{
	using namespace __arena_tests;
	using allocator_t = cxpr_flux::arena_allocator<1024 * 8>;
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