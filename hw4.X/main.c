#include <xc.h>           // processor SFR definitions
#include <sys/attribs.h>  // __ISR macro
#include <math.h>        // math library, to have sin function


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


/* Other Definitions */
#define CS LATBbits.LATB7 // chip select pin for the DAC 


/* Helper Function Prototypes */
unsigned char spi_io(unsigned char);  // send a byte via SPI
void spi_init(void);                  // initiate SPI1
void setVoltage(char, int);           // set voltage value to send to DAC


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

    spi_init();
    __builtin_enable_interrupts();

    int i = 0;        // counter variable 
    float val = 0.0;  // value variable for triangle wave
    
    while(1) {
        _CP0_SET_COUNT(0);
        // sin wave, period = 100 ms (10 Hz)
        float sine = 512.0 + 512.0*sin((i*2.0*3.14/1000.0)*10.0); // for first run (i = 0), f = 512 
                                                                  // --> wave starts at 1.65V 
                                                                  // as long as i is multiple of 100, it doesn't matter
                                                                  // (sin(10s*2pi) = sin(2pi))
        
        // triangle wave, period = 200 ms (5 Hz)
        float tri = 0.0 + (1023.0/100.0)*val;
        if (i<100.0) {
            val = val + 1.0;
        } else if (100.0<=i && i<200.0) {
            val = val - 1.0;
        } else if (i == 200.0) {
            val = 0.0;
            tri = 0.0;
        }
        
        i++;
        if (i == 200) {i = 0;}
        
        // send sin wave to channel A and triangle wave to channel B
        setVoltage(0,sine); 
        setVoltage(1,tri);
                
        while(_CP0_GET_COUNT() < 24000) {;} // wait 1 ms to get 1 kHz desired sampling rate
    }
    return 0;
}


/* Helper Functions */
unsigned char spi_io(unsigned char read) {
    SPI1BUF = read;                       // send the command
    while(!SPI1STATbits.SPIRBF) {Nop();}  // wait for the response
    SPI1BUF;                              // garbage was transferred (old val), ignore it
    SPI1BUF = 5;                          // write garbage, but the read will have the data
    while (!SPI1STATbits.SPIRBF) {Nop();}
    return SPI1BUF;
}

void spi_init() {
    // Pin Function Selections
    SDI1Rbits.SDI1R = 0b0100; // pin B8 for SDI1
    RPA1Rbits.RPA1R = 0b0011; // pin A1 for SDO1
    RPB7Rbits.RPB7R = 0b0011; // pin B7 for SS1
    
    // set chosen CS pin as output
    // to indicate to DAC when command is beginning (cleared lo) and ending (set hi)
    TRISBbits.TRISB7 = 0;
    CS = 1;  // no command given at startup
    
    // SPI initialization for DAC chip
    SPI1CON = 0;                        // turn off and reset SPI1 module
    SPI1BUF;                            // clear the rx buffer by reading it
    SPI1BRG = 0x1;                      // set bit rate to less than 20 MHz: 48/2*(_1_+1) = 12 MHz
    SPI1STATbits.SPIROV = 0;            // clear the overflow bit
    SPI1CONbits.CKE = 1;                // data changes when clocks goes hi to lo (since CKP = 0)
    SPI1CONbits.MSTEN = 1;              // master mode
    SPI1CONbits.ON = 1;                 // turn SPI on
}

void setVoltage(char channel, int voltage) {
    unsigned short v = 0; // v is 16 bits, represents DAC value
    
    v = channel << 15;                        // set bit 15: which out (A or B) to use
    v |= 0b0111000000000000;                  // set bits 14-12: buffered, gain = 1, no shutdown
    v |= ((voltage & 0b1111111111) << 2);     // set bits 11-2: 10-bit voltage value
                                              // (last 2 bits of MCP4912 are N/A)
    
    CS = 0;         // begin command to DAC
    spi_io(v>>8);   // first byte (first half)
    spi_io(v<<8);   // second byte (second half)
    CS = 1;         // end command
}   