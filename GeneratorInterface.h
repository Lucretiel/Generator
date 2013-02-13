/*
 * GeneratorInterface.h
 *
 *  Created on: Feb 13, 2013
 *      Author: nathan
 */

#ifndef GENERATORINTERFACE_H_
#define GENERATORINTERFACE_H_

#include <memory>

class GeneratorFinished {};

template<class YieldType>
class GeneratorInterface
{
public:
	typedef YieldType yield_type;
	typedef GeneratorFinished generator_finished;

	virtual ~GeneratorInterface() {}

	virtual yield_type next() =0;
	virtual std::unique_ptr<yield_type> next_ptr() =0;
};


#endif /* GENERATORINTERFACE_H_ */
