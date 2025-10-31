.equ PIO_INSTRUCT,      0x00

.equ PIO_ENABLE,        0x10

.equ PIO_FLAGS,         0x30



.equ VRAM_MAX_ADDR,     76800

.equ STORE_OPCODE,      0x02

.equ FLAG_DONE_MASK,    0x01

.equ FLAG_ERROR_MASK,   0x02

.equ FLAG_ZOOM_Max_MASK,   0x03

.equ FLAG_ZOOM_Min_MASK,   0x04

.equ TIMEOUT_COUNT,     0x0



.section .rodata

.LC0:           .asciz "/dev/mem"

.LC1:           .asciz "ERROR: could not open '/dev/mem' ...\n"

.LC2:           .asciz "ERROR: mmap() failed ...\n"

.LC3:           .asciz "ERROR: munmap() failed ...\n"



.section .data

FPGA_ADRS:

    .space 4

FILE_DESCRIPTOR:

    .space 4

LW_SPAM:

    .word 0x1000

LW_BASE:

    .word 0xff200



.text

.global iniciarBib

.type iniciarBib, %function

iniciarBib:

    push    {r4-r7, lr}

    ldr      r0, =.LC0

    ldr      r1, =4098

    mov      r2, #0

    mov      r7, #5



.NMAP_CALL:

    svc      0

    mov      r4, r0

    ldr      r1, =FILE_DESCRIPTOR

    str      r4, [r1]

    cmp      r4, #-1

    bne      .NMAP_SETUP

    ldr      r0, =.LC1

    bl       puts

    mov      r0, #-1

    B        .RETURN_INIT



.NMAP_SETUP:

    mov      r0, #0

    ldr      r1, =LW_SPAM
    ldr      r1, [r1]

    mov      r2, #3
    mov      r3, #1



    ldr      r4, =FILE_DESCRIPTOR
    ldr      r4, [r4]

    ldr      r5, =LW_BASE
    ldr      r5, [r5]


    mov      r7, #192

    svc      0

    mov      r4, r0

    ldr      r1, =FPGA_ADRS

    str      r4, [r1]

    cmp      r4, #-1

    bne      .INIT_DONE

    ldr      r0, =.LC2

    bl       puts

    ldr      r0, =FILE_DESCRIPTOR

    ldr      r0, [r0]

    bl       close

    mov      r0, #-1

    B        .RETURN_INIT

.INIT_DONE:

    mov      r0, #0

.RETURN_INIT:

    pop      {r4-r7, pc}

.size iniciarBib, .-iniciarBib

.global encerrarBib

.type encerrarBib, %function

encerrarBib:

    push    {r4-r7, lr}

    ldr      r0, =FPGA_ADRS
    ldr      r0, [r0]

    ldr      r1, =LW_SPAM
    ldr      r1, [r1]

    mov      r7, #91
    svc      0
    mov      r4, r0

    cmp      r4, #0
    beq      .CLOSE_FD

    ldr      r0, =.LC3
    bl       puts

    mov      r0, #-1
    B        .STATUS_BIB

.CLOSE_FD:

    ldr      r0, =FILE_DESCRIPTOR
    ldr      r0, [r0]

    mov      r7, #6

    svc      0

    mov      r0, #0

.STATUS_BIB:

    pop      {r4-r7, pc}

.size encerrarBib, .-encerrarBib

.global write_pixel
.type write_pixel, %function
write_pixel:
    push    {r4-r6, lr}
    ldr     r4, =FPGA_ADRS
    ldr     r4, [r4]

    cmp     r0, #VRAM_MAX_ADDR
    bhs     .ADDR_INVALID     

