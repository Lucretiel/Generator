/*
 * ManagedStack.h
 *
 *  Created on: Feb 8, 2013
 *      Author: nathan
 */

#ifndef MANAGEDSTACK_H_
#define MANAGEDSTACK_H_

//Simple class to add RAII to a context stack allocator
template<class Allocator>
class ManagedStack
{
private:
	static Allocator alloc;
	std::size_t _size;
	void* _stack;

public:
	ManagedStack(std::size_t size):
		_size(size),
		_stack(alloc.allocate(size))
	{}

	~ManagedStack()
	{
		clear();
	}

	//No copying
	ManagedStack(const ManagedStack&) =delete;
	ManagedStack& operator=(const ManagedStack&) =delete;

	//Moving
	ManagedStack(ManagedStack&& mve):
		_size(mve._size),
		_stack(mve._stack)
	{
		mve._stack = nullptr;
	}
	ManagedStack& operator=(ManagedStack&& mve)
	{
		clear();
		_size = mve._size;
		_stack = mve._stack;

		mve._stack = nullptr;
		return *this;
	}

	void clear()
	{
		if(_stack)
		{
			alloc.deallocate(_stack, _size);
			_stack = nullptr;
		}
	}

	void* stack_pointer() const { return _stack; }
	std::size_t size() const { return _size; }
};

template<class Allocator>
Allocator ManagedStack<Allocator>::alloc;

#endif /* MANAGEDSTACK_H_ */
