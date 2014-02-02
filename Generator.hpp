/*
 * Generator.hpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  TODO: put something here
 *  TODO: Documentation, examples (besides main.cpp demo)
 */

#pragma once

#include <cstdlib>
#include <stdexcept>
#include <utility>

#include <boost/context/all.hpp>
#include <boost/iterator/iterator_facade.hpp>

namespace generator
{
namespace detail
{

/*
 * Simple class to add RAII semantics to stack allocation.
 *
 * Implementation note: currently, this callocates a block, then returns a
 * pointer to the other side of the block, because stacks grow down. In the
 * future this will be done in more platform-specific ways- page allocation,
 * guards, etc.
 */
class ScopedStack
{
private:
	std::size_t _size;
	char* _stack;

public:
	//TODO: pick this value more intelligently
	const static std::size_t min_size = 1024 * 8; //8KB, 2 pages on my system

	//TODO: allocate at page boundary
	//TODO: guard page
	//TODO: platform specific allocation, to ensure the stack growth is correct
	//See mprotect(2), sysconf(2), mmap(2)
	explicit ScopedStack(std::size_t size = 0):
		_size(size > min_size ? size : min_size),
		_stack(static_cast<char*>(std::calloc(_size, sizeof(char))))
	{
		if(!_stack) throw std::bad_alloc();
	}

	~ScopedStack()
	{
		clear();
	}

	//No copying
	ScopedStack(const ScopedStack&) =delete;
	ScopedStack& operator=(const ScopedStack&) =delete;

	//Moving
	ScopedStack(ScopedStack&& mve):
		_size(mve._size),
		_stack(mve._stack)
	{
		mve._stack = nullptr;
	}
	ScopedStack& operator=(ScopedStack&& mve)
	{
		if(&mve == this) return *this;

		clear();
		_size = mve._size;
		_stack = mve._stack;

		mve._stack = nullptr;
		return *this;
	}

	//Clear the stack
	void clear()
	{
		std::free(_stack);
		_stack = nullptr;
		_size = 0;
	}
	//Get stack pointer. Returns nullptr if nothing is allocated.
	void* stack() const { return _stack ? _stack + _size : nullptr; }

	//Get the allocated size of the stack. Returns 0 if nothing is allocated.
	std::size_t size() const { return _size; }
}; // class ManagedStack

} //namespace detail

/*
 * Generator. Creates and manages a context. Execution of this context can be
 * paused and resumed, and the context can send values by reference out of the
 * generator.
 */
template<class YieldType>
class Generator
{
private:
	//Internal typedefs and types

	//Return states to give to the generator
	enum class YieldBack : intptr_t {Resume, Stop};

	//Return states to get from the generator
	enum class GeneratorState : intptr_t {Continue, Done};

	//thrown from yield() when the context must be immediately destroyed.
	class ImmediateStop {};

public:
	//public typedefs and types.
	typedef YieldType yield_type;
	typedef YieldType* yield_ptr_type;

	/*
	 * iterator
	 *
	 * See http://www.boost.org/doc/libs/release/libs/iterator/doc/iterator_facade.html
	 * for details on the iterator_facade class. The basic idea is that it
	 * implements all the necessary iterator operations and operators in terms
	 * of simple internal methods. It handles all the iterator tagging used by
	 * STL algorithms, and it even creates a proxy handler for postfix
	 * increment.
	 */
	class iterator :
		public boost::iterator_facade<
			iterator, YieldType, boost::single_pass_traversal_tag>
	{
	public:
		typedef Generator generator_type;

	private:
		Generator* gen;

		friend class boost::iterator_core_access;

		//increment, dereference, and equal are used by iterator_facade
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

		bool equal(const iterator& rhs) const
		{
			return gen == rhs.gen;
		}

	public:
		iterator(Generator* gen=nullptr):
			gen(gen)
		{}
	};

	/*
	 * The Yielder class is passed into the client context to allow it to
	 * perform yielding. This design helps to prevent accidentally switching
	 * into the context while already inside it, or vice versa.
	 *
	 * TODO: define explicit behavior for what happens if this happens
	 */

	class Yield
	{
	private:
		friend class Generator;

		Generator *const *const gen;

		Generator& get_gen() { return **gen; }

		explicit Yield(Generator *const *const generator):
			gen(generator)
		{}

	public:
		//Yield an object
		template<class T>
		void operator()(T&& object)
		{
			get_gen().yield(&object);
		}

