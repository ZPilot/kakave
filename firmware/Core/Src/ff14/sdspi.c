/*
 * sdspi.c
 *
 *  Created on: Jul 15, 2020
 *      Author: alex
 */
#include "main.h"
#include "spi.h"
#include "diskio.h"

#define STM32_USE_DMA

#ifdef STM32_USE_DMA
#warning Information only - using DMA
#endif

#define SELECT()        SPI2_CS_GPIO_Port->BSRR|=(SPI2_CS_Pin<<16)    		/* MMC CS = L */
#define DESELECT()      SPI2_CS_GPIO_Port->BSRR|=SPI2_CS_Pin				/* MMC CS = H */

static void FCLK_SLOW(void) /* Set slow clock (100k-400k) */
{
   uint32_t tmp;

   tmp = SPI2->CR1;
   tmp = ( tmp | LL_SPI_BAUDRATEPRESCALER_DIV256 );
   SPI2->CR1 = tmp;
}

static void FCLK_FAST(void) /* Set fast clock (depends on the CSD) */
{
	uint32_t tmp;

   tmp = SPI2->CR1;
   tmp = ( tmp & ~LL_SPI_BAUDRATEPRESCALER_DIV256 ) | LL_SPI_BAUDRATEPRESCALER_DIV4; // 84MHz/4 here
   SPI2->CR1 = tmp;
}

static volatile
DSTATUS Stat = STA_NOINIT;   /* Disk status */

static volatile
DWORD Timer1, Timer2;   /* 100Hz decrement timers */

static
BYTE CardType;         /* Card type flags */

/*-----------------------------------------------------------------------*/
/* Transmit/Receive a byte to MMC via SPI  (Platform dependent)          */
/*-----------------------------------------------------------------------*/
static BYTE stm32_spi_rw( BYTE out )
{
   *((__IO uint8_t *)&SPI2->DR) = out;				/* Start an SPI transaction */
   	while ((SPI2->SR & 0x1) != 0x01);	/* Wait for end of the transaction */
   	return *((__IO uint8_t *)&SPI2->DR);
}

/*-----------------------------------------------------------------------*/
/* Transmit a byte to MMC via SPI  (Platform dependent)                  */
/*-----------------------------------------------------------------------*/

#define xmit_spi(dat)  stm32_spi_rw(dat)

/*-----------------------------------------------------------------------*/
/* Receive a byte from MMC via SPI  (Platform dependent)                 */
/*-----------------------------------------------------------------------*/

static
BYTE rcvr_spi (void)
{
   return stm32_spi_rw(0xff);
}

/* Alternative macro to receive data fast */
#define rcvr_spi_m(dst)  *(dst)=stm32_spi_rw(0xff)

/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static
BYTE wait_ready (void)
{
   BYTE res;


   Timer2 = 50;   /* Wait for ready in timeout of 500ms */
   rcvr_spi();
   do
      res = rcvr_spi();
   while ((res != 0xFF) && Timer2);

   return res;
}

/*-----------------------------------------------------------------------*/
/* Deselect the card and release SPI bus                                 */
/*-----------------------------------------------------------------------*/

static
void release_spi (void)
{
   DESELECT();
   rcvr_spi();
}

