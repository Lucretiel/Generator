/*
 * Generator.h
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 */

#pragma once

#include <boost/context/all.hpp>
#include <boost/iterator/iterator_facade.hpp>

#include "ManagedStack.hpp"

namespace generator
{
namespace detail
{

}

template<
	class GeneratorFunc,
	class YieldType,
	class Stack=ManagedStack>
class Generator
{
private:
	//Internal typedefs and types
	typedef boost::context::fcontext_t context_type;
	typedef GeneratorFunc crtp_type;

	//Return states to give to the generator
	enum class YieldBack : intptr_t {Resume, Stop};

	//Return states to get from the generator
	enum class GeneratorState : intptr_t {Continue, Done};

	//thrown from yield() when the context must be immediately destroyed.
	class ImmediateStop {};

	//Iterator
	class GeneratorIterator :
		public boost::iterator_facade<
			GeneratorIterator, YieldType,
			boost::single_pass_traversal_tag>
	{
	public:
		typedef Generator generator_type;

	private:
		Generator* gen;

		friend class boost::iterator_core_access;

		void increment()
		{
			if(gen)
			{
				gen->advance();
				if(gen->stopped())
					gen = nullptr;
			}
		}

		YieldType& dereference() const
		{
			return *gen->get();
		}

		bool equal(const GeneratorIterator& rhs) const
		{
			return gen == rhs.gen;
		}

	public:
		GeneratorIterator(Generator* gen=nullptr):
			gen(gen)
		{}
	};

public:
	//public typedefs. YieldType, the associated pointer, and the iterator
	typedef YieldType yield_type;
	typedef YieldType* yield_ptr_type;
	typedef GeneratorIterator iterator;

	//GeneratorCoreAccess. Make this a friend to have a private run()
	class GeneratorCoreAccess
	{
	private:
		friend class Generator;
		static void run(crtp_type& func)
		{
			func.run();
		}
	};

private:
	//State!
	Stack inner_stack;
	YieldType* current_value;
	context_type* inner_context;
	context_type outer_context;

private:
	//Wrappers for launching the generator

	//initiator function to fit into make_fcontext
	static void static_run(intptr_t p)
	{
		reinterpret_cast<Generator*>(p)->begin_run();
	}

	/*
	 * Primary wrapper. Launches function, handles ImmediateStop exceptions,
	 * and cleans up on an exit.
	 */
	void begin_run()
	{
		try { GeneratorCoreAccess::run(crtp_reference()); }
		catch(ImmediateStop&) {}

		current_value = nullptr;
		exit_generator(GeneratorState::Done);
	}

private:
	//Yield implementation

	//core wrapper for jump_fcontext call out of the generator
	YieldBack jump_out_of_generator(GeneratorState state)
	{
		return static_cast<YieldBack>(
			boost::context::jump_fcontext(
				inner_context, &outer_context, static_cast<intptr_t>(
					state)));
	}

	//Jump out of a generator. If it receives a stop instruction, throw ImmediateStop.
	void exit_generator(GeneratorState state)
	{
		if(jump_out_of_generator(state) == YieldBack::Stop)
		{
			throw ImmediateStop();
		}
	}

protected:
	//Yields

	template<class T>
	void yield(T&& obj)
	{
		current_value = &obj;
		exit_generator(GeneratorState::Continue);
	}

	//TODO: yield_from(Generator) that manipulates the context switches

private:
	//Functions relating to managing the generator context (switching into it, etc)

	//core wrapper for jump_fcontext call into the generator
	GeneratorState jump_into_generator(intptr_t val)
	{
		return static_cast<GeneratorState>(
			boost::context::jump_fcontext(
				&outer_context, inner_context, val));
	}

	//Checked jump into generator
	void enter_generator(intptr_t val)
	{
		//If the context exists, enter it
		if(inner_context)
		{
			//If the generator finishes, clean it up
			if(jump_into_generator(val) == GeneratorState::Done)
			{
				clear_generator_context();
			}
		}
	}

	//Checked jump into the generator with a return state
	void resume_generator(YieldBack yield_back)
	{
		enter_generator(static_cast<intptr_t>(yield_back));
	}

	//Wipe the generator context. UNSAFE
	void clear_generator_context()
	{
		current_value = nullptr;
		inner_context = nullptr;
		inner_stack.clear();
	}
public:
	Generator(std::size_t stack_size = Stack::min_size):
		inner_stack(stack_size),
		current_value(nullptr),
		inner_context(boost::context::make_fcontext(
			inner_stack.stack(),
			inner_stack.size(),
			&Generator::static_run))
	{
		enter_generator(reinterpret_cast<intptr_t>(this));
	}

	~Generator()
	{
		stop();
	}

	Generator(const Generator&) =delete;
	Generator& operator=(const Generator&) =delete;

	Generator(Generator&&) =default;
	Generator& operator=(Generator&&) =default;

	GeneratorFunc& crtp_reference()
	{
		return *static_cast<crtp_type*>(this);
	}
	const GeneratorFunc& crtp_reference() const
	{
		return crtp_const_reference();
	}
	const GeneratorFunc& crtp_const_reference() const
	{
		return *static_cast<const crtp_type*>(this);
	}

	void advance()
	{
		resume_generator(YieldBack::Resume);
	}

	void stop()
	{
		resume_generator(YieldBack::Stop);
		clear_generator_context();
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
}; // class Generator
} // namespace generator
