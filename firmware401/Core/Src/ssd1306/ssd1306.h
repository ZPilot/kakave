/*
 * ssd1306.h
 *
 *  Created on: Aug 12, 2020
 *      Author: alex
 */

#ifndef SRC_SSD1306_SSD1306_H_
#define SRC_SSD1306_SSD1306_H_

#include "main.h"


#define SSD1306_WIDTH            128
#define SSD1306_HEIGHT           64

/*!< Black color, no pixel */
#define SSD1306_COLOR_BLACK 0x00
/*!< Pixel is set. Color depends on LCD */
#define SSD1306_COLOR_WHITE 0x01

typedef struct _point{
	uint8_t x,y;
	uint16_t pnt;
} POINT;

extern const uint8_t font_4x6[257][6];
extern const uint8_t font_5x8[257][8];
extern const uint8_t font_5x12[257][12];
extern const uint8_t font_7x12[257][12];
extern const uint8_t font_8x12[257][12];
extern const uint8_t font_8x14[257][14];
extern const uint8_t font_8x8[257][8];

void SSD1306_Init(void);
void fast_fill(uint8_t color);
void fast_putpixel(uint8_t x,uint8_t y,uint8_t color);
void fast_putc(uint8_t x,uint8_t y,uint8_t c,void *font);
void fast_putc_inv(uint8_t x,uint8_t y,uint8_t c,void *font);
void fast_string(uint8_t x,uint8_t y,void *str,void *font);
void fast_string_inv(uint8_t x,uint8_t y,void *str,void *font);

#endif /* SRC_SSD1306_SSD1306_H_ */