#ifdef STM32_USE_DMA
/*-----------------------------------------------------------------------*/
/* Transmit/Receive Block using DMA (Platform dependent. STM32 here)     */
/*-----------------------------------------------------------------------*/
static
void stm32_dma_transfer(
   BOOL receive,      /* FALSE for buff->SPI, TRUE for SPI->buff               */
   const BYTE *buff,   /* receive TRUE  : 512 byte data block to be transmitted
                     receive FALSE : Data buffer to store received data    */
   UINT btr          /* receive TRUE  : Byte count (must be multiple of 2)
                     receive FALSE : Byte count (must be 512)              */
)
{
   uint32_t rw_workbyte[] = { 0xffff };


   /* Disable DMA1 Channel4 */
   LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_3);
   /* Disable DMA1 Channel5 */
   LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);

   /* shared DMA configuration values */
   LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_3,(uint32_t)(&(SPI2->DR)));
   LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_4,(uint32_t)(&(SPI2->DR)));

   if ( receive ) {

      /* DMA1 channel4 configuration SPI2 RX ---------------------------------------------*/
	  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_3,(uint32_t)buff);
	  LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_3,512);
	  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_3,LL_DMA_MEMORY_INCREMENT);

      /* DMA1 channel5 configuration SPI2 TX ---------------------------------------------*/
	  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_4,(uint32_t)rw_workbyte);
	  LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_4,512);
	  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_4,LL_DMA_MEMORY_NOINCREMENT);

   } else {

      /* DMA1 channel2 configuration SPI2 RX ---------------------------------------------*/

      LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_3,(uint32_t)rw_workbyte);
      LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_3,512);
      LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_3,LL_DMA_MEMORY_NOINCREMENT);

      /* DMA1 channel3 configuration SPI2 TX ---------------------------------------------*/
	  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_4,(uint32_t)buff);
	  LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_4,512);
	  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_4,LL_DMA_MEMORY_INCREMENT);

   }

   /* Enable DMA1 Channel4 */
   LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_3);
   /* Enable DMA1 Channel5 */
   LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_4);

   /* Enable SPI2 TX/RX request */
   LL_SPI_EnableDMAReq_RX(SPI2);
   LL_SPI_EnableDMAReq_TX(SPI2);

   /* Wait until DMA1_Channel 5 Transfer Complete */
   // not needed: while (DMA_GetFlagStatus(DMA1_FLAG_TC3) == RESET) { ; }
   /* Wait until DMA1_Channel 4 Receive Complete */
   while (!LL_DMA_IsActiveFlag_TC3(DMA1)) { ; }

   /* Disable DMA1 Channel4 */
   LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_3);
   /* Disable DMA1 Channel5 */
   LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);

   /* Disable SPI1 RX/TX request */
   LL_SPI_DisableDMAReq_RX(SPI2);
   LL_SPI_DisableDMAReq_TX(SPI2);

   LL_DMA_ClearFlag_TC3(DMA1);
   LL_DMA_ClearFlag_TC4(DMA1);
}
#endif /* STM32_USE_DMA */

/*-----------------------------------------------------------------------*/
/* Receive a data packet from MMC                                        */
/*-----------------------------------------------------------------------*/

static
BOOL rcvr_datablock (
   BYTE *buff,         /* Data buffer to store received data */
   UINT btr         /* Byte count (must be multiple of 4) */
)
{
   BYTE token;


   Timer1 = 10;
   do {                     /* Wait for data packet in timeout of 100ms */
      token = rcvr_spi();
   } while ((token == 0xFF) && Timer1);
   if(token != 0xFE) return 0;   /* If not valid data token, return with error */

#ifdef STM32_USE_DMA
   stm32_dma_transfer( 1, buff, btr );
#else
   do {                     /* Receive the data block into buffer */
      rcvr_spi_m(buff++);
      rcvr_spi_m(buff++);
      rcvr_spi_m(buff++);
      rcvr_spi_m(buff++);
   } while (btr -= 4);
#endif /* STM32_USE_DMA */

   rcvr_spi();                  /* Discard CRC */
   rcvr_spi();

   return 1;               /* Return with success */
}

/*-----------------------------------------------------------------------*/
/* Send a data packet to MMC                                             */
/*-----------------------------------------------------------------------*/

