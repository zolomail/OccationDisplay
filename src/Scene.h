#pragma once

#include <memory>
#include <vector>
#include "effects/Effect.h"

enum class SceneId : uint8_t {
	Welcome = 0,	//Must be first
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
	WaterEffect,
	BumpMapEffect,
	ValentinesDay,
	CubeEffect,
	WebUiQr,
	Test1,
	Test2,
	Test3,
	Test4,
	Test5,
	FullscreenTest,
	None	// Must be last
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
