#include "ClearScreen.h"

ClearScreen::ClearScreen(Adafruit_GFX &matrix, uint8_t r, uint8_t g, uint8_t b) :
	Effect(matrix),
	m_r(r),
	m_g(g),
	m_b(b)
{
}

void ClearScreen::render()
{
	m_matrix.fillScreen(color565(m_r, m_g, m_b));
/*
	if(m_r == 0 && m_g == 0 && m_b == 0)
	{
//		m_matrix.clearDisplay();
	}else{
//		uint16_t color = color565(m_r, m_g, m_b);
*/
/*
		m_matrix.drawPixelRGB565(3,0,0b0000100000000000);
		m_matrix.drawPixelRGB565(4,0,0b0001000000000000);
		m_matrix.drawPixelRGB565(5,0,0b0010000000000000);
		m_matrix.drawPixelRGB565(6,0,0b0100000000000000);
		m_matrix.drawPixelRGB565(7,0,0b1000000000000000);
		m_matrix.drawPixelRGB565(2,1,0b0000000000100000);
		m_matrix.drawPixelRGB565(3,1,0b0000000001000000);
		m_matrix.drawPixelRGB565(4,1,0b0000000010000000);
		m_matrix.drawPixelRGB565(5,1,0b0000000100000000);
		m_matrix.drawPixelRGB565(6,1,0b0000001000000000);
		m_matrix.drawPixelRGB565(7,1,0b0000010000000000);
		m_matrix.drawPixelRGB565(3,2,0b0000000000000001);
		m_matrix.drawPixelRGB565(4,2,0b0000000000000010);
		m_matrix.drawPixelRGB565(5,2,0b0000000000000100);
		m_matrix.drawPixelRGB565(6,2,0b0000000000001000);
		m_matrix.drawPixelRGB565(7,2,0b0000000000010000);
		for(int x=0;x<127;x++)
		{
			m_matrix.drawPixel(x,12,color565(x<<1,0,0));
			m_matrix.drawPixel(x,13,color565(0,x<<1,0));
			m_matrix.drawPixel(x,14,color565(0,0,x<<1));
		}
*/
//		m_matrix.fillRect(0, 0, m_matrix.width(), m_matrix.height(), color);	// TIME: 64ms
/*
		uint16_t color = 0b0000100000100001;
		for(uint8_t y=m_matrix.height()/4;y<m_matrix.height()/2; y++)							// TIME: 62ms
		{
			m_matrix.drawFastHLine(0, y, m_matrix.width(), color);
		}
*/
//		uint16_t color = 0b0000100001100001;
//		m_matrix.fillScreen(color);
//	}
}