#if _READONLY == 0
static
BOOL xmit_datablock (
   const BYTE *buff,   /* 512 byte data block to be transmitted */
   BYTE token         /* Data/Stop token */
)
{
   BYTE resp;
#ifndef STM32_USE_DMA
   BYTE wc;
#endif

   if (wait_ready() != 0xFF) return 0;

   xmit_spi(token);               /* Xmit data token */
   if (token != 0xFD) {   /* Is data token */

#ifdef STM32_USE_DMA
      stm32_dma_transfer( 0, buff, 512 );
#else
      wc = 0;
      do {                     /* Xmit the 512 byte data block to MMC */
         xmit_spi(*buff++);
         xmit_spi(*buff++);
      } while (--wc);
#endif /* STM32_USE_DMA */

      xmit_spi(0xFF);               /* CRC (Dummy) */
      xmit_spi(0xFF);
      resp = rcvr_spi();            /* Receive data response */
      if ((resp & 0x1F) != 0x05)      /* If not accepted, return with error */
         return 0;
   }

   return 1;
}
#endif /* _READONLY */

/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/

static
BYTE send_cmd (
   BYTE cmd,      /* Command byte */
   DWORD arg      /* Argument */
)
{
   BYTE n, res;


   if (cmd & 0x80) {   /* ACMD is the command sequence of CMD55-CMD */
      cmd &= 0x7F;
      res = send_cmd(CMD55, 0);
      if (res > 1) return res;
   }

   /* Select the card and wait for ready */
   DESELECT();
   SELECT();
   if (wait_ready() != 0xFF) return 0xFF;

   /* Send command packet */
   xmit_spi(cmd);                  /* Start + Command index */
   xmit_spi((BYTE)(arg >> 24));      /* Argument[31..24] */
   xmit_spi((BYTE)(arg >> 16));      /* Argument[23..16] */
   xmit_spi((BYTE)(arg >> 8));         /* Argument[15..8] */
   xmit_spi((BYTE)arg);            /* Argument[7..0] */
   n = 0x01;                     /* Dummy CRC + Stop */
   if (cmd == CMD0) n = 0x95;         /* Valid CRC for CMD0(0) */
   if (cmd == CMD8) n = 0x87;         /* Valid CRC for CMD8(0x1AA) */
   xmit_spi(n);

   /* Receive command response */
   if (cmd == CMD12) rcvr_spi();      /* Skip a stuff byte when stop reading */
   n = 10;                        /* Wait for a valid response in timeout of 10 attempts */
   do
      res = rcvr_spi();
   while ((res & 0x80) && --n);

   return res;         /* Return with the response value */
}

/*-----------------------------------------------------------------------*/
/* Device Timer Interrupt Procedure  (Platform dependent)                */
/*-----------------------------------------------------------------------*/
/* This function must be called in period of 10ms                        */

