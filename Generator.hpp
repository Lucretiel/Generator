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
	//public typedefs. YieldType and the associated pointer.
	typedef typename Generator::yield_type yield_type;
	typedef yield_type* yield_ptr_type;

private:
	//Internal storage typedefs
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
	yield_ptr_type yield_value;
	bool started;

private:
	//Wrappers for launching the generator

	//initiator function to fit into make_fcontext
	static void static_run(intptr_t p)
	{
		context* outer = reinterpret_cast<Generator*>(p)->begin_run();
		context* inner;

		boost::jump_fcontext(inner, outer, 0);
		//TODO: determine what happens if this simply returns
	}

	/*
	 * Primary wrapper. Launches context, handles ImmediateStop exceptions,
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

private:
	//Yield implementation

	//Jump out of a generator
	void yield_jump()
	{
		if(static_cast<YieldBack>(
			boost::context::jump_fcontext(
				inner_context, &outer_context, 0)) == YieldBack::Stop)
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
		yield_value = &obj;
		yield_jump();
	}

	//Yield from a generator. Only returns when gen is finished.
	void yield_from(GeneratorInterface<yield_type>& gen)
	{
		while(yield_type* obj = gen.next())
		{
			yield(*obj);
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
			yield(item);
		}
	}

private:
	//Functions relating to managing the generator context (switching into it, etc)

	//Executes a context switch into the generator
	void yield_back_jump(intptr_t x)
	{
		boost::context::jump_fcontext(&outer_context, inner_context, x);
	}

	/*
	 * Handler for switching into the generator. Handles sending in yield_back,
	 * casts, etc. Returns pointer to the yielded object.
	 */
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
		yield_value = nullptr;
	}

public:
	Generator(unsigned stack_size = default_stack_size):
		inner_stack(stack_size),
		inner_context(nullptr),
		yield_value(nullptr),
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
		yield_value(mve.yield_value),
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
		yield_value = mve.yield_value;
		started = mve.started;

		mve.clear_internal_context();

		return *this;
	}

	/*
	 * Get the next value from the generator. Returns nullptr when the
	 * generator is finished. The returned pointer points to an object managed
	 * in some fashion by generator- do not move it or hold it. The pointed-to
	 * value is literally the object passed into yield, so two way communication
	 * with the generator function is possible by manipulating that value.
	 */
	yield_type* next() override
	{
		if(!inner_context)
			return nullptr;

		yield_value = nullptr;
		yield_back_internal();

		if(!yield_value) cleanup();
		return yield_value;
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
