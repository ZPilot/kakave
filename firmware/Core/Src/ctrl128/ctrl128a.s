/*
 * ctrl128a.s
 *
 *  Created on: Jul 31, 2020
 *      Author: alex
 */

//Предлагаю записать 0x1593, оно же 012623 в восьмеричной. Это "CRC" в RADIX-50.
.syntax unified
.thumb                 @ тип используемых инструкций Thumb
.cpu cortex-m4    @ семейство микроконтроллера
.word	0x20010000	@ Стек

//GPIO
.equ GPIO_BASE,		0x40020000UL
.equ GPIOA,			0x0000UL
.equ GPIOB,			0x0400UL
.equ GPIOC,			0x0800UL

.equ MODER,			0x00
.equ OTYPER,		0x04
.equ OSPEEDR,		0x08
.equ PUPDR,			0x0C
.equ IDR,			0x10
.equ ODR,			0x14
.equ BSRR,			0x18
.equ BRR,			0x1A
.equ LCKR,			0x1C

//GPIO SETINGS
//port A input
.equ MODER_I, 0x00000000UL
//port A output
.equ MODER_O, 0x55555555UL

//PINOUT
//GPIO PINS
.equ SYNC,			(0x1UL)
.equ DIN, 			(0x1UL << (1U))
.equ DOUT, 			(0x1UL << (2U))
.equ RPLY, 			(0x1UL << (3U))
.equ SEL1,			(0x1UL << (4U))
.equ SEL2,			(0x1UL << (5U))
.equ VA87DIR,		(0x1UL << (10U))
.equ TESTPIN,		(0x1UL << (6U))
.equ SETBTN,		(0x1UL << (15U))
.equ RPLY_IN,		(0x1UL << (6U))
.equ LED2,			(0x1UL << (7U))

//EXTI
.equ EXTI_PR1,		0x40013C14UL
.equ EXTI_LINE_4,	(0x1UL << (4U))
.equ EXTI_LINE_5,	(0x1UL << (5U))
.equ EXTI_LINE_15,	(0x1UL << (15U))

//RAM
.equ MEM_BASEA,			VAR_AD
.equ RSN_RDA,			0x00UL	//Status
.equ RSN_WRA,			0x02UL	//CMD
.equ RDN_RDA,			0x04UL	//UKNC read
.equ RDN_WRA,			0x06UL	//UKNC write
.equ POSA,				0x08UL	//position in track
.equ DRIVEA,			0x0AUL	//Drive number
.equ HEADA,				0x0BUL	//Side
.equ TRACKA,			0x0CUL	//Cylinder
.equ SECTRA,			0x0DUL	//Sector
.equ MOTORA,			0x0EUL	//Motor on/off
.equ GORA,				0x0FUL	//GOR
.equ SELDRVA,			0x10UL	//Select/deselect dive
.equ SAVEA,				0x11UL	//Save mode
.equ READSDA,			0x12UL	//flag READFROMSD
.equ UPDATEA,			0x13UL	//flag UPDATE
.equ UPDATESDA,			0x14UL	//read/write sd
.equ GLPOSA,			0x15UL	//global pos in track

//TIMERS
.equ TIM2,				0x40000000
.equ TIM3,				0x40000400
.equ TIM4,				0x40000800
.equ TIM9,				0x40014000
.equ TIM10,				0x40014400
.equ TIM11,				0x40014800

.equ TIM_CR1,			0x00
.equ TIM_SR,			0x10
.equ TIM_EGR,			0x14
.equ TIM_CNT,			0x24

.equ TIM_CR1_CEN,		0x1UL
.equ TIM_SR_UIF,		0x1UL
.equ TIM_EGR_UG,		0x1UL

