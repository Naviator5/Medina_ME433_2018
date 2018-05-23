#include <xc.h>
#include <stdio.h>

/* PIC32 Configurations */
// DEVCFG0
#pragma config DEBUG = OFF // no debugging
#pragma config JTAGEN = OFF // no jtag
#pragma config ICESEL = ICS_PGx1 // use PGEC1/PGED1
#pragma config PWP = OFF // no write protect
#pragma config BWP = OFF // no boot write protect (boot flash is writeable)
#pragma config CP = OFF // no code protect

// DEVCFG1
#pragma config FNOSC = PRIPLL // use primary oscillator with PLL
#pragma config FSOSCEN = OFF // turn off secondary oscillator
#pragma config IESO = OFF // no switching clocks
#pragma config POSCMOD = HS // high speed crystal mode
#pragma config OSCIOFNC = OFF // disable secondary OSC
#pragma config FPBDIV = DIV_1 // divide sysclk freq by 1 for peripheral bus clock
#pragma config FCKSM = CSDCMD // clock switching disabled
#pragma config WDTPS = PS1048576 // use slowest wdt (1:1048576)
#pragma config WINDIS = OFF // wdt no window mode
#pragma config FWDTEN = OFF // wdt disabled
#pragma config FWDTWINSZ = WINSZ_25 // wdt window at 25%

// DEVCFG2 - get the sysclk clock to 48MHz from the 8MHz crystal
#pragma config FPLLIDIV = DIV_2 // divide input clock by 2 to get 4MHz
#pragma config FPLLMUL = MUL_24 // multiply clock by 24 after FPLLIDIV to get 96MHz
#pragma config FPLLODIV = DIV_2 // then divide by 2 after FPLLMUL to get 48MHz
#pragma config UPLLIDIV = DIV_2 // divider for the 8MHz input clock, then multiplied by 12 to get 48MHz for USB
#pragma config UPLLEN = ON // USB clock on

// DEVCFG3
#pragma config USERID = 00000000 // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = OFF // allow multiple module reconfigurations
#pragma config IOL1WAY = OFF // allow multiple pin reconfigurations
#pragma config FUSBIDIO = ON // USB pins controlled by USB module
#pragma config FVBUSONIO = ON // USB BUSON controlled by USB module


/* Main Function */
int main(void) {
    __builtin_disable_interrupts();
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583); // CP0 CONFIG register: kseg0 is cacheable (0x3)
    BMXCONbits.BMXWSDRM = 0x0;                                   // 0 data RAM access wait states
    INTCONbits.MVEC = 0x1;                                       // enable multi vector interrupts
    DDPCONbits.JTAGEN = 0;                                       // disable JTAG to get pins back
    TRISAbits.TRISA4 = 0;                                        // set up A4 LED as output
    LATAbits.LATA4 = 0;                                          // --> initially low
    TRISBbits.TRISB4 = 1;                                        // set up B4 as input
    __builtin_enable_interrupts();
    
    while(1) {
        _CP0_SET_COUNT(0);
        LATAbits.LATA4 = !LATAbits.LATA4; 
        while(_CP0_GET_COUNT() < 12000000) {Nop();}
        LATAbits.LATA4 = !LATAbits.LATA4; 
        while(_CP0_GET_COUNT() < 24000000) {Nop();} 
        while(!PORTBbits.RB4) {Nop();}
    }
    return 0;
}