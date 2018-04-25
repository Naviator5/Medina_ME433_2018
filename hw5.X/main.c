#include <xc.h>
#include "i2c_master_noint.h"

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


/* Helper Function Prototypes */
void initExpander(void);       // initialize the MCP23008
void setExpander(char, char);  // set an output pin
//char getExpander(void);      // read input pins


/* Main Function */
int main(void) {
    __builtin_disable_interrupts();
    // set the CP0 CONFIG register to indicate that kseg0 is cacheable (0x3)
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583);
    // 0 data RAM access wait states
    BMXCONbits.BMXWSDRM = 0x0;
    // enable multi vector interrupts
    INTCONbits.MVEC = 0x1;
    // disable JTAG to get pins back
    DDPCONbits.JTAGEN = 0;

    i2c_master_setup();            // set up I2C2 as master, at 400 kHz
    initExpander();                // initialize MCP23008
    TRISAbits.TRISA4 = 0;          // set up green LED to serve as comm check
    LATAbits.LATA4 = 0;   
    __builtin_enable_interrupts();
    
    while(1) {
        // blinking green LED to serve as comm check 
        _CP0_SET_COUNT(0);
        LATAbits.LATA4 = !LATAbits.LATA4; 
        while(_CP0_GET_COUNT() < 24000) {;} 
        LATAbits.LATA4 = !LATAbits.LATA4; 
        while(_CP0_GET_COUNT() < 48000) {;} 
    
        //setExpander(0,1); // test: turn on LED (set GP0 high) 
        /* if(getExpander() == 0x00) {setExpander(0,1);} else {setExpander(0,0);}*/
    }
    return 0;
}


/* Helper Functions */
void initExpander() {
    // IODIR: GP4-7 = inputs, GP0-3 = outputs
    i2c_master_start();
    i2c_master_send((0b0100101 << 1) | 0); // chip address, with 0 to indicate write
    i2c_master_send(0x00); // IODIR register
    i2c_master_send(0b11110000); 
    i2c_master_stop();
    
    // OLAT: set outputs low to start
    i2c_master_start();
    i2c_master_send((0b0100101 << 1) | 0); // chip address
    i2c_master_send(0x0A); // OLAT register
    i2c_master_send(0x01); // test: turn on LED by default
    i2c_master_stop();
}

void setExpander(char pin, char level) {
    // choose desired pin
    unsigned short p = 0;
    p = level << pin; // this will take level (1 or 0) and shift it to the proper position
    
    // set said pin high
    i2c_master_start();
    i2c_master_send((0b0100101 << 1) | 0); // chip address
    i2c_master_send(0x0A); // OLAT register
    i2c_master_send(p); 
    i2c_master_stop();
}

/*char getExpander() {
    char val = 0;
    
    i2c_master_start();
    i2c_master_send((0b0100101 << 1) | 0); // notify via write
    i2c_master_send(0x09); // GPIO register
    i2c_master_restart();
    i2c_master_send((0b0100101 << 1) | 1); // master is reading
    val = i2c_master_recv();
    i2c_master_ack(1);
    i2c_master_stop();
}*/