		/*
		 * Safely exit the context, as though from a Generator::Stop. This
		 * throws an exception to unwind the stack and call destructors; this
		 * exception is caught by the base method of the context.
		 */
		void done()
		{
			throw ImmediateStop();
		}

		/*
		 * Unsafely exit the context. This exits the context immediately, and
		 * signals the generator to wipe the context (stack, etc). Use this if
		 * you have no destructors or other scope-cleanup you need to do.
		 */
		void exit()
		{
			get_gen().exit_generator(GeneratorState::Done);
		}
	};

private:
	//State!

	//The stack that the context lives on
	detail::ScopedStack inner_stack;

	//Pointer to the most recently yielded value, which lives in the context
	YieldType* current_value;

	//Pointer to the context's copy of the this pointer
	Generator** self;

	//See the boost context docs for detailed info on these types.
	//data for the execution point of the inner context
	boost::context::fcontext_t* inner_context;

	//data for the execution point of the outer context
	boost::context::fcontext_t outer_context;

private:
	//Wrappers for launching the generator

	//Data to pass to the context
	template<class GeneratorFunc>
	struct ContextBootstrap
	{
		Generator* self;
		GeneratorFunc* func;
	};

	/*
	 * Launch the context. Set the self pointer, so that moves won't break
	 * anything. Create a Yield object and launch the generator with it,
	 */
	template<class GeneratorFunc>
	static void static_context_base(intptr_t p)
	{
		//Copy the pointers to this stack
		auto bootstrap = *reinterpret_cast<ContextBootstrap<GeneratorFunc>*>(p);

		//Set the self pointer in the Generator
		bootstrap.self->self = &bootstrap.self;

		//Launch the client function in this context
		try { (*bootstrap.func)(Yield(&bootstrap.self)); }
		catch(ImmediateStop&) {}

		//Leave the context
		bootstrap.self->current_value = nullptr;
		bootstrap.self->self = nullptr;
		bootstrap.self->exit_generator(GeneratorState::Done);
	}

	//As above, but also moves-constructs the functor on this stack and uses it
	//TODO: eliminate the code repetition, ideally without a (runtime) variable
	template<class GeneratorFunc>
	static void static_context_base_own(intptr_t p)
	{
		//Copy the pointers to this stack
		auto bootstrap = *reinterpret_cast<ContextBootstrap<GeneratorFunc>*>(p);

		//Set the self pointer in the Generator
		bootstrap.self->self = &bootstrap.self;

		try
		{
			/*
			 * Create a local copy of the GeneratorFunc. Do it in the try block
			 * so that it is properly destructed at the end.
			 */
			GeneratorFunc func(std::move(*bootstrap.func));

			//launch the client function in this context
			func(Yield(&bootstrap.self));
		}
		catch(ImmediateStop&) {}

		//Leave the context
		bootstrap.self->current_value = nullptr;
		bootstrap.self->self = nullptr;
		bootstrap.self->exit_generator(GeneratorState::Done);
	}

private:
	//Methods for exiting the context

	//core wrapper for jump_fcontext call out of the generator
	YieldBack jump_out_of_generator(GeneratorState state)
	{
		return static_cast<YieldBack>(
			boost::context::jump_fcontext(
				inner_context, &outer_context, static_cast<intptr_t>(
					state)));
	}

	//Jump out of a generator. Checks for a stop instruction
	void exit_generator(GeneratorState state)
	{
		if(jump_out_of_generator(state) == YieldBack::Stop)
		{
			throw ImmediateStop();
		}
	}

	//Collect value, then jump out
	void yield(YieldType* obj)
	{
		current_value = obj;
		exit_generator(GeneratorState::Continue);
	}

private:
	//Context Entry

	//core wrapper for jump_fcontext call into the generator
	GeneratorState jump_into_generator(intptr_t val)
	{
		return static_cast<GeneratorState>(
			boost::context::jump_fcontext(
				&outer_context, inner_context, val));
	}

	/*
	 * Checked jump into generator. checks that the context exists before
	 * entering it, and clears the context if it receives a
	 * GeneratorState::Done
	 */
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

	//Wipe the generator context. Doesn't perform any cleanup in the context.
	void clear_generator_context()
	{
		current_value = nullptr;
		inner_context = nullptr;
		inner_stack.clear();
	}

private:
	//Delegated constructor
	template<class GeneratorFunc>
	explicit Generator(GeneratorFunc* func, void(*context_base)(intptr_t),
			std::size_t stack_size):

