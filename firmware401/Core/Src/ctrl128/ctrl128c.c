/*
 * ctrl128c.c
 *
 *  Created on: Jul 31, 2020
 *      Author: alex
 */
#include "ctrl128.h"
#include "stdlib.h"
#include "../ff14/ff.h"
#include "../ff14/diskio.h"
#include "../ssd1306/ssd1306.h"


#define    DWT_CYCCNT    *(volatile unsigned long *)0xE0001004
#define    DWT_CONTROL   *(volatile unsigned long *)0xE0001000
#define    SCB_DEMCR     *(volatile unsigned long *)0xE000EDFC


volatile uint8_t VAR_AD[31];								//base memory array for system variables
//ctrl128
volatile uint16_t *RSN_RD	=(uint16_t*) (VAR_AD+0x00UL);	//Status
volatile uint16_t *RSN_WR	=(uint16_t*) (VAR_AD+0x02UL);	//CMD
volatile uint16_t *RDN_RD	=(uint16_t*) (VAR_AD+0x04UL);	//UKNC read
volatile uint16_t *RDN_WR	=(uint16_t*) (VAR_AD+0x06UL);	//UKNC write
volatile uint16_t *POS		=(uint16_t*) (VAR_AD+0x08UL);	//position in track
volatile uint8_t  *DRIVE	=(uint8_t*)  (VAR_AD+0x0AUL);	//Drive number
volatile uint8_t  *HEAD		=(uint8_t*)  (VAR_AD+0x0BUL);	//Side
volatile uint8_t  *TRACK	=(uint8_t*)  (VAR_AD+0x0CUL);	//Cylinder
volatile uint8_t  *SECTR	=(uint8_t*)  (VAR_AD+0x0DUL);	//Sector
volatile uint8_t  *MOTOR	=(uint8_t*)  (VAR_AD+0x0EUL);	//Motor on/off
volatile uint8_t  *GOR		=(uint8_t*)  (VAR_AD+0x0FUL);	//GOR
volatile uint8_t  *SELDRV	=(uint8_t*)	 (VAR_AD+0x10UL);	//Select/deselect dive
volatile uint8_t  *SAVE		=(uint8_t*)	 (VAR_AD+0x11UL);	//Save mode
volatile uint8_t  *READSD	=(uint8_t*)	 (VAR_AD+0x12UL);	//flag READFROMSD
volatile uint8_t  *UPDATE	=(uint8_t*)	 (VAR_AD+0x13UL);	//flag UPDATE
volatile uint8_t  *UPDATESD	=(uint8_t*)	 (VAR_AD+0x14UL);	//read/write sd
volatile uint16_t *GLPOS	=(uint16_t*) (VAR_AD+0x15UL);	//global pos in track
volatile uint8_t  *DSIZE	=(uint8_t*)	 (VAR_AD+0x16UL);	//drive size 4 byte!!DSIZE[4]

volatile uint16_t rawtrk[SECTORLEN/2];
extern const uint16_t rawtrk00[SECTORLEN/2];

volatile unsigned long count_tic=0;

FATFS drives;
FIL fl[4],fcfg;
FRESULT rc;
DIR dir;
//uint16_t nomfile=0;
static FILINFO fno;
uint8_t rdwrcfg=0;

const char cfgname[]={"kakave.cfg"};

struct _mfile mfile[4]={
	{"DISKA.DSK",0,0},
	{"DISKB.DSK",0,0},
	{"DISKC.DSK",0,0},
	{"DISKD.DSK",0,0}
};
UINT count=0;

volatile uint8_t str[50];
volatile uint8_t chksd=0;
uint32_t OCR;
uint8_t nomsel=0,set=0;

volatile static void updatesd(void);
static void printdsp(void);
static void printstr(uint8_t _nomsel);
static FRESULT getfilename(char *ffname,uint8_t drvsel,uint8_t direct);

FRESULT load_cfg(void)
{
	FRESULT res;
	f_lseek(&fcfg,0);
	res=f_read(&fcfg, (void *)mfile, sizeof(struct _mfile)*4, &count);
	if(count!=sizeof(struct _mfile)*4)res=FR_INVALID_OBJECT;
	return res;
}

