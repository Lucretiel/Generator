Generator
=========

Python/C# style yield statements in C++

Overview
--------

This library allows the user to create generators- functions that can be paused
and resumed, yielding values to the caller. These functions maintain their
stacks between yields.

`Generator` objects are templated on the type of object they yield. To create
one, create an object of the type, and pass a callable object that takes a
`Yield` object as its argument:

```cpp
// A generator that yields 1,2,3,4
Generator<int> generator(
[](Yield<int> yield)
{
	yield(1); yield(2); yield(3); yield(4);
});
```

When a `Generator` is first constructed, it immediately launches. The function
is executed in its own stack. When it calls `yield(value)`, execution of the of
the function is suspended, and a pointer to the value is passed to the calling
context. The calling context, in turn, can call the `generator.get()` to get
this pointer. The caller can then call `generator.advance()` to resume the
generator function. This causes the function's stack to be restored, and for
execution to continue from the site the most recent yield until the next one.

If the function returns, the generator becomes stopped. All the allocated
resources are reclaimed immediately, and execution returns to the last time the
generator was resumed (either construction or a call to `advance`). Once the
generator is stopped, it cannot be restarted; `get()` will always return
`nullptr`, `advance` becomes a no-op, and `stopped()` returns `true`.

### Complete simple example:

```cpp
#include <iostream>
#include "Generator.hpp"

namespace gen = generator;
using namespace std;

int main()
{
	cout << "[main] creating generator\n";

	gen::Generator<int> generator(
	[](gen::Yield<int> yield)
	{
		cout << "[generator] generator started\n";
		for(int i = 0; i < 5; ++i)
		{
			cout << "[generator] local i: " << i << "\n";
			cout << "[generator] yielding i\n";
			yield(i);
		}
		cout << "[generator] exiting generator\n";
	});

	cout << "[main] beginning for loop\n";
	for(int i = 5; i > 0; --i)
	{
		cout << "[main] local i: " << i << "\n";
		cout << "[main] getting from generator: " << *generator.get() << "\n";
		cout << "[main] advancing generator\n";
		generator.advance();
	}
	cout << "[main] exiting\n";
}
```

#### Output:

```
[main] creating generator
[generator] generator started
[generator] local i: 0
[generator] yielding i
[main] beginning for loop
[main] local i: 5
[main] getting from generator: 0
[main] advancing generator
[generator] local i: 1
[generator] yielding i
[main] local i: 4
[main] getting from generator: 1
[main] advancing generator
[generator] local i: 2
[generator] yielding i
[main] local i: 3
[main] getting from generator: 2
[main] advancing generator
[generator] local i: 3
[generator] yielding i
[main] local i: 2
[main] getting from generator: 3
[main] advancing generator
[generator] local i: 4
[generator] yielding i
[main] local i: 1
[main] getting from generator: 4
[main] advancing generator
[generator] exiting generator
[main] exiting
```

Details
-------

### Getting and using the library

This library is header-only, and all contained in a single file. However, it
depends on boost (specifically `boost::context` and `boost::iterator_facade`).
`boost::context` in particular needs to be build and linked against in order
to use the generator; see the boost docs at
http://www.boost.org/doc/libs/1_55_0/more/getting_started/index.html
to learn how to build this library on your system.

### Construction

A Generator can be constructed with any callable object or function pointer with
the correct signature- `/* ignored return type */ function(Yield<YieldType>)`.
The template parameter of the Yield argument must be the same as that of the
Generator object; doing otherwise is a compiler error. This callable can be
passed as an lvalue or an rvalue. If it is passed as an lvalue, a reference is
kept to the original object and used- this means that any changes to the object
in the generator's context are reflected in the original object. If it is passed
as an rvalue, a new instance is move-constructed in the generator's local stack
and destroyed when the generator ends.

A Generator can also optionally receive a size parameter as a second argument;
this is the requested size in bytes of the stack that will be allocated for the
general. The actual size may be adjusted to match your OS page layout or other
considerations, but it will always be at least that many bytes; you can call
`stack_size()` to get the actual allocated size.

