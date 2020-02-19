#pragma once

#include <cxpr.h>
#include <cxpr_flux.h>


//////////////////////////////////////////////////////////////////////////
// The classes below implement facebooks' todo-example
// https://github.com/facebook/flux/tree/master/examples/flux-todomvc
//////////////////////////////////////////////////////////////////////////

namespace todo_test
{
	namespace signals
	{
		struct addTodo
		{
			std::string text;
		};

		struct deleteTodo
		{
			int id;
		};

		struct toggleTodo
		{
			int id;
		};
	}

	//////////////////////////////////////////////////////////////////////////
	// TodoStore.
	// Maintains a list of all todo's currently in the system. Single point of state
	// for todos
	struct TodoStore : public cxpr_flux::FluxStoreBase<TodoStore>
	{
		using cxpr_flux::FluxStoreBase<TodoStore>::FluxStoreBase;
		struct todoState
		{
			int id;
			bool complete;
			std::string text;
		};

		using state_t = std::vector<todoState>;

		static constexpr decltype(auto) GetCallbacks()
		{
			return std::make_tuple
			(
				cxpr_flux::make_callback<signals::addTodo>
				(
					[](TodoStore& self, const signals::addTodo& changes, auto& context)
					{
						self.newTodo(changes);
						self.EmitChanged();
						return true;
					}
				),
				cxpr_flux::make_callback<signals::deleteTodo>
				(
					[](TodoStore& self, const signals::deleteTodo& changes, auto& context)
					{
						self.deleteTodo(changes);
						self.EmitChanged();
						return true;
					}
				),
				cxpr_flux::make_callback<signals::toggleTodo>
				(
					[](TodoStore& self, const signals::toggleTodo& changes, auto& context)
					{
						self.toggleTodo(changes);
						return true;
					}
				)
			);
		}

		state_t getState() const { return todos; }

	private:
		void newTodo(const signals::addTodo& changes)
		{
			todoState newState = {};
			newState.text = changes.text;
			newState.complete = false;
			newState.id = counter++;
			todos.push_back(std::move(newState));
			EmitChanged();
		}

		void deleteTodo(const signals::deleteTodo& changes)
		{
			todos.erase(std::find_if(std::begin(todos), std::end(todos), [&](const auto& it) { return it.id == changes.id; }));
			EmitChanged();
		}

		void toggleTodo(const signals::toggleTodo& changes)
		{
			auto found = std::find_if(std::begin(todos), std::end(todos), [&](const auto& it) { return it.id == changes.id; });
			if (found != std::end(todos))
			{
				found->complete = !found->complete;

			}
			EmitChanged();
		}

		int counter = 0;
		state_t todos;
	};

	//////////////////////////////////////////////////////////////////////////
	// Current state of the system as well as type-erased callbacks needed to interact with the context
	struct ContainerState
	{
		std::vector<TodoStore::todoState> states;
		cxpr_flux::flux_callback<void(int)> onToggle;
		cxpr_flux::flux_callback<void(int)> onDelete;
	};

	//////////////////////////////////////////////////////////////////////////
	// 'Renderable' part of the app. We obviously can't render to the screen in this simple
	// of a test so just maintain a vector state
	struct AppView
	{
		struct viewData
		{
			int id;
			bool complete;
			std::string text;
			cxpr_flux::flux_callback<void()> onToggle;
			cxpr_flux::flux_callback<void()> onDelete;
		};

		template <typename containter_state_t>
		AppView(const containter_state_t& inState)
		{
			onToggle = inState.onToggle;
			onDelete = inState.onDelete;

			const std::vector<TodoStore::todoState>& stateData = inState.states;
			for (const auto& it : stateData)
			{
				viewData view = {};
				view.complete = it.complete;
				view.text = it.text;
				view.onToggle.bind_lambda([id = it.id, this]{ this->onToggle(id); });
				view.onDelete.bind_lambda([id = it.id, this]{ this->onDelete(id); });
				views.emplace_back(std::move(view));
			}
		}

		std::vector<viewData> views;

		cxpr_flux::flux_callback<void(int)> onToggle;
		cxpr_flux::flux_callback<void(int)> onDelete;
	};

	//////////////////////////////////////////////////////////////////////////
	// Container for the system, binds the view and the context together and provides type-erasure through getState()
	// Also can generate a new 'render' state based on the current state of the stores
	template <typename context_t, typename view_t>
	struct AppContainer : public cxpr_flux::flux_container<TodoStore>
	{
		AppContainer(context_t& _ctx) : context(_ctx) 
		{
			bind(context);
		}

		ContainerState GetState()
		{
			ContainerState created = {};
			created.onToggle.bind_lambda([&](int id)
			{
				context.getDispatcher().signal(
					signals::toggleTodo{ id }
				);
			});

			created.onDelete.bind_lambda([&](int id)
			{
				context.getDispatcher().signal(
					signals::deleteTodo{ id }
				);
			});

			created.states = getState<TodoStore>();
			return created;
		}

		view_t Render()
		{
			return view_t(GetState());
		}

	private:
		context_t& context;
	};
}