FRESULT save_cfg(void)
{
	FRESULT res;
	f_lseek(&fcfg,0);
	res=f_write(&fcfg, (void *)mfile, sizeof(struct _mfile)*4, &count);
	f_sync(&fcfg);
	return res;
}

void mountdsk(void)
{
	GPIOB->BSRR=SPI2_CS_Pin;

	fast_memcpy2((void*)rawtrk,(void*)rawtrk00,SECTORLEN);

	fast_memset((void *)fl,0,sizeof(fl));
	fast_memset((void *)&dir,0,sizeof(dir));

	while(f_mount(&drives,"",1)!=FR_OK)
	{
		GPIOB->BSRR|=LED1_Pin;
		LL_mDelay(50);
		GPIOB->BSRR|=LED1_Pin<<16;
		LL_mDelay(50);
	}

	rc=f_open(&fcfg, cfgname, FA_OPEN_ALWAYS|FA_READ|FA_WRITE);
	if(rc==FR_OK)
	{
		if(!rdwrcfg)
		{
			if(load_cfg()!=FR_OK)save_cfg();
		}
		else rdwrcfg=0,save_cfg();
	}

	f_opendir(&dir, "/");

	rc=f_open(&fl[0], mfile[0].fname, FA_READ|FA_WRITE);
	if(rc==FR_OK)
	{
		mfile[0].mnt=1;
		DSIZE[0]=f_size(&fl[0])==409600?39:79;
		GPIOB->BSRR|=LED1_Pin;
		LL_mDelay(50);
		GPIOB->BSRR|=LED1_Pin<<16;
		LL_mDelay(50);
	}
	else mfile[0].mnt=0;
	rc=f_open(&fl[1], mfile[1].fname, FA_READ|FA_WRITE);
	if(rc==FR_OK)
	{
		mfile[1].mnt=1;
		DSIZE[1]=f_size(&fl[1])==409600?39:79;
		GPIOB->BSRR|=LED1_Pin;
		LL_mDelay(50);
		GPIOB->BSRR|=LED1_Pin<<16;
		LL_mDelay(50);
	}
	else mfile[1].mnt=0;

	rc=f_open(&fl[2], mfile[2].fname, FA_READ|FA_WRITE);
	if(rc==FR_OK)
	{
		mfile[2].mnt=1;
		DSIZE[2]=f_size(&fl[2])==409600?39:79;
		GPIOB->BSRR|=LED1_Pin;
		LL_mDelay(50);
		GPIOB->BSRR|=LED1_Pin<<16;
		LL_mDelay(50);
	}
	else mfile[2].mnt=0;

	rc=f_open(&fl[3], mfile[3].fname, FA_READ|FA_WRITE);
	if(rc==FR_OK)
	{
		mfile[3].mnt=1;
		DSIZE[3]=f_size(&fl[3])==409600?39:79;
		GPIOB->BSRR|=LED1_Pin;
		LL_mDelay(50);
		GPIOB->BSRR|=LED1_Pin<<16;
		LL_mDelay(50);
	}
	else mfile[3].mnt=0;
}

void ctrl128ini(void)
{
	*RSN_RD		=0x00;	//Status
	*RSN_WR		=0x00;	//CMD
	*RDN_RD		=0x00;	//UKNC read
	*RDN_WR		=0x00;	//UKNC write
	*POS		=0x00;	//position in track
	*DRIVE		=0x00;	//Drive number
	*HEAD		=0x00;	//Side
	*TRACK		=0x02;	//Cylinder
	*SECTR		=0x01;	//Sector
	*MOTOR		=0x00;	//Motor on/off
	*GOR		=0x00;	//GOR
	*SELDRV		=0x00;	//Select/deselect dive
	*SAVE		=0x00;
	*READSD		=0x00;	//flag READFROMSD
	*UPDATE		=0x00;	//flag UPDATE
	*UPDATESD	=0x00;	//read/write sd
	*GLPOS		=0x00;	//global pos in track
	DSIZE[0]=DSIZE[1]=DSIZE[2]=DSIZE[3]=79; //drivesize = 39/79

	SSD1306_Init();

	mountdsk();

	printdsp();
	//TIM3 - timer for data out
	LL_TIM_SetCounter(TIM3, 0);
	LL_TIM_ClearFlag_UPDATE(TIM3);
	LL_TIM_EnableIT_UPDATE(TIM3);
	LL_TIM_EnableCounter(TIM3);
	//TIM4 - timer for test SD insert
	LL_TIM_ClearFlag_UPDATE(TIM4);
	LL_TIM_EnableIT_UPDATE(TIM4);
	//LL_TIM_EnableCounter(TIM4);
	//TIM10 - timer 3 milS head next rtack
	LL_TIM_ClearFlag_UPDATE(TIM10);
	LL_TIM_EnableIT_UPDATE(TIM10);
}