//const
//----------------------------------------------
.equ CYLPOS,			52
.equ SIDEPOS,			53
.equ SECTPOS,			54
.equ SETOFSSET,			94
.equ SETREADSD,			96
.equ SECTR11LEN,		110
.equ RAWTRACKSIZE,		6250
.equ FDD_DATATRACKSIZE, 5120
.equ SECTORLEN, 		614
.equ SECTDATAPOS, 		98
.equ SECTDATALEN, 		512
.equ TRACKENDLEN,		110
//---------------------------------------
.macro	SETGPIO_R0 regwrk,port,value
	ldrh \regwrk,=\value
	strh \regwrk, [r0, #(\port+BSRR)]
.endm
.macro	RESETGPIO_R0 regwrk,port,value
	ldrh \regwrk,=\value
	strh \regwrk,[r0,#(\port+BSRR+2)] //ah-ah-ah BRR?!
.endm
.macro	SETPORT_R0 regwrk,port,regport,number
	ldr \regwrk,=\number
	str \regwrk,[r0,#(\port+\regport)]
.endm

.macro	SETODRPORT_R0 regs,port
	str \regs,[r0,#(\port+ODR)]
.endm

.macro	READPORT_R0 reg,port
	ldrh \reg,[r0,#(\port+IDR)]
.endm

.macro	EXTICLEARFLAG regprt,regwrk,ExtiLine
	ldr \regwrk,=\ExtiLine
	str \regwrk, [\regprt, #(EXTI_PR1)]
.endm
//----------------------------------------------

//-------------macro CMDPARCE-----------------
//	ldr r0,=GPIO_BASE	ldr r1,=MEM_BASE
//  r2 = #RSN_WRA (CMD)
//--------------------------------------------
.macro CMDPARCE
	push {r4-r5}
	//ldrb r5,=0						//r5 - clear update reg
testcmd_start:
//-----select drive------
	lsr 	r3,r2,#10					//if set 10 bit
	ands 	r3,#1
	ldrb 	r4,[r1,#SELDRVA]			//load flag SELDRV
	strb 	r3,[r1,#SELDRVA]			//save new flag SELDRV
	beq		testcmd_end_end

	cmp 	r3,r4						//old SELDRV!= new SELDRV
	it ne
		ldrbne r5,=1				//then set flag update
//------motor------------
	lsr 	r3,r2,#4
	and 	r3,#1
	ldrb 	r4,[r1,#MOTORA]
	strb 	r3,[r1,#MOTORA]

	cmp 	r3,r4						//old MOTOR!= new MOTOR
	it ne
		ldrbne 	r5,=1				//then set flag update
	cmp		r3,#0
		beq 	testcmd_end_end
//------drive------------
	and 	r3,r2,#3
	eor 	r3,#3
	ldrb 	r4,[r1,#DRIVEA]
	strb 	r3,[r1,#DRIVEA]
	cmp 	r3,r4						//old DRIVE!= new DRIVE
	it ne
		ldrbne r5,=1				//then set flag update
testcmd_head:
//------head------------
	lsr 	r3,r2,#5
	and 	r3,#1
	ldrb 	r4,[r1,#HEADA]
	strb 	r3,[r1,#HEADA]
	cmp 	r3,r4						//old HEAD!= new HEAD
	ittt ne
		ldrbne 		r5,=1			//then set flag update
		ldrne		r4,=rawtrk		//r5=rawtrk
		strbne		r3,[r4,#SIDEPOS]//save to rawsect[SIDEPOS]=HEAD
//------step ------------
	tst 	r2,#0x80					//if no set flag STEP
		beq 	gor						//then next step
	ldrb 	r3,[r1,#TRACKA]			//load number TRACK
	tst 	r2,#0x40					//dir +/-
	ite eq
		subeq 	r3,#1					//to 0 track
		addne 	r3,#1					//from 0 track
	cmp 	r3,#0						//if TRACK<0
	it mi
		movmi 	r3,#0					//then TRACK=0
	cmp 	r3,#79						//if TRACK>79
	it gt
		movgt 	r3,#79				//then TRACK=79
	ldr		r4,=rawtrk				//r5=rawtrk
	strb 	r3,[r1,#TRACKA]			//save TRACK
	strb	r3,[r4,#CYLPOS]			//save to rawsect[CYLPOS]=TRACKA
	ldrb 	r5,=1					//set flag update
//------GOR------------
gor:
	tst 	r2,#0x100					//if not set flag GOR
		beq 	testcmd_end				//goto next step
gor_set:							//set flag GOR
	and 	r3,#0
	strh 	r3,[r1,#RSN_RDA]			//clean STATUS (RSN_RDA)
	strh 	r3,[r1,#RDN_RDA]			//clean DATA
	add 	r3,#1
	strb 	r3,[r1,#GORA]				//save flag GOR
testcmd_end:
	cbz 	r5,testcmd_end_end	//if no set flag UPDATE

	//----stop TIM3------------
	ldr 	r2,=(TIM3+TIM_CR1)				//stop timer3
	ldr 	r3,[r2]							//load CR
	bic 	r3,#TIM_CR1_CEN					//clear bit Enable
	str 	r3,[r2]							//
	//----stop TIM10-----------
	ldr 	r2,=(TIM10+TIM_CR1)				//stop timer10
	ldr 	r3,[r2]							//load CR
	bic 	r3,#TIM_CR1_CEN					//clear bit Enable
	str 	r3,[r2]							//

	ldrh	r3,[r1,#POSA]
	ldrh	r4,[r1,#GLPOSA]
	sub		r4,r4,r3
	strh	r4,[r1,#GLPOSA]

	and 	r3,#0
	strh	r3,[r1,#RSN_RDA]				//clear Status
	strh	r3,[r1,#RDN_RDA]				//Clear DATA
	strh	r3,[r1,#POSA]					//set pos=0
	strh	r3,[r1,#GLPOSA]					//load pos in sector
	add		r3,#1
	strb	r3,[r1,#SECTRA]					//set sectr=1

	//----start TIM10-----------
	ldr 	r2,=TIM10						//stop timer10
	and		r3,#0							//set CNT=0
	str		r3,[r2,TIM_CNT]
	ldr 	r3,[r2,#TIM_CR1]				//load CR
	orr 	r3,#TIM_CR1_CEN					//clear bit Enable
	str 	r3,[r2,#TIM_CR1]				//
testcmd_end_end:
	pop {r4-r5}
.endm


//----------------------------------------------
//		EXTI4_IRQHandler macro
//---------------------------------------
.macro RESETEXTI4
//reset exti port
	ldr r0,=EXTI_PR1
	ldr r3,=EXTI_LINE_4
	str r3,[r0]
.endm
.macro ENDIRQSTATUS
	//release RPLY|VA87DIR
	SETGPIO_R0 r3,GPIOB,#(RPLY|VA87DIR)
	RESETEXTI4
	pop {r0-r3,lr}
	bx lr
.endm
//----------------------------------------------
//		EXTI4_IRQHandler //RSN o177130
//----------------------------------------------
.section	.text.EXTI4_IRQHandler
	.global	EXTI4_IRQHandler
	.type	EXTI4_IRQHandler, %function
EXTI4_IRQHandler:
	push {r0-r3,lr}
	ldr r0,=GPIO_BASE
	ldr r1,=MEM_BASEA
	ldrh r2,[r1,#RSN_RDA]			//preload Status
irq4_start:
	READPORT_R0 r3,GPIOB
	tst r3,#(SYNC|DIN)
		beq irq4_rsn_din
	tst r3,#(SYNC|DOUT)
		beq irq4_rsn_dout
	tst r3,#(SYNC)
		beq irq4_start
	ENDIRQSTATUS
//-------------STATUS-------------------------
irq4_rsn_din:
	RESETGPIO_R0 r3,GPIOB,#(VA87DIR)
	SETPORT_R0 r3,GPIOA,MODER,MODER_O
	SETODRPORT_R0 r2,GPIOA 			//Status in r2
	RESETGPIO_R0 r3,GPIOB,#(RPLY)
	irq4_din_rsn_h_wait:
		READPORT_R0 r3,GPIOB
		tst r3,#(DIN)
			beq irq4_din_rsn_h_wait

	SETPORT_R0 r3,GPIOA,MODER,MODER_I
	ENDIRQSTATUS
//----------CMD-------------------------------
irq4_rsn_dout:
	RESETGPIO_R0 r3,GPIOB,#(RPLY)
	SETPORT_R0 r3,GPIOA,MODER,MODER_I
	irq4_rsn_dout_h_wait:
		READPORT_R0 r2,GPIOA
		READPORT_R0 r3,GPIOB
		tst r3,#(DOUT)
			beq irq4_rsn_dout_h_wait

	//release RPLY|VA87DIR
	SETGPIO_R0 r3,GPIOB,#(RPLY|VA87DIR)
	RESETEXTI4
	CMDPARCE
	pop {r0-r3,lr}
	bx lr
//--------------------------------------------

//--------------------------------------------
//--------------------------------------------
.macro RESETEXTI5
	ldr 	r2,=EXTI_PR1					//reset exti port
	ldr 	r3,=EXTI_LINE_5					//LINE_5
	str 	r3,[r2]							//
.endm

.macro ENDIRQDATA
	RESETEXTI5
	//release RPLY|VA87DIR
	SETGPIO_R0 	r3,GPIOB,#(RPLY|VA87DIR)	//higth VA87DIR and RPLY
	pop {r0-r3,lr}
	bx lr
.endm

.macro CLEAN_TR_BIT
	ldrh 	r3,[r1,#RSN_RDA]				//Read status
	bic 	r3,#0x80 						// 7 bit (TR)
	strh 	r3,[r1,#RSN_RDA]				//write status
.endm

//----------------------------------------------
//		EXTI9_5_IRQHandler // RDN o177132
//----------------------------------------------
.section	.text.EXTI9_5_IRQHandler
	.global	EXTI9_5_IRQHandler
	.type	EXTI9_5_IRQHandler, %function
EXTI9_5_IRQHandler:
	push {r0-r3,lr}
	ldr 	r0,=GPIO_BASE
	ldr 	r1,=MEM_BASEA
	ldrh 	r2,[r1,#RDN_RDA]				//preload r3 DATA
irq95_start:
	READPORT_R0 	r3,GPIOB
	tst 	r3,#(SYNC|DIN)
		beq 		irq95_rdn_din
	tst 	r3,#(SYNC|DOUT)
		beq 		irq95_rdn_dout
	tst 	r3,#(SYNC)
		beq irq95_start
	ENDIRQDATA

irq95_rdn_din:								//uknc read data
	RESETGPIO_R0 	r3,GPIOB,#(VA87DIR)		//Set VA87 to output
	SETPORT_R0 		r3,GPIOA,MODER,MODER_O	//GPIOA to output
	SETODRPORT_R0 	r2, GPIOA				//
	RESETGPIO_R0 	r3,GPIOB,#(RPLY)		//RPLY
	irq95_din_rdn_h_wait:
		READPORT_R0 r3,GPIOB
		tst 	r3,#(DIN)					//if DIN is low
			beq 	irq95_din_rdn_h_wait	//white DIN
	SETPORT_R0 		r3,GPIOA,MODER,MODER_I	//GPIOA to input
	CLEAN_TR_BIT							//
	and 	r3,#0							//Clear flag save mode
	strb 	r3,[r1,#SAVEA]					//
	ldrb 	r3,[r1,#READSDA]				//if set flag READSDA
	cbnz 	r3,irq95_rdn_sd					//then goto read/write from sd
	ENDIRQDATA								//EXTI,RPLY and VA87 to input

irq95_rdn_dout:								//uknc write data
	RESETGPIO_R0 	r3,GPIOB,#(RPLY)
	SETPORT_R0 		r3,GPIOA,MODER,MODER_I
	irq95_rdn_dout_h_wait:
		READPORT_R0 r3,GPIOA
		READPORT_R0 r2,GPIOB
		tst 	r2,#(DOUT)
		beq 	irq95_rdn_dout_h_wait
	strh 	r3,[r1,#RDN_WRA]
	CLEAN_TR_BIT
	ldrb 	r3,=1							//Set flag
	strb 	r3,[r1,#SAVEA]					//save mode
	ldrb 	r3,[r1,#READSDA]				//if set flag READSDA
	cbnz 	r3,irq95_rdn_sd					//goto read/write from sd
	ENDIRQDATA

irq95_rdn_sd:
	ldr 	r2,=(TIM3+TIM_CR1)				//stop timer3
	ldr 	r3,[r2]							//load CR
	bic 	r3,#TIM_CR1_CEN					//clear bit Enable
	str 	r3,[r2]							//
	RESETEXTI5								//clear bit EXTI LINE5
	//-------------------interrupt mode-------------------------------
	//ldr		r3,=1							//generate irq TIM9
	//ldr		r2,=(TIM9+TIM_EGR)				//
	//str		r3,[r2]							//
	//-------------------no interrupt mode----------------------------
	ldrb	r3,=1
	strb	r3,[r1,#UPDATESDA]
	//----------------------------------------------------------------
	//release VA87DIR
	SETGPIO_R0 	r3,GPIOB,#(VA87DIR)			//higth VA87DIR, low RPLY
	pop {r0-r3,lr}
	bx lr

//------------------------------------------------------------------
//		TIM3_IRQHandler DATA in/out
//------------------------------------------------------------------
.section	.text.TIM3_IRQHandler
	.global	TIM3_IRQHandler
	.type	TIM3_IRQHandler, %function
TIM3_IRQHandler:
	push	{r0-r7,lr}
//-------clean bit TIM3 irq handler---------------------------------
	ldr		r0,=(TIM3+TIM_SR)
	and		r1,#0
	str		r1,[r0]
//------------Local const-------------------------------------------
	ldr		r0,=MEM_BASEA
	ldrh	r1,[r0,#POSA]					//load pos in sector
	ldrh	r5,[r0,#GLPOSA]					//r5=load pos in track
	ldrb	r2,[r0,#SECTRA]					//number of sector
	ldr		r4,=rawtrk						//r4=rawtrk
	ldr		r3,=stat_rw						//service sector status mass
	ldrh	r3,[r3,r1]						//r3=status
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++ r0-mass,r1-pos,r2-sec,r3,-status,r4-data,r5-pos in track ++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//--------- Motor&SelDrv--------------------------------------------
	ldrb	r6,[r0,MOTORA]
	ldrb	r7,[r0,SELDRVA]
	and		r6,r7							//if MOTOR=off or no select drive
	cmp		r6,#1
		bne		TIM3_IRQHandler_NoDrv			//then goto step NoDrv
//+++++++++ Start +++++++++++++++++++++++++++++++++++++++++++++++++++++
//--------- Track=0 ------------------------------------------------
	ldrb	r6,[r0,#TRACKA]					//r4=TRACK
	cmp		r6,#0							//if TRACK=0
	itte	eq								//then
		orreq	r3,#0x0001					//status TRK0
		ldrheq	r7,=0x0001
		andne		r7,#0
//--------- Index bit ----------------------------------------------
	cmp		r5,#0x64						//if pos<100 set index bit
	it		lo
		orrlo	r3,#0x8000
//-----Send message for set oset for sd-----------------------------
	tst		r3,#0x0200						//if pos=SETOFSSET
	ittt 	ne								//then
		ldrbne		r6,[r0,#READSDA]		//Get flag READSD
		orrne		r6,#1					//
		strbne		r6,[r0,#READSDA]		//Set flag READSD=1 to step "set pos sd"
//-----Chek servis positions----------------------------------------
	cmp		r1,#CYLPOS						//if pos=cylpos
	it 		eq								//then
		strbeq		r2,[r4,#SECTPOS]		//save to rawsect[SECTPOS]=SECTR
//------------START Save -------------------------------------------
	ldrb 	r6,[r0,#SAVEA]					//if set flag save
	cmp		r6,#1
		beq		TIM3_IRQHandler_Save		//then goto step Save
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++ r0-mass,r1-pos,r2-sec,r3,-status,r4-data,r5-pos in track +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++READ MODE++++++++++++++++++++++++++++++++++++++++++++++
//-------Chek synchro marker----------------------------------------
	tst		r3,#0x0100						//service synchro marker?
	itt 	ne								//Yes
		andne		r6,#0					//
		strbne		r6,[r0,#GORA]			//save GOR=0
//---------CMD GOR--------------------------------------------------
	ldrb	r6,[r0,#GORA]					//If set flag GOR CMD
	cmp		r6,#1
	itttt	eq								//then
		//andeq		r6,#0					//
		ldrheq		r6,=0xFFFF				//
		strheq 		r6,[r0,#RDN_RDA]		//RDN_RDA=0
		andeq		r3,#0x8000				//Status Index
		beq			TIM3_IRQHandler_end		//goto step END
//--------GAP3 (11 sector)------------------------------------------
	cmp 	r2,#11							//if sectr=11
	itttt	eq								//then
		ldrheq		r6,=0x4E4E				//DATA=0x4E4E
		strheq		r6,[r0,#RDN_RDA]		//Save Data to RDN_RD
		orreq		r3,r7,0x80				//0x80+TRK0
		beq			TIM3_IRQHandler_end		//goto step END
//---------READ message from SD-------------------------------------
	tst		r3,#0x0400						//if pos=SETREADSD
	ittt	ne								//then
		ldrbne		r6,[r0,#READSDA]		//Get flag READSD
		orrne		r6,#2					//
		strbne		r6,[r0,#READSDA]		//Set flag READSDA=2 to step "load from sd"
//--Set DATA to RDN_RD  (Read DATA port 177132)--
	ldrh	r6,[r4,r1]						//step "read DATA"
	rev16	r6,r6							//revers low and hight byte
	strh	r6,[r0,#RDN_RDA]				//Save Data to RDN_RD
	b		TIM3_IRQHandler_end

TIM3_IRQHandler_Save:
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++ r0-mass,r1-pos,r2-sec,r3,-status,r4-data,r5-pos in track +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++SAVE MODE++++++++++++++++++++++++++++++++++++++++++++++
//--------GAP3 (11 sector)------------------------------------------
	cmp 	r2,#11							//if sectr=11
	itt		eq								//then
		orreq		r3,r7,0x80				//0x80+TRK0
		beq			TIM3_IRQHandler_end		//goto step END
//-------------------------------------------------------------------
	tst		r3,#0x2000
	ittt	ne
		ldrbne		r6,[r0,#READSDA]		//Get flag READSD
		orrne		r6,0x04
		strbne		r6,[r0,#READSDA]		//Set flag READSD=4 to step "Save to sd"
//-------------------------------------------------------------------
	tst		r3,#0x1000
	ittt	ne
		ldrhne 		r6,[r0,#RDN_WRA]		//rawtrk[pos]=RDN_WRA
		strhne		r6,[r4,r1]
		bicne		r3,#0x4000
TIM3_IRQHandler_end:
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++ r0-mass,r1-pos,r2-sec,r3,-status,r4-data,r5-pos in track +++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++END MODE++++++++++++++++++++++++++++++++++++++++++++++
//----------- Status -----------------------------------------------
	ldr		r7,=0xC087
	and		r3,r7
	strh	r3,[r0,#RSN_RDA] 				//Save status
//------------------------------------------------------------------
	cmp 	r2,#11							//if SECTR=11
	ite		eq								//
		ldreq	r6,=SECTR11LEN				//then r6,=SECTR11LEN
		ldrne	r6,=SECTORLEN				//else r6,=SECTORLEN
	add 	r1,#2							//pos in sector+=2
	add		r5,#2							//glpos in track +=2
	cmp 	r1,r6							//if pos=end sector or end track
	itt		eq								//then
		andeq	r1,#0						//pos=0
		addeq	r2,#1						//sectr++
	cmp		r2,#11							//if sectr>11
	itt		hi								//then
		ldrbhi	r2,=1						//sectr=1
		andhi	r5,#0
	strb	r2,[r0,#SECTRA]					//save SETCR
	strh	r1,[r0,#POSA]					//save pos
	strh	r5,[r0,#GLPOSA]					//save global pos
	pop		{r0-r7,lr}
	bx		lr

TIM3_IRQHandler_NoDrv:
	ldrh	r6,=0xFFFF					//
	strh	r6,[r0,#RDN_RDA]			//RDN_RDA=0xFFFF
	ldrh	r6,=0x80					//Set TR bit
	strh	r6,[r0,#RSN_RDA]			//RSN_RDA=0, Status =0, TR bit set
	pop		{r0-r7,lr}
	bx		lr

//----------------------------------------------
//		TIM4_IRQHandler CMD runer
//----------------------------------------------
.section	.text.TIM4_IRQHandler
	.global	TIM4_IRQHandler
	.type	TIM4_IRQHandler, %function
TIM4_IRQHandler:
	push {r0,r1,lr}
	ldr		r0,=(TIM4+TIM_SR)
	and		r1,#0
	str		r1,[r0]
	//-----
	ldr r0,=MEM_BASEA
	ldrb r1,=1
	strb r1,[r0,#UPDATEA]
	//-----
TIM4_IRQHandler_end:
	pop {r0,r1,lr}
	bx lr


//----------------------------------------------
//		TIM1_UP_TIM10_IRQHandler nex track delay
//----------------------------------------------
.section	.text.TIM1_UP_TIM10_IRQHandler
	.global	TIM1_UP_TIM10_IRQHandler
	.type	TIM1_UP_TIM10_IRQHandler, %function
TIM1_UP_TIM10_IRQHandler:
	push {r0,r1,lr}
	ldr		r0,=(TIM10+TIM_SR)
	and		r1,#0
	str		r1,[r0]
	//-----
	//ldr r0,=MEM_BASEA

	//----start TIM3------------
	ldr 	r2,=TIM3						//start timer3
	and		r3,#0							//set 0 in CNT
	str 	r3,[r2,#TIM_CNT]				//
	ldr 	r3,[r2,#TIM_CR1]				//load CR
	orr 	r3,#TIM_CR1_CEN					//clear bit Enable
	str 	r3,[r2,#TIM_CR1]				//
	//-----
TIM1_UP_TIM10_IRQHandler_end:
	pop {r0,r1,lr}
	bx lr

//----------------------------------------
//void fast_memcpy(void *,void *,uint32_t len)
//----------------------------------------
.section	.text.fast_memcpy
	.global	fast_memcpy
	.type	fast_memcpy, %function
fast_memcpy:
	push {r3,lr}
fast_memcpy_loop:
	ldrb r3,[r1],#1
	strb r3,[r0],#1
	subs r2,#1
	bne fast_memcpy_loop
	pop {r3,lr}
	bx lr

//----------------------------------------
//void fast_memcpy2(void *,void *,uint32_t len)
//----------------------------------------
.section	.text.fast_memcpy2
	.global	fast_memcpy2
	.type	fast_memcpy2, %function
fast_memcpy2:
	push {r3,lr}
fast_memcpy_loop2:
	ldr r3,[r1],#2
	str r3,[r0],#2
	subs r2,#2
	bne fast_memcpy_loop2
	pop {r3,lr}
	bx lr

//----------------------------------------
//void fast_memcpy4(void *des,void *src,uint32_t len)
//----------------------------------------
.section	.text.fast_memcpy4
	.global	fast_memcpy4
	.type	fast_memcpy4, %function
fast_memcpy4:
	push {r3,lr}
fast_memcpy_loop4:
	ldr r3,[r1],#4
	str r3,[r0],#4
	subs r2,#4
	bne fast_memcpy_loop4
	pop {r3,lr}
	bx lr

//----------------------------------------
//void fast_memset(void *,uint8_t set,uint32_t len)
//----------------------------------------
.section	.text.fast_memset
	.global	fast_memset
	.type	fast_memset, %function
fast_memset:
	push {r1-r2,lr}
fast_memset_loop:
	strb r1,[r0],#1
	subs r2,#1
	bne fast_memset_loop
	pop {r1-r2,lr}
	bx lr

//----------------------------------------
//uint8_t fast_cmp(void *src,void *str)
//----------------------------------------
.section	.text.fast_cmp
	.global	fast_cmp
	.type	fast_cmp, %function
fast_cmp:
	push {r2-r4,lr}
	ldr		r4,=0
fast_cmp_loop:
	ldrb 	r2,[r0],#1
	ldrb 	r3,[r1],#1
	cmp 	r2,#0x60
	it	hs
		subhs	r2,#0x20
	cmp 	r3,#0x60
	it	hs
		subhs	r3,#0x20
	cmp		r2,r3
	itee	eq
		addeq	r4,#1
		andne	r4,#0
		bne		fast_cmp_end
	cmp		r2,#0
		beq fast_cmp_end
	cmp		r3,#0
		beq fast_cmp_end
	b	fast_cmp_loop
fast_cmp_end:
	mov		r0,r4
	pop {r2-r4,lr}
	bx lr
