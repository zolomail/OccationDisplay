#pragma once

#include <memory>
#include <vector>
#include "effects/Effect.h"

enum class SceneId : uint8_t {
	None = 0,
	Welcome,
	Connecting,
	SetupAp,
	WifiUnknown,
	WifiStopped,
	Test,
	NewYearsMidnight,
	JanuaryDays,
	Aquatic,
	Halloween,
	JustSnow,
	ChristmasEve,
	Birthday,
	NewYearsDay,
	ChristmasDays,
	NormalDays,
	FlamingHot,
	WaterEffect
};

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
