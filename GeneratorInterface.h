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
	typedef std::unique_ptr<yield_type> yield_ptr_type;

	virtual ~GeneratorInterface() {}

	virtual yield_type next() =0;
	virtual yield_ptr_type next_ptr()
	{
		try
		{
			return yield_ptr_type(new yield_type(next()));
		}
		catch(generator_finished&)
		{}
		return nullptr;
	}
};


#endif /* GENERATORINTERFACE_H_ */
