/*
 * ssd1306.s
 *
 *  Created on: Aug 12, 2020
 *      Author: alex
 */


.syntax unified
.thumb                 @ тип используемых инструкций Thumb
.cpu cortex-m4    @ семейство микроконтроллера
.word	0x20010000	@ Стек

.equ SSD1306_WIDTH,            128
.equ SSD1306_HEIGHT,           64
.equ SSD1306_WIDTHDIV8,        16
.equ SSD1306_HEIGHTDIV8,       8

//----------------------------------------
//void fast_fill(uint8_t color)
//----------------------------------------
.section	.text.fast_fill
	.global	fast_fill
	.type	fast_fill, %function
fast_fill:
	push {r1-r2,lr}
	cmp r0,#0
	ite eq
		ldreq r0,=0x00000000
		ldrne r0,=0xFFFFFFFF
	ldr r1,=SSD1306_Buffer_all
	ldr r2,=0x400			//SSD1306_WIDTH*SSD1306_HEIGHT/8
fast_fill_loop:
	str r0,[r1],#4
	subs r2,#4
	bne fast_fill_loop
	pop {r1-r2,lr}
	bx lr

//----------------------------------------
//void fast_putpixel(uint8_t x,uint8_t y,uint8_t color)
//----------------------------------------
.section	.text.fast_putpixel
	.global	fast_putpixel
	.type	fast_putpixel, %function
fast_putpixel:
	push {r3-r5,lr}
	ldr r3,=SSD1306_HEIGHTDIV8
	mul r0,r3
	lsr r4,r1,#3			//r4=y/8
	add r0,r4				//r0=pos in memory
	sub r1,r1,r4,lsl #3		//y-r4*8
	ldr r4,=1
	lsl r4,r1
	ldr r3,=SSD1306_Buffer_all
	ldrb r5,[r3,r0]
	cmp r2,#1
	ite eq
		orreq r5,r4
		bicne r5,r4
	strb r5,[r3,r0]
	pop {r3-r5,lr}
	bx lr

//----------------------------------------
//void fast_putc(uint8_t x,uint8_t y,uint8_t c,void *font)
//----------------------------------------
.section	.text.fast_putc
	.global	fast_putc
	.type	fast_putc, %function
fast_putc:
	push {r4-r9,lr}
	ldrb r4,[r3]				//get r4=font szx
	ldrb r5,[r3,#1]				//get r5=font szy
	add r2,#1					//c+=1 skip fons parametr
	mla r3,r2,r5,r3				//font mem pos=c*szy+addr mem
fast_putc_loop0:
	ldrb r6,[r3],#1				//r6=font[r3],font+1
	mov r7,r4					//r7=szx
	mov r8,r0					//r8=x
fast_putc_loop1:
	tst r6,#1					//font[r3]&1
	ite ne						//==1?
		ldrne r9,=1				//yes
		andeq r9,#0				//no
	push {r0-r2}
		mov r0,r8
		mov r2,r9
		bl fast_putpixel
	pop {r0-r2}
	lsr r6,#1
	add r8,#1
	subs r7,#1
		bne fast_putc_loop1
	add r1,#1
	subs r5,#1
		bne fast_putc_loop0
	pop {r4-r9,lr}
	bx lr


//----------------------------------------
//void fast_putc_inv(uint8_t x,uint8_t y,uint8_t c,void *font)
//----------------------------------------
.section	.text.fast_putc_inv
	.global	fast_putc_inv
	.type	fast_putc_inv, %function
fast_putc_inv:
	push {r4-r9,lr}
	ldrb r4,[r3]				//get r4=font szx
	ldrb r5,[r3,#1]				//get r5=font szy
	add r2,#1					//c+=1 skip fons parametr
	mla r3,r2,r5,r3				//font mem pos=c*szy+addr mem
fast_putc_inv_loop0:
	ldrb r6,[r3],#1				//r6=font[r3],font+1
	mov r7,r4					//r7=szx
	mov r8,r0					//r8=x
fast_putc_inv_loop1:
	tst r6,#1					//font[r3]&1
	ite eq						//==1?
		ldreq r9,=1				//yes
		andne r9,#0				//no
	push {r0-r2}
		mov r0,r8
		mov r2,r9
		bl fast_putpixel
	pop {r0-r2}
	lsr r6,#1
	add r8,#1
	subs r7,#1
		bne fast_putc_inv_loop1
	add r1,#1
	subs r5,#1
		bne fast_putc_inv_loop0
	pop {r4-r9,lr}
	bx lr

//----------------------------------------
//void fast_string(uint8_t x,uint8_t y,void *str,void *font)
//----------------------------------------
.section	.text.fast_string
	.global	fast_string
	.type	fast_string, %function
fast_string:
	push {r4-r5,lr}
	ldrb r4,[r3]				//get r4=font szx
fast_string_loop:
	ldrb r5,[r2],#1				//r5=*str,str++
	cbz r5,fast_string_end		//if r5=0 then end
	push {r0-r3}					//save r2
	mov r2,r5					//r2=r5
	bl fast_putc				//putc(x,y,r2,font)
	pop {r0-r3}					//load r2
	add r0,r4					//x+font szx
	b fast_string_loop			//goto loop
fast_string_end:
	pop {r4-r5,lr}
	bx lr

//----------------------------------------
//void fast_string_inv(uint8_t x,uint8_t y,void *str,void *font)
//----------------------------------------
.section	.text.fast_string_inv
	.global	fast_string_inv
	.type	fast_string_inv, %function
fast_string_inv:
	push {r4-r5,lr}
	ldrb r4,[r3]				//get r4=font szx
fast_string_inv_loop:
	ldrb r5,[r2],#1				//r5=*str,str++
	cbz r5,fast_string_inv_end	//if r5=0 then end
	push {r0-r3}				//save r2
	mov r2,r5					//r2=r5
	bl fast_putc_inv			//putc(x,y,r2,font)
	pop {r0-r3}					//load r2
	add r0,r4					//x+font szx
	b fast_string_inv_loop			//goto loop
fast_string_inv_end:
	pop {r4-r5,lr}
	bx lr
