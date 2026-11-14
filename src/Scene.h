#pragma once

#include "Effect.h"

class Scene : public std::vector<std::shared_ptr<Effect>>
{
public:
	using std::vector<std::shared_ptr<Effect>>::vector;
	void render(){
		for(auto &effect: *this)
		{
			effect->render();
			yield();
		}
	};
};
