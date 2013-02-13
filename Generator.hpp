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

#include "GeneratorInterface.hpp"
#include "ManagedStack.hpp"

//TODO: Fix the OH GOD IT'S ALL ONE FILE thing
//TODO: No seriously we need some separation of responsibility up in this bitch
//      Specifically, split yielding and nexting into different classes.
//TODO: add namespace
//TODO: Maybe a few less casts

//The actual stack size will be this or minimum_stacksize, whichever is larger
const unsigned default_stack_size = 256;

template<class YieldType>
class Generator : public GeneratorInterface<YieldType>
{
public:
	typedef typename Generator::yield_type yield_type;
	typedef yield_type* yield_ptr_type;

private:
	//Return states from a next into a yield
	enum class YieldBack : intptr_t {Resume, Stop};

	//thrown from yield() when the context must be immediately destroyed.
	class ImmediateStop {}; //For throwing out of yield()

	//Storage for either a yield_type or pointer to a yield_type
	typedef boost::variant<
			yield_ptr_type,
			yield_type> YieldTypeStorage;

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
		yield_value = nullptr;

		//While true here in case this context is resumed after returning
		while(true)
		{
			boost::context::jump_fcontext(inner_context, &outer_context, 0);
		}
	}

	virtual void run() =0;

	//Handles the actual context switch. Object passing is handled elsewhere
	void yield_internal()
	{
		auto result = static_cast<YieldBack>(
				boost::context::jump_fcontext(
						inner_context, &outer_context, 0));

		if(result == YieldBack::Stop)
			throw ImmediateStop();
	}

	void yield_back_jump(intptr_t x)
	{
		boost::context::jump_fcontext(&outer_context, inner_context, x);
	}

	void yield_back_internal(YieldBack yield_back = YieldBack::Resume)
	{
		if(started)
		{
			yield_back_jump(static_cast<intptr_t>(yield_back));
		}
		else
		{
			if(yield_back == YieldBack::Resume)
				yield_back_jump(reinterpret_cast<intptr_t>(this));
		}
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
		yield_value = nullptr;
	}

protected:
	//TODO: investigate template implementation of yield
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

	//TODO: investigate corner case with yielding nullptr
	void yield(yield_type* ptr)
	{
		yield_value = ptr;
		yield_internal();
	}

	void yield_from(GeneratorInterface<yield_type>& gen)
	{
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

		yield_value = nullptr;
		yield_back_internal();

		auto ret = boost::apply_visitor(yield_value_getter(), yield_value);
		if(!ret) cleanup();
		return ret;
	}
};

#endif /* GENERATOR_H_ */