volatile void ctrl128loop(void)
{
	uint8_t displaymod=0;
	ctrl128ini();


	/*SCB_DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;// разрешаем использовать DWT
	DWT_CYCCNT = 0;// обнуляем значение
	DWT_CONTROL|= DWT_CTRL_CYCCNTENA_Msk; // включаем счётчик
	//count_tic = DWT_CYCCNT;//смотрим сколько натикало
	asm("nop");*/

	while(1)
	{
		if(*UPDATESD)*UPDATESD=0,updatesd();
		if(*MOTOR&*SELDRV)
		{
			if(TIM4->CR1&TIM_CR1_CEN)
			{
				LL_TIM_DisableCounter(TIM4);
				GPIOB->BSRR=LED1_Pin;
				displaymod=0;
				*UPDATE=0;
				fast_fill(0);
			}
		}
		else
		{
			if(!TIM4->CR1&TIM_CR1_CEN)
			{
				GPIOB->BSRR=LED1_Pin<<16;
				LL_TIM_EnableCounter(TIM4);
				displaymod=1;
				chksd=0;
			}
		}
		if(*UPDATE)
		{
			*UPDATE=0;
			if(!chksd)printdsp();
			if(chksd%10==0)
			{
				if(disk_ioctl(0,MMC_GET_OCR,&OCR)!=RES_OK)
				{
					mfile[0].nomfile=0;mfile[1].nomfile=0;
					mfile[2].nomfile=0;mfile[3].nomfile=0;
					rdwrcfg=0;
					mountdsk();
				}
			}
			if(chksd==100)fast_fill(0);
			chksd+=chksd<100?1:0;
		}
		if(displaymod)
		{
			switch((GPIOC->IDR&(LFL_BTN_Pin|RGT_BTN_Pin|SET_BTN_Pin))^0xE000)
			{
			case SET_BTN_Pin:
				if(chksd>1)set=!set,printstr(nomsel);
				if(!set)rdwrcfg=1,mountdsk();
				chksd=0;
				break;
			case RGT_BTN_Pin:
				if(nomsel && chksd>1 &&!set)--nomsel,printstr(nomsel);
				else if(set && chksd>1)getfilename(mfile[nomsel].fname,nomsel,0),printstr(nomsel);
				chksd=0;
				break;
			case LFL_BTN_Pin:
				if(nomsel<3  && chksd>1 &&!set)++nomsel,printstr(nomsel);
				else if(set && chksd>1)getfilename(mfile[nomsel].fname,nomsel,1),printstr(nomsel);
				chksd=0;
				break;
			default:
				break;
			}
		}
	}
}

volatile static void updatesd(void)
{
	/*static uint32_t foffset;
	foffset=( ((uint32_t)(*TRACK<<1))+(uint32_t)*HEAD )*FLOPPY_DATATRACKSIZE+((uint32_t)(*SECTR-1)<<9);*/
	//Bright data led
	GPIOB->BSRR=LED2_Pin;
	//set file position
	if(*READSD&0x01)*READSD^=0x01,f_lseek(&fl[*DRIVE],( ((uint32_t)(*TRACK<<1))+(uint32_t)*HEAD )*FLOPPY_DATATRACKSIZE+((uint32_t)(*SECTR-1)<<9));
	//first write
	if(*READSD&0x08)*READSD^=0x08,f_sync(&fl[*DRIVE]);
	if(*READSD&0x04)*READSD^=0x0C,f_write(&fl[*DRIVE],(void *)&rawtrk[SECTDATAPOS/2],SECTDATALEN,(UINT*)&count);
	//next read
	if(*READSD&0x02)*READSD^=0x02,f_read(&fl[*DRIVE],(void *)&rawtrk[SECTDATAPOS/2],SECTDATALEN,(UINT*)&count);
	//Upp RPLY pin
	GPIOB->BSRR=RPLY_Pin;
	//Start TIM3, data out
	LL_TIM_EnableCounter(TIM3);
	//Off data led
	GPIOB->BSRR=LED2_Pin<<16;
}

