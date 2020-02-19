#include <iostream>

#include "gtest/gtest.h"

#include <cxpr_flux.h>
#include "todo_classes.h"

using namespace cxpr;

//////////////////////////////////////////////////////////////////////////
// This test implements facebooks' basic todo flux example
// https://github.com/facebook/flux/tree/master/examples/flux-todomvc
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////

TEST(flux_tests, todo_test)
{
	using namespace cxpr_flux;
	using namespace todo_test;

	{		
		cxpr_flux::flux_static_context<std::allocator<void>, TodoStore> ctx;
		auto appContainer = cxpr_flux::create_container<AppContainer, AppView>(ctx);

		// Add a todo
		ctx.getDispatcher().signal(
			signals::addTodo{ "My first task" }
		);
		ctx.getDispatcher().signal(
			signals::addTodo{ "Another task" }
		);
		ctx.getDispatcher().signal(
			signals::addTodo{ "Finish this tutorial" }
		);

		ctx.processSignals();
		{	// 'Render' then toggle some tasks
			auto view = appContainer.Render();
			EXPECT_EQ(view.views.size(), 3);
			view.views[0].onToggle();
			view.views[1].onToggle();
			view.views[2].onToggle();
			// Everything should be false until we process
			EXPECT_FALSE(view.views[0].complete);
			EXPECT_EQ(view.views[0].text, "My first task");
			EXPECT_FALSE(view.views[1].complete);
			EXPECT_EQ(view.views[1].text, "Another task");
			EXPECT_FALSE(view.views[2].complete);
			EXPECT_EQ(view.views[2].text, "Finish this tutorial");
		}
		ctx.processSignals();
		{	// Re-render and check our work
			auto view = appContainer.Render();
			EXPECT_EQ(view.views.size(), 3);
			// Everything should be true now that we've toggled and processed
			EXPECT_TRUE(view.views[0].complete);
			EXPECT_TRUE(view.views[1].complete);
			EXPECT_TRUE(view.views[2].complete);
			view.views[0].onDelete();
		}
		ctx.processSignals();
		{	// Re-render and check our work
			auto view = appContainer.Render();
			EXPECT_EQ(view.views.size(), 2);
			// Deleted the first, should be 2 entries still toggled
			EXPECT_TRUE(view.views[0].complete);
			EXPECT_EQ(view.views[0].text, "Another task");
			EXPECT_TRUE(view.views[1].complete);
			EXPECT_EQ(view.views[1].text, "Finish this tutorial");
		}
		// Go crazy and add 100 todos
		for (int i = 0; i < 100; i++)
		{
			ctx.getDispatcher().signal(
				signals::addTodo{ std::string("New Signal ") + std::to_string(i) }
			);
		}
		ctx.processSignals();
		{	// Should have 102 tasks now... Busy busy
			auto view = appContainer.Render();
			EXPECT_EQ(view.views.size(), 102);
		}

 		ctx.processSignals();
		ctx.processSignals();
	}
}

//////////////////////////////////////////////////////////////////////////

TEST(flux_tests, todo_adv_test)
{

}

