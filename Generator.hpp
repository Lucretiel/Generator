/*
 * Generator.h
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 */

#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <boost/context/all.hpp>

//TODO: Fix the OH GOD IT'S ALL ONE FILE thing
//TODO: No seriously we need some separation of responsibility up in this bitch
//TODO: add namespace
//TODO: Maybe a few less casts

#include "ManagedStack.hpp"
#include "GeneratorIterator.hpp"

template<
	class GeneratorFunc,
	class YieldType,
	class Stack=ManagedStack<char> >
class Generator
{
public:
	//public typedefs. YieldType and the associated pointer.
	typedef YieldType yield_type;
	typedef YieldType* yield_ptr_type;
	typedef GeneratorIterator<Generator> iterator;

private:
	//Internal typedefs
	typedef boost::context::fcontext_t context_type;

private:
	//Return states from a next into a yield
	enum class YieldBack : intptr_t {Resume, Stop};

	//thrown from yield() when the context must be immediately destroyed.
	class ImmediateStop {};

private:
	//State!
	YieldType* current_value;
	Stack inner_stack;
	context_type* inner_context;
	context_type outer_context;

private:
	//Wrappers for launching the generator

	//initiator function to fit into make_fcontext
	static void static_run(intptr_t p)
	{
		context_type* outer = reinterpret_cast<Generator*>(p)->begin_run();
		context_type inner;

		boost::context::jump_fcontext(&inner, outer, 0);
	}

	/*
	 * Primary wrapper. Launches context, handles ImmediateStop exceptions,
	 * and cleans up on an exit. Returns a context to switch back to, on completion,
	 * to static_run
	 */
	context_type* begin_run()
	{
		try { crtp_reference().run(); }
		catch(ImmediateStop&) {}

		//Point of no return
		current_value = nullptr;
		inner_context = nullptr;

		return &outer_context;
	}

private:
	//Yield implementation

	//Jump out of a generator
	void exit_generator(YieldType* yield_ptr)
	{
		current_value = yield_ptr;
		if(static_cast<YieldBack>(
			boost::context::jump_fcontext(
				inner_context, &outer_context, 0)) == YieldBack::Stop)
		{
			throw ImmediateStop();
		}
	}
protected:
	//Yields

	template<class T>
	void yield(T&& obj)
	{
		exit_generator(&obj);
	}

private:
	//Functions relating to managing the generator context (switching into it, etc)

	void enter_generator(intptr_t val)
	{
		boost::context::jump_fcontext(&outer_context, inner_context, val);
	}
	void resume_generator(YieldBack yield_back)
	{
		if(inner_context)
			enter_generator(static_cast<intptr_t>(yield_back));
		if(!inner_context && inner_stack.stack())
			inner_stack.clear();
	}

public:
	Generator(unsigned stack_size):
		current_value(nullptr),
		inner_stack(stack_size),
		inner_context(boost::context::make_fcontext(
			inner_stack.stack(),
			stack_size,
			&Generator::static_run))
	{
		//Take no chances with reinterpret cast
		Generator* self(this);
		enter_generator(reinterpret_cast<intptr_t>(self));
	}

	virtual ~Generator()
	{
		stop();
	}

	Generator(const Generator&) =delete;
	Generator& operator=(const Generator&) =delete;
	Generator(Generator&&) =delete;
	Generator& operator=(Generator&&) =delete;

	GeneratorFunc& crtp_reference()
	{
		return *static_cast<GeneratorFunc*>(this);
	}
	const GeneratorFunc& crtp_reference() const
	{
		return crtp_const_reference();
	}
	const GeneratorFunc& crtp_const_reference() const
	{
		return *static_cast<const GeneratorFunc*>(this);
	}

	void advance()
	{
		resume_generator(YieldBack::Resume);
	}

	void stop()
	{
		resume_generator(YieldBack::Stop);
	}

	YieldType* get()
	{
		return current_value;
	}

	const YieldType* get() const
	{
		return cget();
	}

	const YieldType* cget() const
	{
		return current_value;
	}

	bool stopped() const
	{
		return inner_context == nullptr;
	}

	iterator begin()
	{
		return iterator(this);
	}

	iterator end()
	{
		return iterator();
	}
};

#endif /* GENERATOR_H_ */
