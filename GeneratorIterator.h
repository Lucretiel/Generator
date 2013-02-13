#ifndef GENERATORITERATOR_H_
#define GENERATORITERATOR_H_

#include "Generator.h"
#include "GeneratorIteratorGeneric.h"

template<class T>
using GeneratorIterator = GeneratorIteratorGeneric<Generator<T> >;

//in C++11, these can be used in place of .begin() and .end()
template<class T>
GeneratorIterator<T> begin(Generator<T>& gen)
{
	return ++GeneratorIterator<T>(gen);
}

template<class T>
GeneratorIterator<T> end(Generator<T>& gen)
{
	return GeneratorIterator<T>();
}

#endif /* GENERATORITERATOR_H_ */
