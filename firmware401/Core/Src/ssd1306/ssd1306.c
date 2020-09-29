/*
 * ssd1306.c
 *
 *  Created on: Aug 12, 2020
 *      Author: alex
 */


#include "ssd1306.h"

#define SSD1306_I2C_ADDR	0x78
#define TIMEOUT_I2C			10000

/* Write command */
#define SSD1306_WRITECOMMAND(command)      ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x00, (command))
/* Write data */
#define SSD1306_WRITEDATA(data)            ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x40, (data))

/* SSD1306 data buffer */
volatile uint8_t SSD1306_Buffer_all[SSD1306_WIDTH * SSD1306_HEIGHT / 8];


void ssd1306_I2C_Write(uint8_t address, uint8_t reg, uint8_t data)
{
	volatile uint32_t tm=TIMEOUT_I2C;
	I2C1->CR1 |= I2C_CR1_PE;
	I2C1->CR1 |= I2C_CR1_START;
	while (!(I2C1->SR1 & I2C_SR1_SB) && --tm){};
	(void) I2C1->SR1;

	tm=TIMEOUT_I2C;
	I2C1->DR = address;
	while (!(I2C1->SR1 & I2C_SR1_ADDR)&& --tm){};
	(void) I2C1->SR1;
	(void) I2C1->SR2;

	tm=TIMEOUT_I2C;
	I2C1->DR = reg;
	while (!(I2C1->SR1 & I2C_SR1_TXE)&& --tm){};
	tm=TIMEOUT_I2C;
	I2C1->DR = data;
	while (!(I2C1->SR1 & I2C_SR1_TXE)&& --tm){};

	I2C1->CR1 |= I2C_CR1_STOP;
}

void ssd1306_I2C_WriteMulti_DMA(uint8_t address, uint8_t* data, uint16_t count)
{
	volatile uint32_t tm=TIMEOUT_I2C;
	I2C1->CR1 |= I2C_CR1_PE;
	I2C1->CR1 |= I2C_CR1_START;
	while (!(I2C1->SR1 & I2C_SR1_SB)&& --tm){};
	(void) I2C1->SR1;

	tm=TIMEOUT_I2C;
	I2C1->DR = address;
	while (!(I2C1->SR1 & I2C_SR1_ADDR)&& --tm){};
	(void) I2C1->SR1;
	(void) I2C1->SR2;

	tm=TIMEOUT_I2C;
	I2C1->DR = 0x40;
	while (!(I2C1->SR1 & I2C_SR1_TXE)&& --tm){};

	//DMA1->
	LL_DMA_DisableStream(DMA1,LL_DMA_STREAM_6);

	LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_6, count);
	LL_DMA_ConfigAddresses(DMA1, LL_DMA_STREAM_6, (uint32_t)(data), (uint32_t)LL_I2C_DMA_GetRegAddr(I2C1), LL_DMA_GetDataTransferDirection(DMA1, LL_DMA_STREAM_6));
	LL_I2C_EnableDMAReq_TX(I2C1);
	LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_6);
}

void SSD1306_Init(void)
{
	uint32_t b_moder=GPIOB->MODER;
	GPIOB->MODER&=0xFFF0FFFF;
	GPIOB->MODER|=0x50000;
	GPIOB->BSRR=(LL_GPIO_PIN_8|LL_GPIO_PIN_9)<<16;
	LL_mDelay(100);
	GPIOB->BSRR=(LL_GPIO_PIN_8|LL_GPIO_PIN_9);
	GPIOB->MODER=b_moder;
	/* Init LCD */
	SSD1306_WRITECOMMAND(0xAE); //display off
	SSD1306_WRITECOMMAND(0x20); //Set Memory Addressing Mode
	SSD1306_WRITECOMMAND(0x01); //00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	SSD1306_WRITECOMMAND(0xB0); //Set Page Start Address for Page Addressing Mode,0-7
	SSD1306_WRITECOMMAND(0xC8); //Set COM Output Scan Direction
	SSD1306_WRITECOMMAND(0x00); //---set low column address
	SSD1306_WRITECOMMAND(0x10); //---set high column address
	SSD1306_WRITECOMMAND(0x40); //--set start line address
	SSD1306_WRITECOMMAND(0x81); //--set contrast control register
	SSD1306_WRITECOMMAND(0x7F);
	SSD1306_WRITECOMMAND(0xA1); //--set segment re-map 0 to 127
	SSD1306_WRITECOMMAND(0xA8); //--set multiplex ratio(1 to 64)
	SSD1306_WRITECOMMAND(0x3F); //
	SSD1306_WRITECOMMAND(0xA6); //--set normal display
	SSD1306_WRITECOMMAND(0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	SSD1306_WRITECOMMAND(0xD3); //-set display offset
	SSD1306_WRITECOMMAND(0x00); //-not offset
	SSD1306_WRITECOMMAND(0xD5); //--set display clock divide ratio/oscillator frequency
	SSD1306_WRITECOMMAND(0xF0); //--set divide ratio
	SSD1306_WRITECOMMAND(0xD9); //--set pre-charge period
	SSD1306_WRITECOMMAND(0x22); //
	SSD1306_WRITECOMMAND(0xDA); //--set com pins hardware configuration
	SSD1306_WRITECOMMAND(0x10);//(0x12);
	SSD1306_WRITECOMMAND(0xDB); //--set vcomh
	SSD1306_WRITECOMMAND(0x20); //0x20,0.77xVcc
	SSD1306_WRITECOMMAND(0x8D); //--set DC-DC enable
	SSD1306_WRITECOMMAND(0x14); //
	SSD1306_WRITECOMMAND(0x21); //
	SSD1306_WRITECOMMAND(0x0); //
	SSD1306_WRITECOMMAND(127); //
	SSD1306_WRITECOMMAND(0x22); //
	SSD1306_WRITECOMMAND(0x0); //
	SSD1306_WRITECOMMAND(7); //
	SSD1306_WRITECOMMAND(0xAF); //--turn on SSD1306 panel



	ssd1306_I2C_WriteMulti_DMA(SSD1306_I2C_ADDR, (uint8_t *)SSD1306_Buffer_all, SSD1306_WIDTH * SSD1306_HEIGHT / 8);
	fast_fill(0);
}
