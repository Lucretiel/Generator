/*
 * Generator.h
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 */

#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <utility>
#include <type_traits>
#include <boost/context/all.hpp>

#include "ManagedStack.hpp"

//TODO: Fix the OH GOD IT'S ALL ONE FILE thing
//TODO: No seriously we need some separation of responsibility up in this bitch
//      Specifically, split yielding and nexting into different classes.
//TODO: add namespace
//TODO: Maybe a few less casts

//The actual stack size will be this or minimum_stacksize, whichever is larger
const unsigned default_stack_size = 256;

template<
	class GeneratorFunc,
	class YieldType,
	class Allocator = boost::context::guarded_stack_allocator>
class Generator
{
public:
	//public typedefs. YieldType and the associated pointer.
	typedef YieldType yield_type;
	typedef YieldType* yield_ptr_type;
	typedef GeneratorFunc generator_func_type;

private:
	//Internal typedefs
	typedef ManagedStack<Allocator> Stack;
	typedef boost::context::fcontext_t context_type;

private:
	//Return states from a next into a yield
	enum class YieldBack : intptr_t {Resume, Stop};

	//thrown from yield() when the context must be immediately destroyed.
	class ImmediateStop {};

private:
	//State!
	Stack inner_stack;
	context_type* inner_context;
	context_type outer_context;
	bool started;

private:
	//Wrappers for launching the generator

	//initiator function to fit into make_fcontext
	static void static_run(intptr_t p)
	{
		context_type* outer = reinterpret_cast<Generator*>(p)->begin_run();
		context_type inner;
		yield_ptr_type val = nullptr;

		boost::context::jump_fcontext(&inner, outer,
				reinterpret_cast<intptr_t>(val));
		//TODO: determine what happens if this simply returns
	}

	/*
	 * Primary wrapper. Launches context, handles ImmediateStop exceptions,
	 * and cleans up on an exit
	 */
	context_type* begin_run()
	{
		started = true;

		try { crtp_reference().run(); }
		catch(ImmediateStop&) {}

		//Point of no return
		inner_context = nullptr;

		return &outer_context;
	}

private:
	//Yield implementation

	//Jump out of a generator
	void yield_jump(yield_ptr_type val)
	{
		if(static_cast<YieldBack>(
					boost::context::jump_fcontext(inner_context, &outer_context,
							reinterpret_cast<intptr_t>(val))) == YieldBack::Stop)
		{
			throw ImmediateStop();
		}
	}
protected:
	//Yields

	/*
	 * Primary yield function. Yields the address of the object passed. This is
	 * safe to use in most contexts, because even if an rvalue is passed, it
	 * will have a lifespan in the yield call, and therefore an address.
	 */
	template<class T>
	void yield(T&& obj)
	{
		yield_ptr_type yield_value = &obj;
		yield_jump(yield_value);
	}

	//yield from some iterable
	template<class T>
	void yield_from(T&& obj)
	{
		for(auto&& item : obj)
			yield(item);
	}

private:
	//Functions relating to managing the generator context (switching into it, etc)

	//Executes a context switch into the generator
	yield_ptr_type yield_back_jump(intptr_t val)
	{
		return reinterpret_cast<yield_ptr_type>(
			boost::context::jump_fcontext(
				&outer_context, inner_context, val));
	}

	/*
	 * Handler for switching into the generator. Handles sending in yield_back,
	 * casts, etc. Returns pointer to the yielded object.
	 */
	yield_ptr_type yield_back_internal(YieldBack yield_back = YieldBack::Resume)
	{
		if(started)
		{
			return yield_back_jump(static_cast<intptr_t>(yield_back));
		}
		else
		{
			if(yield_back == YieldBack::Resume)
			{
				//Take no chances with reinterpret cast
				Generator* self = this;
				return yield_back_jump(reinterpret_cast<intptr_t>(self));
			}
			else
				return nullptr;
		}
	}

	//End the generator. This is the safe one.
	void cleanup()
	{
		if(inner_context)
		{
			yield_back_internal(YieldBack::Stop);
			clear_internal_context();
		}
	}

	//Destroy the generator. This is the unsafe one. Use cleanup() instead.
	void clear_internal_context()
	{
		inner_context = nullptr;
		inner_stack.clear();
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
				&Generator::static_run);
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

	generator_func_type& crtp_reference()
	{
		return static_cast<generator_func_type&>(*this);
	}
	const generator_func_type& crtp_reference() const
	{
		return static_cast<const generator_func_type&>(*this);
	}
	const generator_func_type& crtp_const_reference() const
	{
		return static_cast<const generator_func_type&>(*this);
	}

	/*
	 * Get the next value from the generator. Returns nullptr when the
	 * generator is finished. The returned pointer points to an object managed
	 * in some fashion by generator- do not move it or hold it. The pointed-to
	 * value is literally the object passed into yield, so two way communication
	 * with the generator function is possible by manipulating that value.
	 */
	yield_ptr_type next()
	{
		if(!inner_context)
			return nullptr;

		yield_ptr_type yielded = yield_back_internal();

		if(!yielded) cleanup();
		return yielded;
	}
};

#endif /* GENERATOR_H_ */