static void printstr(uint8_t _nomsel)
{
	if(_nomsel==nomsel)
	{
		if(DSIZE[_nomsel]==39)fast_string_inv(0,16*_nomsel, " #",(void *)font_8x14);
		else fast_string_inv(0,16*_nomsel, " :",(void *)font_8x14);
		fast_putc_inv(0,16*_nomsel,'0'+_nomsel,(void *)font_8x14);
	}
	else
	{
		if(DSIZE[_nomsel]==39)fast_string(0,16*_nomsel, " #",(void *)font_8x14);
		else fast_string(0,16*_nomsel, " :",(void *)font_8x14);
		fast_putc(0,16*_nomsel,'0'+_nomsel,(void *)font_8x14);
	}
	if( !set&&_nomsel==nomsel)
	{
		if(mfile[_nomsel].mnt)
			fast_string_inv(16,16*_nomsel,"            ",(void *)font_8x14),fast_string_inv(16,16*_nomsel,  mfile[_nomsel].fname,(void *)font_8x14);
		else fast_string_inv(16,16*_nomsel,"            ",(void *)font_8x14);
	}
	else
	{
		if(set&&_nomsel==nomsel)fast_string(32,16*_nomsel,"            ",(void *)font_8x14),fast_string(16,16*_nomsel,  mfile[_nomsel].fname,(void *)font_8x14);
		else
			if(mfile[_nomsel].mnt)
				fast_string(16,16*_nomsel,"            ",(void *)font_8x14),fast_string(16,16*_nomsel,  mfile[_nomsel].fname,(void *)font_8x14);
			else fast_string(16,16*_nomsel,"            ",(void *)font_8x14);
	}
}

static void printdsp(void)
{
	printstr(0);
	printstr(1);
	printstr(2);
	printstr(3);
}

/*static uint8_t test_ext(char *ffname,char *ext)
{
	uint8_t npos=0;
	char *ptrfn=ffname;
	while(*ptrfn!='.'&&*ptrfn!=0&&npos<13)++ptrfn,++npos;
	if(*ptrfn!=0&&npos<13)++ptrfn,npos=0;
	else return 0;
	while((ptrfn[0]+0x20==*ext || ptrfn[0]==*ext ) && *ptrfn!=0 && *ext!=0 && npos<4)++ptrfn,++ext,++npos;
	if(npos==3)return 1;
	return 0;
}*/


static FRESULT getfilename(char *ffname,uint8_t drvsel,uint8_t direct)
{
	FRESULT res;
	uint16_t tmpp=0;

	res = f_readdir(&dir, NULL);			/* Read a directory item */
	if(direct)
	{
		mfile[drvsel].nomfile++;
		while(res==FR_OK && tmpp<mfile[drvsel].nomfile)
		{
			res = f_readdir(&dir, &fno);	/* Read a directory item */
			if (res != FR_OK || fno.fname[0] == 0){res=FR_NO_FILE;if(mfile[drvsel].nomfile)mfile[drvsel].nomfile--;return res;}  /* Break on error or end of dir */
			if (!(fno.fattrib & AM_DIR) && !fast_cmp((void *)fno.fname,(void *)cfgname))
				tmpp++,fast_memcpy((void *)ffname,(void *)fno.fname,255);
		}
	}
	else
	{
		mfile[drvsel].nomfile-=mfile[drvsel].nomfile?1:0;
		while(res==FR_OK && tmpp<mfile[drvsel].nomfile)
		{
			res = f_readdir(&dir, &fno);    /* Read a directory item */
			if (res != FR_OK || fno.fname[0] == 0){res=FR_NO_FILE;mfile[drvsel].nomfile=0;return res;}  /* Break on error or end of dir */
			if (!(fno.fattrib & AM_DIR) && !fast_cmp((void *)fno.fname,(void *)cfgname) )
				tmpp++,fast_memcpy((void *)ffname,(void *)fno.fname,255);
		}
	}
	return res;
}
