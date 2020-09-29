/*
 * ctrl128.h
 *
 *  Created on: Jul 31, 2020
 *      Author: alex
 */

#ifndef SRC_CTRL128_CTRL128_H_
#define SRC_CTRL128_CTRL128_H_

#include "main.h"

#define FLOPPY_RAWTRACKSIZE 6250
#define FLOPPY_DATATRACKSIZE 5120
#define SECTORLEN 614
#define SECTDATAPOS 98
#define SECTDATALEN 512

struct _mfile{
	char fname[255];
	uint8_t mnt;
	uint16_t nomfile;
};

volatile void ctrl128loop(void);
void fast_memset(void *,uint8_t set,uint32_t len);
void fast_memcpy(void *,void *,uint32_t len);
void fast_memcpy2(void *,void *,uint32_t len);
void fast_memcpy4(void *,void *,uint32_t len);
uint8_t fast_cmp(void *src,void *str);

#endif /* SRC_CTRL128_CTRL128_H_ */