The generator is immediately launched when first constructed. Execution is
paused once the first yield is called; if no yield is called the generator
becomes stopped immediately.

Generators can also be move-constructed from other generators. In this case, the
new generator takes ownership of the internal context of the old generator, and
the old generator becomes stopped, if it wasn't already. Unlike normal
construction, move-constructing a generator does not resume or launch the
internal context.

Generators cannot be copied. They currently cannot be move-assigned, though this
may change in a future release.

### Yielding and Generator lifetime

The generator function receives a single Yield object as its argument. This
object is used to yield values to the calling context; for this section we will
assume the object is called `yield`.

The generator yields a value by calling `yield(object)`. Objects are yielded by
reference- the calling context calls `get()` to get a pointer to this object.
Any changes to the object are reflected when the generator resumes. Both lvalues
and rvalues may be yielded. The generator function can also call `yield()` with
no argument; in this case, calls to `get()` will return `nullptr`.

If the generator function returns, the generator stops. Execution returns to the
last time the generator was resumed (either the constructor or a call to
`advance`), and the stack memory is recovered. A stopped generator cannot be
restarted or otherwise be made to have a new context, though this may change
if move-assignment is introduced.

A generator function may also stop by calling `yield.done()` or `yield.exit()`.
`yield.exit()` will cause the context to be exited and the stack destroyed
*immediately*, without calling any local destructors, similar to a call to
`std::exit()`. `yield.done()` performs a clean exit by throwing an exception of
an internal type and catching it below the generator function itself; this
allows proper stack unwinding to happen and destructors to be called.

If an exception is allowed to leave the generator function without being caught
(besides the internal exception type used by `yield.done()` and `stop()`, it is
undefined behavior. In my limited testing it is treated the same as an exception
leaving `main`- `terminate` called, etc. 

### Using the generator

Once the generator object is created and control is returned to the calling
context, the generator can be used. The caller can use the `get()` function to
get a pointer to the most recently yielded object, and the `advance()` function
to advance the generator to the next yield. The convenience function `next()` is
also provided, which advances the generator then returns the pointer to the new
value.

The pointers returned by `get()` point to the yielded values, so the calling can
context can communicate with the generator by manipulating. Additionally, if a
function object was passed by lvalue reference into the generator, that objects
members are available to both the caller and the generator function.

The generator can be iterated over, and has the standard functions `begin()` and
`end()` to get iterators. The iterators are standard InputIterators, but also
have some features of OutputIterators. Dereferencing them gets a reference to
the yielded value, and incrementing them advances the generator, invalidating
old iterators. An iterator will compare equal to the `end()` iterator when the
generator becomes stopped. Postfix-incrementing the iterator creates a copy of
the yielded object and stores it in a proxy before advancing the generator. This
is why these iterator do not model standard OutputIterators; they do not have
postfix-increment-assignment (`*it++ = x;` will not assign x to the yielded
value in the generator). If the generator yields forever, iteration will never
terminate. Yielding nothing to an iterator is undefined behavior, as it is the
same as dereferencing a `nullptr`.

The calling context can call `stack_size()` to get the size in bytes of the
stack allocated to the generator. It can call `stopped()` to test if the
generator is stopped.

### Ending the generator

The caller has many options for prematurely stopping a generator. The normal way
is to call `stop()`. This will cause the generator to attempt to cleanly exit-
as with `yield.done()`, an exception of internal type is thrown at the last
yield site, allowing destructors to be called and the stack to be unwound. The
exception is caught when it leaves the generator function, and the generator's
resources are freed. If, for whatever reason, this exception is NOT allowed to
propogate out of the generator function, the context it still cleared the next
time control returns to the calling context.

The caller can choose instead to call `kill()`. This wipes out the context
without performing any cleanup and should be used sparingly.

If the generator is destructed before it is stopped, it calls `stop()`.
