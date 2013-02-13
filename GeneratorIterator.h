#ifndef MAKE_ITERATOR_H_
#define MAKE_ITERATOR_H_

#include "Generator.h"
#include "GeneratorIteratorGeneric.h"


template<class T>
using GeneratorIterator =
	GeneratorIteratorGeneric<
		Generator<T>,
		typename Generator<T>::value_type>;
		
template<class T>
using GeneratorIteratorConst =
	GeneratorIteratorGeneric<
		Generator<T>,
		typename Generator<T>::value_type const>;

//in C++11, these can be used in place of .begin() and .end()
template<class T>
GeneratorIterator<T> begin(Generator<T>& gen)
{
	return GeneratorIterator<T>(&gen);
}

template<class T>
GeneratorIterator<T> end(Generator<T>& gen)
{
	return GeneratorIterator<T>();
}

template<class T>
GeneratorIteratorConst<T> cbegin(Generator<T>& gen)
{
	return GeneratorIteratorConst<T>(&gen);
}

template<class T>
GeneratorIteratorConst<T> cend(Generator<T>& gen)
{
	return GeneratorIteratorConst<T>();
}
/*
 * Would rather have used a template here, but the savings for these one-
 * liners are negligible
 */

#endif /* MAKE_ITERATOR_H_ */
