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
#include <boost/variant.hpp>

#include "GeneratorInterface.h"
#include "ManagedStack.h"

//TODO: Fix the OH GOD IT'S ALL ONE FILE thing
//TODO: No seriously we need some separation of responsibility up in this bitch
//TODO: add namespace
//TODO: Exception-free or partially exception-free implementation.
//TODO: Maybe a few less casts
//TODO: Change the YieldBack passing style to a local member or something

//The actual stack size will be this or minimum_stacksize, whichever is larger
const unsigned default_stack_size = 256;

//Public Exceptions

template<class YieldType>
class Generator : public GeneratorInterface<YieldType>
{
public:
	//Have to explicitly inherit typedefs when using in definitions
	typedef typename Generator::yield_type yield_type;
	typedef yield_type* yield_ptr_type;

private:
	//Return states from a yield, into a next call
	enum class YieldResult : intptr_t {Object, Return};

	//Return states from a next into a yield
	enum class YieldBack : intptr_t {Resume, Stop};

	//thrown from yield() when the context must be immediatly destroyed.
	class ImmediateStop {}; //For throwing out of yield()

	//Storage for either a yield_type or pointer to a yield_type
	typedef boost::variant<
			boost::blank,
			yield_type,
			yield_ptr_type> YieldTypeStorage;

	struct yield_value_getter : public boost::static_visitor<yield_ptr_type>
	{
		yield_ptr_type operator()(yield_type& obj) const
		{
			return &obj;
		}
		yield_ptr_type operator()(yield_ptr_type p) const
		{
			return p;
		}
		yield_ptr_type operator()(boost::blank) const
		{
			return nullptr;
		}
	};

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
		yield_value = boost::blank();
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

	void yield(yield_type* ptr)
	{
		yield_value = ptr;
		yield_internal();
	}

	void yield_from(GeneratorInterface<yield_type>& gen)
	{
		//Yes, this is supposed to be an assignment, not comparison
		while(yield_type* obj = gen.next())
		{
			yield(obj);
		}
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

	/*
	 * Note that move constructing or assigning a generator will invalidate any
	 * return value pointers from next(), because the pointed-to value has
	 * moved to the new generator.
	 */
	Generator(Generator&& mve):
		inner_stack(std::move(mve.inner_stack)),
		inner_context(mve.inner_context),
		outer_context(mve.outer_context),
		yield_value(std::move(mve.yield_value)),
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
		yield_value = std::move(mve.yield_value);
		started = mve.started;

		mve.clear_internal_context();

		return *this;
	}

	yield_type* next() override
	{
		if(!inner_context)
			return nullptr;

		yield_value = boost::blank();
		YieldResult yield_result = yield_back_internal();

		/*
		 * Could use a switch here, but we need block-level stack allocation,
		 * and this way is cleaner to read
		 */

		if(yield_result == YieldResult::Object)
		{
			return boost::apply_visitor(yield_value_getter(), yield_value);
		}
		else if(yield_result == YieldResult::Return)
		{
			clear_internal_context();
			return nullptr;
		}

		//If yield_result, isn't one of the YieldResult types, FUCK YOU
		//TODO: maybe make this a little nicer
		std::terminate();
	}
};

#endif /* GENERATOR_H_ */
