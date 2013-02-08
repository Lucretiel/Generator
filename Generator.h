/*
 * Generator.h
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 */

#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <exception>
#include <utility>
#include <boost/context/all.hpp>
#include <boost/optional.hpp>

#include "ManagedStack.h"

//TODO: Fix the OH GOD IT'S ALL ONE FILE thing
//TODO: No seriously we need some separation of responsibility up in this bitch
//TODO: add namespace
//TODO: Exception-free or partially exception-free implementation.
//TODO: Move, copy, and assignment operations

//Public Exceptions
class GeneratorFinished {};
class NotImplemented {};

template<class YieldType, unsigned stack_size = 1024 * 4>
class Generator
{
private:
	//Return states from a yield, into a next call
	enum class YieldResult : intptr_t {Object, Return};

	//Return states from a next into a yield
	enum class YieldBack : intptr_t {Resume, Stop};

	class ImmediateStop {};

	ManagedStack<boost::context::guarded_stack_allocator> inner_stack;
	boost::context::fcontext_t* inner_context;
	boost::context::fcontext_t outer_context;
	boost::optional<YieldType> yield_value;
	bool started;

	//initiator function to fit into make_fcontext
	static void static_run(intptr_t p)
	{
		reinterpret_cast<Generator*>(p)->begin_run();
	}

	/*
	 * Wrapper for the virtual run. Handles exceptions and outermost context
	 * switching
	 */
	void begin_run()
	{
		started = true;

		try { run(); }
		catch(ImmediateStop& stop) {}

		//While true here in case this context is resumed after returning
		while(true)
		{
			boost::context::jump_fcontext(
					inner_context,
					&outer_context,
					static_cast<intptr_t>(YieldResult::Return));
		}
	}

	virtual void run() =0;

	//Handles the actual context switch. Object passing is handled elsewhere
	void yield_internal()
	{
		auto result = static_cast<YieldBack>(boost::context::jump_fcontext(
				inner_context,
				&outer_context,
				static_cast<intptr_t>(YieldResult::Object)));

		if(result == YieldBack::Stop)
			throw ImmediateStop();
	}

	YieldResult yield_back_internal(YieldBack yield_back = YieldBack::Resume)
	{
		return static_cast<YieldResult>(boost::context::jump_fcontext(
				&outer_context,
				inner_context,
				started ?
						static_cast<intptr_t>(yield_back) :
						reinterpret_cast<intptr_t>(this)));
	}

	void cleanup()
	{
		if(inner_context)
		{
			yield_back_internal(YieldBack::Stop);
			clear_internal_context();
		}
	}

	void clear_internal_context()
	{
		inner_context = nullptr;
		inner_stack.clear();
	}

protected:
	void yield(const YieldType& obj)
	{
		yield_value = obj;
		yield_internal();
	}
	void yield(YieldType&& obj)
	{
		yield_value = std::move(obj);
		yield_internal();
	}

public:
	Generator():
		inner_stack(stack_size),
		inner_context(nullptr),
		started(false)
	{
		inner_context = boost::context::make_fcontext(
				inner_stack.stack_pointer(),
				stack_size,
				Generator::static_run);
	}
	virtual ~Generator()
	{
		cleanup();
	}

	Generator(const Generator&) =delete;
	Generator& operator=(const Generator&) =delete;

	Generator(Generator&& mve):
		inner_stack(std::move(mve.inner_stack)),
		inner_context(mve.inner_context),
		outer_context(mve.outer_context),
		started(mve.started)
	{
		mve.clear_internal_context();
	}
	Generator& operator=(Generator&& mve)
	{
		cleanup();

		inner_stack = std::move(mve.inner_stack);
		inner_context = mve.inner_context;
		outer_context = mve.outer_context;
		started = mve.started;

		mve.clear_internal_context();
	}

	YieldType next()
	{
		if(!inner_context)
			throw GeneratorFinished();

		auto yield_back = yield_back_internal();

		/*
		 * Could use a switch here, but we need block-level stack allocation,
		 * and this way is cleaner to read
		 */

		if(yield_back == YieldResult::Object)
		{
			YieldType obj(std::move(yield_value.get()));
			yield_value = boost::none;
			return obj;
		}
		else if(yield_back == YieldResult::Return)
		{
			clear_internal_context();
			throw GeneratorFinished();
		}
		else
			throw std::logic_error("Invalid YieldResult returned from yield_back_internal");
	}

	//Some stuff for the container adapter
	typedef YieldType value_type;
};


#endif /* GENERATOR_H_ */
