/*
 * Generator.h
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 */

#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <utility>
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

template<class YieldType, class Allocator = boost::context::guarded_stack_allocator>
class Generator : public GeneratorInterface<YieldType>
{
public:
	typedef typename Generator::yield_type yield_type;
	typedef yield_type* yield_ptr_type;

private:
	//Internal storage and state types
	//Storage for either a yield_type or pointer to a yield_type
	typedef boost::variant<
			yield_ptr_type,
			yield_type> YieldTypeStorage;

	typedef ManagedStack<Allocator> Stack;
	typedef boost::context::fcontext_t context_type;

private:
	//Return states from a next into a yield
	enum class YieldBack : intptr_t {Resume, Stop};

	//thrown from yield() when the context must be immediately destroyed.
	class ImmediateStop {};

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
	Stack inner_stack;
	context_type* inner_context;
	context_type outer_context;
	YieldTypeStorage yield_value;
	bool started;

private:
	//Wrappers for launching the coroutine

	//initiator function to fit into make_fcontext
	static void static_run(intptr_t p)
	{
		context* outer = reinterpret_cast<Generator*>(p)->begin_run();
		context* inner;

		boost::jump_fcontext(inner, outer, 0);
		//TODO: determine what happens if this simply returns
	}

	/*
	 * Primary wrapper. Launches coroutine, handles ImmediateStop exceptions,
	 * and cleans up on an exit
	 */
	context_type* begin_run()
	{
		started = true;

		try { run(); }
		catch(ImmediateStop&) {}

		yield_value = nullptr;

		//Point of no return
		inner_context = nullptr;

		return &outer_context;
	}

	//The actual run implementation. This is the one you care about, clients!
	virtual void run() =0;
	//TODO: consider using a function pointer or CRTP instead of a virtual

protected:
	//Yields

	/*
	 * Primary yield function. T should be convertible to yield_type or
	 * yield_ptr_type. Move aware.
	 */
	template<class T>
	void yield(T&& obj)
	{
		yield_value = std::forward<T>(obj);

		auto result = static_cast<YieldBack>(
				boost::context::jump_fcontext(
						inner_context, &outer_context, 0));

		if(result == YieldBack::Stop)
			throw ImmediateStop();
	}

	//Yield from a generator. Only returns when gen is finished.
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

	//yield from some other iterable
	template<class T>
	void yield_from(T&& obj)
	{
		for(auto& item : obj)
		{
			yield(&item);
		}
	}

private:
	//Functions relating to managing the coroutine (switching into it, etc)

	//Executes a context switch into the generator
	void yield_back_jump(intptr_t x)
	{
		boost::context::jump_fcontext(&outer_context, inner_context, x);
	}

	/*
	 * Handler for switching into the generator. Handles sending in yield_back,
	 * casts, etc. Returns pointer to the yielded object.
	 */
	yield_ptr_type yield_back_internal(YieldBack yield_back = YieldBack::Resume)
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
		return boost::apply_visitor(yield_value_getter(), yield_value);
	}

	//End the coroutine. This is the safe one.
	void cleanup()
	{
		if(inner_context)
		{
			yield_back_internal(YieldBack::Stop);
			clear_internal_context();
		}
	}

	//Destroy the coroutine. This is the unsafe one. Use cleanup() instead.
	void clear_internal_context()
	{
		inner_context = nullptr;
		inner_stack.clear();
		yield_value = nullptr;
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
	 * Note that move constructing or assigning a generator MAY invalidate any
	 * return value pointers from next(), because the pointed-to value has
	 * moved to the new generator. If a pointer was yielded from the coroutine,
	 * rather than a full object, it should be safe
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

	/*
	 * Get the next value from the generator. Returns nullptr when the
	 * generator is finished. The returned pointer points to a value that is
	 * handled by Generator- do not delete or hold it. This pointer is
	 * invalidated on the next call to next().
	 */
	yield_type* next() override
	{
		if(!inner_context)
			return nullptr;

		yield_value = nullptr;
		yield_ptr_type ret(yield_back_internal());

		if(!ret) cleanup();
		return ret;
	}
};

#define GENERATOR(NAME, RETURN_TYPE) \
	class NAME : public Generator<RETURN_TYPE> \
	{ \
	private: \
		void run() override; \
	}; \
	inline void NAME::run()

#define YIELD this->yield

#endif /* GENERATOR_H_ */