		inner_stack(stack_size),
		current_value(nullptr),
		self(nullptr),
		inner_context(boost::context::make_fcontext(
			inner_stack.stack(),
			inner_stack.size(),
			context_base))
	{
		ContextBootstrap<GeneratorFunc> bootstrap{this, func};
		enter_generator(reinterpret_cast<intptr_t>(&bootstrap));
	}

public:
	/*
	 * Constructors: Create and launch a new generator context.
	 *
	 * Args:
	 *   func: The client function/function object to be used. The different
	 *   constructors treat this argument in different ways.
	 *
	 *   std::size_t stack_size: The requested size of the stack. The actual
	 *   allocated stack size will not be smaller than some implementation
	 *   defined minimum_size; use the stack_size method to check the actual
	 *   allocated size, and see the detai::ScopedStack class for details on
	 *   this minimum size
	 */

	/*
	 * Normal reference version. Takes a reference to a callable object, and
	 * uses that object in the context. This means that the object's members are
	 * usable outside of the context.
	 *
	 * This can also take a reference to a const object- the GeneratorFunc
	 * template parameter will be deduced to const Type, so as long as the
	 * object's operator() is const as well everything will be fine.
	 */
	template<class GeneratorFunc>
	explicit Generator(GeneratorFunc& func, std::size_t stack_size = 0):
		Generator(&func, &static_context_base<GeneratorFunc>, stack_size)
	{}

	/*
	 * Function pointer version. Takes a function pointer with the correct
	 * signature and uses it in the context. This is provided to prevent an
	 * unnessesary copy into the context that would be performed by the rvalue
	 * reference constructor.
	 */
	explicit Generator(void (*func)(Yield), std::size_t stack_size = 0):
		Generator(func, &static_context_base<void(Yield)>, stack_size)
	{}

	/*
	 * rvalue reference function. Takes an rvalue ref to the function object
	 * being used (for instance, a capturing lambda). It move-constructs an
	 * object of this type in the context and uses that object.
	 */
	template<class GeneratorFunc>
	explicit Generator(GeneratorFunc&& func, std::size_t stack_size = 0):
		Generator(&func, &static_context_base_own<GeneratorFunc>, stack_size)
	{}

	//When destructed, a generator tries to gracefully exit.
	~Generator()
	{
		stop();
	}

	//No copying or move-assigning generators
	Generator(const Generator&) =delete;
	Generator& operator=(const Generator&) =delete;
	Generator& operator=(Generator&&) =delete;

	/*
	 * Move-construct from a running generator. The other generator no longer
	 * refers to a context.
	 */
	Generator(Generator&& mve):
		inner_stack(std::move(mve.inner_stack)),
		current_value(mve.current_value),
		self(mve.self),
		inner_context(mve.inner_context)
	{
		//Update the in-context self pointer, so that Yield doesn't break.
		(*self) = this;

		//Clear the other context.
		mve.current_value = nullptr;
		mve.self = nullptr;
		mve.inner_context = nullptr;
	}

	//Resume the context until the next yield
	void advance()
	{
		resume_generator(YieldBack::Resume);
	}

	/*
	 * Resume the context, then return a pointer to the yielded value. Returns
	 * nullptr if the generator finishes.
	 */
	YieldType* next()
	{
		advance();
		return get();
	}

	/*
	 * Attempt to gracefully stop the generator. This resumes the generator,
	 * but causes an exception to be immediately raised in the context that is
	 * caught by the function at the base function of the context, allowing
	 * destructors and other scope-cleanup to happen. If the context prevents
	 * this exception from reaching the base call, it is still destroyed.
	 */
	void stop()
	{
		resume_generator(YieldBack::Stop);
		clear_generator_context();
	}

	/*
	 * Ungracefully stop the generator. This immediately wipes the inner
	 * context, without giving it the opportunity to run cleanup functions.
	 */
	void kill()
	{
		clear_generator_context();
	}

	//Get a pointer to the most recently yielded object.
	YieldType* get() const
	{
		return current_value;
	}

	//Get a const pointer to the most recently yielded object.
	const YieldType* cget() const
	{
		return current_value;
	}

	//Returns true if this context has stopped.
	bool stopped() const
	{
		return inner_context == nullptr;
	}

	//Iterators.
	iterator begin()
	{
		return iterator(this);
	}

	iterator end()
	{
		return iterator();
	}

	//Actual size of the allocated stack
	std::size_t stack_size() const
	{
		return inner_stack.size();
	}
}; // class Generator

template<class YieldType>
using Yield = typename Generator<YieldType>::Yield;

} // namespace generator
