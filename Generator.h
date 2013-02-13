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
#include <memory>
#include <boost/context/all.hpp>
#include <boost/optional.hpp>

#include "GeneratorInterface.h"
#include "ManagedStack.h"

//TODO: Fix the OH GOD IT'S ALL ONE FILE thing
//TODO: No seriously we need some separation of responsibility up in this bitch
//TODO: add namespace
//TODO: Exception-free or partially exception-free implementation.
//TODO: Maybe a few less casts
//TODO: Change the YieldBack passing style to a local member or something

const unsigned default_stack_size = 1024 * 4;

//Public Exceptions

template<class YieldType>
class Generator : public GeneratorInterface<YieldType>
{
public:
	//Have to explicitly inherit typedefs when using in definitions
	typedef typename Generator::yield_type yield_type;
	typedef typename Generator::generator_finished generator_finished;
	typedef typename Generator::yield_ptr_type yield_ptr_type;

private:
	//Return states from a yield, into a next call
	enum class YieldResult : intptr_t {Object, Return};

	//Return states from a next into a yield
	enum class YieldBack : intptr_t {Resume, Stop};

	class ImmediateStop {}; //For throwing out of yield()

	typedef boost::optional<yield_type> YieldTypeStorage;

private:
	ManagedStack<boost::context::guarded_stack_allocator> inner_stack;
	boost::context::fcontext_t* inner_context;
	boost::context::fcontext_t outer_context;
	YieldTypeStorage yield_value;
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
		catch(ImmediateStop&) {}
		catch(GeneratorFinished&) {}

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
			//TODO: add a return check here
			yield_back_internal(YieldBack::Stop);
			clear_internal_context();
		}
	}

	void clear_internal_context()
	{
		inner_context = nullptr;
		inner_stack.clear();
		yield_value = boost::none;
	}

protected:
	void yield(const yield_type& obj)
	{
		yield_value = obj;
		yield_internal();
	}

	void yield(yield_type&& obj)
	{
		yield_value = std::move(obj);
		yield_internal();
	}

	//TODO: exception-free yield_from
	void yield_from(GeneratorInterface<yield_type>& gen)
	{
		try
		{
			while(true)
				yield(gen.next());
		}
		catch(generator_finished& e)
		{}
	}

	void yield_from(GeneratorInterface<yield_type>&& gen)
	{
		yield_from(gen);
	}

public:
	Generator(unsigned stack_size = default_stack_size):
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

		return *this;
	}

	yield_type next() override
	{
		if(!inner_context)
			throw GeneratorFinished();

		YieldResult yield_result = yield_back_internal();

		/*
		 * Could use a switch here, but we need block-level stack allocation,
		 * and this way is cleaner to read
		 */

		if(yield_result == YieldResult::Object)
		{
			yield_type obj(std::move(yield_value.get()));

			yield_value = boost::none;
			return obj;
		}
		else if(yield_result == YieldResult::Return)
		{
			clear_internal_context();
			throw GeneratorFinished();
		}

		throw std::logic_error("Error: Got an invalid type back from yield_back_iternal");
	}

	yield_ptr_type next_ptr() override
	{
		if(!inner_context)
			return nullptr;

		YieldResult yield_result = yield_back_internal();

		if(yield_result == YieldResult::Object)
		{
			yield_ptr_type ret(new yield_type(std::move(yield_value.get())));
			yield_value = boost::none;
			return ret;
		}
		else if(yield_result == YieldResult::Return)
		{
			clear_internal_context();
		}
		return nullptr;
	}

	//TODO: find a way to reduce code repetition.
		/*
		 * The trouble is that next() involves exceptions, while next_ptr
		 * involves dynamic allocation.
		 */
};

#endif /* GENERATOR_H_ */
