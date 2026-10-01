/*
 * ssd1306.c
 *
 *  Created on: Aug 12, 2020
 *      Author: alex
 *
 * 128x64 I2C OLED at 0x3C. Two kinds of controller are found on these
 * modules, and SSD1306_Init() tells them apart by the status byte:
 *
 *  - SSD1306: vertical addressing mode, and SSD1306_Buffer_all goes out to
 *    the display by one endless circular DMA.
 *  - CH1116 (SH1106 command set): 132 columns of RAM and page addressing
 *    only, so the endless DMA cannot work (the column pointer never moves on
 *    to the next page). Each page is sent as its own I2C transaction by a
 *    non-blocking state machine, one step per SysTick (1 ms, the lowest
 *    interrupt priority), so the picture also follows while the code waits
 *    (LL_mDelay, SD card loops). LL_mDelay() only polls COUNTFLAG, which the
 *    interrupt does not touch.
 *
 * The SH1106 status byte is BUSY ON/OFF x x x 0 0 0. The SSD1306 has no
 * status read on I2C: it does not answer (read as 0xFF) or gives 0x06/0x03
 * in the low bits. So: low 3 bits all 0 - CH1116, anything else - SSD1306,
 * as before.
 */


#include "ssd1306.h"
#include "i2c.h"

#define SSD1306_I2C_ADDR	0x78
#define TIMEOUT_I2C			10000

/* Write command */
#define SSD1306_WRITECOMMAND(command)      ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x00, (command))
/* Write data */
#define SSD1306_WRITEDATA(data)            ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x40, (data))

/* SSD1306 data buffer */
volatile uint8_t SSD1306_Buffer_all[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

/* Status byte read at init (0xFF: no answer) and the controller found */
volatile uint8_t oled_status;
volatile uint8_t oled_ch1116;


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

/* Status read: S 0x78 0x00 0xE3 P (control byte "command" and a NOP, which
 * both controllers take), then S 0x79 status P. Returns 0xFF if nothing
 * answers the read. */
static uint8_t ssd1306_I2C_ReadStatus(uint8_t address)
{
	volatile uint32_t tm;
	uint8_t st=0xFF;

	ssd1306_I2C_Write(address, 0x00, 0xE3);
	tm=TIMEOUT_I2C;
	while ((I2C1->CR1 & I2C_CR1_STOP) && --tm){};

	I2C1->SR1 = ~I2C_SR1_AF & 0xFFFF;	/* a NACK of the write above */
	I2C1->CR1 |= I2C_CR1_ACK;
	I2C1->CR1 |= I2C_CR1_START;
	tm=TIMEOUT_I2C;
	while (!(I2C1->SR1 & I2C_SR1_SB) && --tm){};

	I2C1->DR = address | 1;
	tm=TIMEOUT_I2C;
	while (!(I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF)) && --tm){};
	if (I2C1->SR1 & I2C_SR1_ADDR)
	{
		/* one byte: NACK it, clear ADDR (SR1 then SR2), then STOP */
		I2C1->CR1 &= ~I2C_CR1_ACK;
		(void) I2C1->SR2;
		I2C1->CR1 |= I2C_CR1_STOP;
		tm=TIMEOUT_I2C;
		while (!(I2C1->SR1 & I2C_SR1_RXNE) && --tm){};
		if (tm) st = I2C1->DR;
	}
	else
	{
		I2C1->SR1 = ~I2C_SR1_AF & 0xFFFF;
		I2C1->CR1 |= I2C_CR1_STOP;
	}
	tm=TIMEOUT_I2C;
	while ((I2C1->CR1 & I2C_CR1_STOP) && --tm){};
	I2C1->CR1 |= I2C_CR1_ACK;
	return st;
}