void disk_timerproc (void)
{
   BYTE n;

   n = Timer1;                  /* 100Hz decrement timer */
   if (n) Timer1 = --n;
   n = Timer2;
   if (n) Timer2 = --n;
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
   BYTE drv      /* Physical drive number (0) */
)
{
   BYTE n, cmd, ty, ocr[4];

   LL_SPI_Enable(SPI2);
   LL_SPI_ClearFlag_CRCERR(SPI2);
   SPI2->DR=0xFF;
   (void)SPI2->DR;

   if (drv) return STA_NOINIT;         /* Supports only single drive */
   if (Stat & STA_NODISK) return Stat;   /* No card in the socket */

   FCLK_SLOW();
   LL_mDelay(100);
   for (n = 12; n; n--) rcvr_spi();   /* 80 dummy clocks */
   LL_mDelay(10);

   ty = 0;
   if (send_cmd(CMD0, 0) == 1) {         /* Enter Idle state */
      Timer1 = 100;                  /* Initialization timeout of 1000 msec */
      if (send_cmd(CMD8, 0x1AA) == 1) {   /* SDHC */
         for (n = 0; n < 4; n++) ocr[n] = rcvr_spi();      /* Get trailing return value of R7 resp */
         if (ocr[2] == 0x01 && ocr[3] == 0xAA) {            /* The card can work at vdd range of 2.7-3.6V */
            while (Timer1 && send_cmd(ACMD41, 1UL << 30));   /* Wait for leaving idle state (ACMD41 with HCS bit) */
            if (Timer1 && send_cmd(CMD58, 0) == 0) {      /* Check CCS bit in the OCR */
               for (n = 0; n < 4; n++) ocr[n] = rcvr_spi();
               ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
            }
         }
      } else {                     /* SDSC or MMC */
         if (send_cmd(ACMD41, 0) <= 1)    {
            ty = CT_SD1; cmd = ACMD41;   /* SDSC */
         } else {
            ty = CT_MMC; cmd = CMD1;   /* MMC */
         }
         while (Timer1 && send_cmd(cmd, 0));         /* Wait for leaving idle state */
         if (!Timer1 || send_cmd(CMD16, 512) != 0)   /* Set R/W block length to 512 */
            ty = 0;
      }
   }
   CardType = ty;
   release_spi();

   if (ty) {         /* Initialization succeeded */
      Stat &= ~STA_NOINIT;      /* Clear STA_NOINIT */
      FCLK_FAST();
   }

   return Stat;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
   BYTE drv      /* Physical drive number (0) */
)
{
   if (drv) return STA_NOINIT;      /* Supports only single drive */
   return Stat;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read (
   BYTE drv,         /* Physical drive number (0) */
   BYTE *buff,         /* Pointer to the data buffer to store read data */
   DWORD sector,      /* Start sector number (LBA) */
   BYTE count         /* Sector count (1..255) */
)
{
   if (drv || !count) return RES_PARERR;
   if (Stat & STA_NOINIT) return RES_NOTRDY;

   if (!(CardType & CT_BLOCK)) sector *= 512;   /* Convert to byte address if needed */

   if (count == 1) {   /* Single block read */
      if ((send_cmd(CMD17, sector) == 0)   /* READ_SINGLE_BLOCK */
         && rcvr_datablock(buff, 512))
         count = 0;
   }
   else {            /* Multiple block read */
      if (send_cmd(CMD18, sector) == 0) {   /* READ_MULTIPLE_BLOCK */
         do {
            if (!rcvr_datablock(buff, 512)) break;
            buff += 512;
         } while (--count);
         send_cmd(CMD12, 0);            /* STOP_TRANSMISSION */
      }
   }
   release_spi();

   return count ? RES_ERROR : RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _READONLY == 0
DRESULT disk_write (
   BYTE drv,         /* Physical drive number (0) */
   const BYTE *buff,   /* Pointer to the data to be written */
   DWORD sector,      /* Start sector number (LBA) */
   BYTE count         /* Sector count (1..255) */
)
{
   if (drv || !count) return RES_PARERR;
   if (Stat & STA_NOINIT) return RES_NOTRDY;
   if (Stat & STA_PROTECT) return RES_WRPRT;

   if (!(CardType & CT_BLOCK)) sector *= 512;   /* Convert to byte address if needed */

   if (count == 1) {   /* Single block write */
      if ((send_cmd(CMD24, sector) == 0)   /* WRITE_BLOCK */
         && xmit_datablock(buff, 0xFE))
         count = 0;
   }
   else {            /* Multiple block write */
      if (CardType & CT_SDC) send_cmd(ACMD23, count);
      if (send_cmd(CMD25, sector) == 0) {   /* WRITE_MULTIPLE_BLOCK */
         do {
            if (!xmit_datablock(buff, 0xFC)) break;
            buff += 512;
         } while (--count);
         if (!xmit_datablock(0, 0xFD))   /* STOP_TRAN token */
            count = 1;
      }
   }
   release_spi();

   return count ? RES_ERROR : RES_OK;
}
#endif /* _READONLY == 0 */



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

//#if _USE_IOCTL != 0
DRESULT disk_ioctl (
   BYTE drv,      /* Physical drive number (0) */
   BYTE ctrl,      /* Control code */
   void *buff      /* Buffer to send/receive control data */
)
{
   DRESULT res;
   BYTE n, csd[16], *ptr = buff;
   WORD csize;


   if (drv) return RES_PARERR;

   res = RES_ERROR;

   if (ctrl == CTRL_POWER) {
      switch (*ptr) {
      case 0:      /* Sub control code == 0 (POWER_OFF) */
         //if (chk_power())
            //power_off();      /* Power off */
         res = RES_OK;
         break;
      case 1:      /* Sub control code == 1 (POWER_ON) */
         //power_on();            /* Power on */
         res = RES_OK;
         break;
      case 2:      /* Sub control code == 2 (POWER_GET) */
        // *(ptr+1) = (BYTE)chk_power();
         res = RES_OK;
         break;
      default :
         res = RES_PARERR;
      }
   }
   else {
      if (Stat & STA_NOINIT) return RES_NOTRDY;

      switch (ctrl) {
      case CTRL_SYNC :      /* Make sure that no pending write process */
         SELECT();
         if (wait_ready() == 0xFF)
            res = RES_OK;
         break;

      case GET_SECTOR_COUNT :   /* Get number of sectors on the disk (DWORD) */
         if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
            if ((csd[0] >> 6) == 1) {   /* SDC ver 2.00 */
               csize = csd[9] + ((WORD)csd[8] << 8) + 1;
               *(DWORD*)buff = (DWORD)csize << 10;
            } else {               /* SDC ver 1.XX or MMC*/
               n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
               csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
               *(DWORD*)buff = (DWORD)csize << (n - 9);
            }
            res = RES_OK;
         }
         break;

      case GET_SECTOR_SIZE :   /* Get R/W sector size (WORD) */
         *(WORD*)buff = 512;
         res = RES_OK;
         break;

      case GET_BLOCK_SIZE :   /* Get erase block size in unit of sector (DWORD) */
         if (CardType & CT_SD2) {   /* SDC ver 2.00 */
            if (send_cmd(ACMD13, 0) == 0) {   /* Read SD status */
               rcvr_spi();
               if (rcvr_datablock(csd, 16)) {            /* Read partial block */
                  for (n = 64 - 16; n; n--) rcvr_spi();   /* Purge trailing data */
                  *(DWORD*)buff = 16UL << (csd[10] >> 4);
                  res = RES_OK;
               }
            }
         } else {               /* SDC ver 1.XX or MMC */
            if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {   /* Read CSD */
               if (CardType & CT_SD1) {   /* SDC ver 1.XX */
                  *(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
               } else {               /* MMC */
                  *(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
               }
               res = RES_OK;
            }
         }
         break;

      case MMC_GET_TYPE :      /* Get card type flags (1 byte) */
         *ptr = CardType;
         res = RES_OK;
         break;

      case MMC_GET_CSD :      /* Receive CSD as a data block (16 bytes) */
         if (send_cmd(CMD9, 0) == 0      /* READ_CSD */
            && rcvr_datablock(ptr, 16))
            res = RES_OK;
         break;

      case MMC_GET_CID :      /* Receive CID as a data block (16 bytes) */
         if (send_cmd(CMD10, 0) == 0      /* READ_CID */
            && rcvr_datablock(ptr, 16))
            res = RES_OK;
         break;

      case MMC_GET_OCR :      /* Receive OCR as an R3 resp (4 bytes) */
         if (send_cmd(CMD58, 0) == 0) {   /* READ_OCR */
            for (n = 4; n; n--) *ptr++ = rcvr_spi();
            res = RES_OK;
         }
         break;

      case MMC_GET_SDSTAT :   /* Receive SD status as a data block (64 bytes) */
         if (send_cmd(ACMD13, 0) == 0) {   /* SD_STATUS */
            rcvr_spi();
            if (rcvr_datablock(ptr, 64))
               res = RES_OK;
         }
         break;

      default:
         res = RES_PARERR;
      }

      release_spi();
   }

   return res;
}
//#endif /* _USE_IOCTL != 0 */