.ASM_DATA:
    mov     r2, #STORE_OPCODE    @ [2:0] = opcode (0x02)

    lsl     r3, r0, #3           @ [19:3] = address
    orr     r2, r2, r3

    mov     r3, #1
    lsl     r3, r3, #20
    orr     r2, r2, r3

    lsl     r3, r1, #21          @ [28:21] = data
    orr     r2, r2, r3

    str     r2, [r4, #PIO_INSTRUCT]
    dmb     sy                  

    mov     r2, #1
    str     r2, [r4, #PIO_ENABLE]

    mov     r2, #0
    str     r2, [r4, #PIO_ENABLE]

    mov     r5, #0x3000

.WAIT_LOOP:
    ldr     r2, [r4, #PIO_FLAGS]

    tst     r2, #FLAG_DONE_MASK
    bne     .CHECK_ERROR       

    subs    r5, r5, #1
    bne     .WAIT_LOOP

    # Timeout
    mov     r0, #0
    b       .EXIT
.CHECK_ERROR:
    tst     r2, #FLAG_ERROR_MASK
    bne     .HW_ERROR

    mov     r0, #0               
    b       .EXIT
.ADDR_INVALID:
    mov     r0, #-1
    b       .EXIT
.HW_ERROR:
    mov     r0, #-3

.EXIT:
    pop     {r4-r6, pc}
.size write_pixel, .-write_pixel

.global Vizinho_Prox
.type Vizinho_Prox, %function
Vizinho_Prox:

    push    {r7, lr}
    sub     sp, sp, #0      

    ldr     r3, =FPGA_ADRS
    ldr     r3, [r3]          

    movs    r2, #3
    str     r2, [r3, #PIO_INSTRUCT]

    movs    r2, #1
    str     r2, [r3, #PIO_ENABLE]

    movs    r2, #0
    str     r2, [r3, #PIO_ENABLE]

    add     sp, sp, #0
    pop     {r7, pc}
.size Vizinho_Prox, .-Vizinho_Prox


.global Replicacao 
.type Replicacao, %function 

Replicacao:
    push    {r7, lr} 
    sub     sp, sp, #0 

    ldr     r3, =FPGA_ADRS 
    ldr     r3, [r3] 

    movs    r2, #4                   
    str     r2, [r3, #PIO_INSTRUCT] 

    movs    r2, #1     
    str     r2, [r3, #PIO_ENABLE] 

    movs    r2, #0     
    str     r2, [r3, #PIO_ENABLE] 

    add     sp, sp, #0 
    pop     {r7, pc} 

.size Replicacao, .-Replicacao


.global Decimacao 
.type Decimacao, %function 

Decimacao:
    push    {r7, lr} 
    sub     sp, sp, #0 

    ldr     r3, =FPGA_ADRS 
    ldr     r3, [r3] 

    movs    r2, #6                      
    str     r2, [r3, #PIO_INSTRUCT] 

    movs    r2, #1     
    str     r2, [r3, #PIO_ENABLE] 

    movs    r2, #0     
    str     r2, [r3, #PIO_ENABLE] 

    add     sp, sp, #0 
    pop     {r7, pc} 

.size Decimacao, .-Decimacao


.global Media 
.type Media, %function 

Media:
    push    {r7, lr} 
    sub     sp, sp, #0 

    ldr     r3, =FPGA_ADRS 
    ldr     r3, [r3] 

    movs    r2, #5                     
    str     r2, [r3, #PIO_INSTRUCT] 

    movs    r2, #1     
    str     r2, [r3, #PIO_ENABLE]    

    movs    r2, #0     
    str     r2, [r3, #PIO_ENABLE] 

    add     sp, sp, #0 
    pop     {r7, pc} 

.size Media, .-Media

.global Reset
.type Reset, %function
Reset:
    push    {r7, lr}

    sub     sp, sp, #0

    ldr     r3, =FPGA_ADRS
    ldr     r3, [r3]

    movs    r2, #7                 @ Opcode 111 (binário) = 7 (decimal)
    str     r2, [r3, #PIO_INSTRUCT]

    movs    r2, #1
    str     r2, [r3, #PIO_ENABLE]

    movs    r2, #0
    str     r2, [r3, #PIO_ENABLE]

    add     sp, sp, #0
    pop     {r7, pc}
.size Reset, .-Reset

.global Flag_Done
.type Flag_Done, %function
Flag_Done: 
    push    {r7, lr}
    
    ldr     r3, =FPGA_ADRS
    ldr     r3, [r3]
    
    ldr     r0, [r3, #PIO_FLAGS]      
    and     r0, r0, #FLAG_DONE_MASK   
    
    pop     {r7, pc}
.size Flag_Done, .-Flag_Done

.global Flag_Error
.type Flag_Error, %function
Flag_Error: 
    push    {r7, lr}
    
    ldr     r3, =FPGA_ADRS
    ldr     r3, [r3]
    
    ldr     r0, [r3, #PIO_FLAGS]      
    and     r0, r0, #FLAG_ERROR_MASK   
    
    pop     {r7, pc}
.size Flag_Error, .-Flag_Error

.global Flag_Max
.type Flag_Max, %function
Flag_Max: 
    push    {r7, lr}
    
    ldr     r3, =FPGA_ADRS
    ldr     r3, [r3]
    
    ldr     r0, [r3, #PIO_FLAGS]      
    and     r0, r0, #FLAG_ZOOM_Max_MASK   
    
    pop     {r7, pc}
.size Flag_Max, .-Flag_Max

.global Flag_Min
.type Flag_Min, %function
Flag_Min: 
    push    {r7, lr}
    
    ldr     r3, =FPGA_ADRS
    ldr     r3, [r3]
    
    ldr     r0, [r3, #PIO_FLAGS]      
    and     r0, r0, #FLAG_ZOOM_Min_MASK   
    
    pop     {r7, pc}
.size Flag_Min, .-Flag_Min