static void ssd1306_init_ssd1306(void)
{
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

/*----------------------------------------------------------------------------
 * CH1116 (SH1106 command set)
 *
 * Each page is one I2C transaction:
 *
 *   S 0x78 | 0x80 B0+p  0x80 0x00  0x80 0x10 | 0x40 col0 .. col131 | P
 *
 * The control bytes 0x80 (one command follows) and 0x40 (data up to STOP)
 * let the page/column commands and the data go out in one DMA transfer.
 * SSD1306_Buffer_all stays in SSD1306 vertical-mode order (byte x*8+page),
 * so the drawing code is the same; a page is gathered into ch1116_tx just
 * before it is sent. Between steps the I2C master holds SCL low. A page takes
 * ~8 SysTicks, a whole screen ~70 ms.
 *--------------------------------------------------------------------------*/
/* The 128 visible columns sit in the middle of the 132-column RAM
 * (SEG2..SEG129); the other 4 columns are sent as 0. */
#define CH1116_COL_OFFSET	2
#define CH1116_RAM_WIDTH	132
#define CH1116_HDR			7
#define CH1116_TIMEOUT		(1u<<20)	/* wait units without progress before I2C reset */
#define CH1116_TICK_WAIT	(CH1116_TIMEOUT / 1000)	/* one SysTick: the reset after ~1 s */

enum { ST_IDLE, ST_SB, ST_ADDR, ST_DMA, ST_BTF, ST_STOP };

static uint8_t ch1116_tx[CH1116_HDR + CH1116_RAM_WIDTH];
static uint8_t ch1116_state;
static uint8_t ch1116_page;
static uint32_t ch1116_wait;

static void ch1116_fill_page(uint8_t page)
{
	uint8_t *d = ch1116_tx;
	uint32_t x;

	*d++ = 0x80; *d++ = 0xB0 | page;	/* page address */
	*d++ = 0x80; *d++ = 0x00;			/* column low nibble = 0 */
	*d++ = 0x80; *d++ = 0x10;			/* column high nibble = 0 */
	*d++ = 0x40;						/* data follows */
	for (x = 0; x < CH1116_COL_OFFSET; x++) *d++ = 0;
	for (x = 0; x < SSD1306_WIDTH; x++) *d++ = SSD1306_Buffer_all[x * 8 + page];
	for (x = CH1116_COL_OFFSET + SSD1306_WIDTH; x < CH1116_RAM_WIDTH; x++) *d++ = 0;
}

static void ch1116_dma_off(void)
{
	LL_I2C_DisableDMAReq_TX(I2C1);
	LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_6);
	while (LL_DMA_IsEnabledStream(DMA1, LL_DMA_STREAM_6)) {};
	DMA1->HIFCR = DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 |
				  DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6;
}

/* Bus stuck (e.g. SDA held low): reset the I2C block and set it up again as
 * MX_I2C1_Init() left it. */
static void ch1116_i2c_reset(void)
{
	ch1116_dma_off();
	I2C1->CR1 |= I2C_CR1_SWRST;
	I2C1->CR1 &= ~I2C_CR1_SWRST;
	MX_I2C1_Init();
	LL_DMA_SetMode(DMA1, LL_DMA_STREAM_6, LL_DMA_MODE_NORMAL);
}

/* One step of the refresh. Returns 1 when page 7 has just been sent. `wait`
 * is what a call without progress adds towards CH1116_TIMEOUT: 1 from the
 * tight loop in init, CH1116_TICK_WAIT from SysTick. */
static uint32_t ch1116_poll(uint32_t wait)
{
	uint32_t sr1 = I2C1->SR1;
	uint32_t done = 0;

	if (sr1 & (I2C_SR1_AF | I2C_SR1_ARLO | I2C_SR1_BERR))
	{
		/* NACK (display gone) or bus error: drop this page */
		I2C1->SR1 = ~(I2C_SR1_AF | I2C_SR1_ARLO | I2C_SR1_BERR) & 0xFFFF;
		ch1116_dma_off();
		I2C1->CR1 |= I2C_CR1_STOP;
		ch1116_page = (ch1116_page + 1) & 7;
		ch1116_state = ST_STOP;
		ch1116_wait = 0;
		return ch1116_page == 0;
	}

	switch (ch1116_state)
	{
	case ST_IDLE:
		if ((I2C1->CR1 & I2C_CR1_STOP) || (I2C1->SR2 & I2C_SR2_BUSY)) break;
		ch1116_fill_page(ch1116_page);
		I2C1->CR1 |= I2C_CR1_PE;
		I2C1->CR1 |= I2C_CR1_START;
		ch1116_state = ST_SB;
		ch1116_wait = 0;
		return 0;
	case ST_SB:
		if (!(sr1 & I2C_SR1_SB)) break;
		I2C1->DR = SSD1306_I2C_ADDR;
		ch1116_state = ST_ADDR;
		ch1116_wait = 0;
		return 0;
	case ST_ADDR:
		if (!(sr1 & I2C_SR1_ADDR)) break;
		(void) I2C1->SR2;				/* SR1 then SR2 clears ADDR */
		ch1116_dma_off();
		LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_6, sizeof(ch1116_tx));
		LL_DMA_ConfigAddresses(DMA1, LL_DMA_STREAM_6, (uint32_t)ch1116_tx,
				LL_I2C_DMA_GetRegAddr(I2C1), LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
		LL_I2C_EnableDMAReq_TX(I2C1);
		LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_6);
		ch1116_state = ST_DMA;
		ch1116_wait = 0;
		return 0;
	case ST_DMA:
		if (!(DMA1->HISR & DMA_HISR_TCIF6)) break;
		ch1116_state = ST_BTF;
		ch1116_wait = 0;
		return 0;
	case ST_BTF:
		if (!(sr1 & I2C_SR1_BTF)) break;	/* last byte out of the shifter */
		I2C1->CR1 |= I2C_CR1_STOP;
		ch1116_dma_off();
		ch1116_page = (ch1116_page + 1) & 7;
		done = ch1116_page == 0;
		ch1116_state = ST_STOP;
		ch1116_wait = 0;
		return done;
	case ST_STOP:
		if (I2C1->CR1 & I2C_CR1_STOP) break;	/* cleared once STOP is on the bus */
		ch1116_state = ST_IDLE;
		ch1116_wait = 0;
		return 0;
	}

	if ((ch1116_wait += wait) >= CH1116_TIMEOUT)
	{
		ch1116_i2c_reset();
		ch1116_state = ST_IDLE;
		ch1116_wait = 0;
	}
	return 0;
}

