/*
 * GeneratorIterator.hpp
 *
 *  Created on: Feb 15, 2013
 *      Author: nathan
 */

#ifndef GENERATORITERATOR_HPP_
#define GENERATORITERATOR_HPP_

#include "GeneratorIteratorGeneric.hpp"
#include "Generator.hpp"

template<class T>
GeneratorIterator<T> generator_begin(T& gen)
{
	return GeneratorIterator<T>(gen);
}

template<class T>
GeneratorIterator<T> generator_end(T&)
{
	return GeneratorIterator<T>();
}

template<class G, class Y, class A>
GeneratorIterator<Generator<G, Y, A> > begin(Generator<G, Y, A>& gen)
{
	return generator_begin(gen);
}

template<class G, class Y, class A>
GeneratorIterator<Generator<G, Y, A> > end(Generator<G, Y, A>& gen)
{
	return generator_end(gen);
}



#endif /* GENERATORITERATOR_HPP_ */
