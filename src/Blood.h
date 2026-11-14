#pragma once

#include "Effect.h"

class Drop
{
public:
	Drop();
	Drop(const Drop &f);
	Drop(uint8_t start_y);
	uint8_t		y;
	bool		falling;
};

class Blood : public Effect
{
public:
	Blood(Adafruit_GFX &matrix);
	void render();
private:
	std::vector<Drop> m_drips;
};