static void ssd1306_init_ch1116(void)
{
	uint32_t n;

	/* MX_I2C1_Init() sets the stream up circular for the SSD1306 */
	LL_DMA_SetMode(DMA1, LL_DMA_STREAM_6, LL_DMA_MODE_NORMAL);

	SSD1306_WRITECOMMAND(0xAE); //display off
	SSD1306_WRITECOMMAND(0xD5); //display clock divide ratio/oscillator frequency
	SSD1306_WRITECOMMAND(0x80);
	SSD1306_WRITECOMMAND(0xA8); //multiplex ratio
	SSD1306_WRITECOMMAND(0x3F); //1/64
	SSD1306_WRITECOMMAND(0xD3); //display offset
	SSD1306_WRITECOMMAND(0x00);
	SSD1306_WRITECOMMAND(0x40); //display start line 0
	SSD1306_WRITECOMMAND(0xAD); //DC-DC control (SH1106)
	SSD1306_WRITECOMMAND(0x8B); //DC-DC on
	SSD1306_WRITECOMMAND(0x8D); //charge pump (SSD1306 style; SH1106 parts ignore it)
	SSD1306_WRITECOMMAND(0x14); //  and take this one as "column high = 4", reset per page
	SSD1306_WRITECOMMAND(0xA1); //segment remap, as the SSD1306
	SSD1306_WRITECOMMAND(0xC8); //COM scan reversed, as the SSD1306
	SSD1306_WRITECOMMAND(0xDA); //COM pins hardware configuration
	SSD1306_WRITECOMMAND(0x12); //alternative
	SSD1306_WRITECOMMAND(0x81); //contrast
	SSD1306_WRITECOMMAND(0x7F);
	SSD1306_WRITECOMMAND(0xD9); //pre-charge period
	SSD1306_WRITECOMMAND(0x22);
	SSD1306_WRITECOMMAND(0xDB); //VCOM deselect level
	SSD1306_WRITECOMMAND(0x35);
	SSD1306_WRITECOMMAND(0xA4); //output follows RAM
	SSD1306_WRITECOMMAND(0xA6); //normal display

	/* RAM is random after power-up: clear all 8 pages (the buffer is still
	 * zero) before the panel is switched on. */
	fast_fill(0);
	ch1116_state = ST_STOP;			/* wait for the last command's STOP */
	ch1116_page = 0;
	ch1116_wait = 0;
	for (n = 0; n < 8 * CH1116_TIMEOUT && !ch1116_poll(1); n++) {};

	SSD1306_WRITECOMMAND(0xAF); //display on
	ch1116_state = ST_STOP;

	/* From here on the picture goes out from SysTick_Handler (priority set
	 * in main.c, below the bus and timer interrupts). */
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

/* 1 ms, enabled only for the CH1116. The SSD1306 is fed by the circular
 * DMA and needs nothing. */
void SysTick_Handler(void)
{
	if (oled_ch1116) ch1116_poll(CH1116_TICK_WAIT);
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

	oled_status = ssd1306_I2C_ReadStatus(SSD1306_I2C_ADDR);
	oled_ch1116 = (oled_status & 0x07) == 0;
	if (oled_ch1116) ssd1306_init_ch1116();
	else ssd1306_init_ssd1306();
}
