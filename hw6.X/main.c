#include <xc.h>           // processor SFR definitions
#include <sys/attribs.h>  // __ISR macro
#include <stdio.h>        // for sprintf function
#include "ST7735.h"       // LCD library


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


/* Function Prototypes */
void drawString(unsigned short, unsigned short, char *, unsigned short, unsigned short);
void drawChar(unsigned short, unsigned short, char, unsigned short, unsigned short); 
void drawProgressBar(unsigned short, unsigned short, unsigned short, \
 unsigned short, unsigned short, unsigned short, unsigned short);


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
    
    LCD_init(); // initialize LCD/SPI communication
    __builtin_enable_interrupts();
    
    LCD_clearScreen(BLACK); // clear the screen to background color of choice
    
    int i = 0;
    while(1) {
        _CP0_SET_COUNT(0);
        char hello[30];
        char framerate[20];
      
        int fps_start = _CP0_GET_COUNT();  // timer for 1 frame to display 
        sprintf(hello, "Hello World! %d   ",i);
        drawString(28, 32, hello, WHITE, BLACK); 
        drawProgressBar(14, 45, 4, i, BLUE, 100-i, WHITE);
        
        // calculate frames per second and display on screen 
        float fps = 24000000.0/(_CP0_GET_COUNT() - fps_start);
        sprintf(framerate, "FPS = %3.2f",fps);
        drawString(14, 60, framerate, WHITE, BLACK); 
        
        // update counter/progress bar
        i++;
        if (i == 100) {i = 0;}
                
        while(_CP0_GET_COUNT() < 2400000) {Nop();} // update speed = 10 Hz
    }
    return 0;
}


/* Helper Functions */
void drawChar(unsigned short x, unsigned short y, char message, \
 unsigned short color1, unsigned short color2) { 
    char row = message - 0x20; // variables for rows and columns of ASCII array
    int col = 0;
    
    // loop through array 
    for(col = 0; col < 5; col++) {
        char pixels = ASCII[row][col];
        int j = 0;
        
        for(j = 0; j < 8; j++) {
            if ((x+col) < 128 && (y+j) < 160) {    // condition to remain on 128x160 screen
                if ( ((pixels >> j) & 1) == 1) {   // if = 1, fill in the pixel w character color
                LCD_drawPixel(x+col, y+j, color1);
                } else {LCD_drawPixel(x+col, y+j, color2);} // otherwise leave as background color
            }
        }
    }
}

void drawString(unsigned short x, unsigned short y, char *message, \
 unsigned short color1, unsigned short color2) {
    int i = 0;
    
    while (message[i]) {
        drawChar(x+(i*5), y, message[i], color1, color2);
        i++;
    }
}

void drawProgressBar(unsigned short x, unsigned short y, unsigned short h, \
 unsigned short len1, unsigned short color1, unsigned short len2, unsigned short color2) {
    // make background for bar (across, then down)
    int i = 0, j = 0;
    
    // this part controls the color that fills the bar (when i = 0, bar is all background)
    for (j = 0; j <= h; j++) { 
        for (i = 0; i <= len1; i++) {
            LCD_drawPixel(x+i,y+j,color1);
        }
    }
    
    // this part controls the background color
    for (j = 0; j <= h; j++) {
        for (i = 0; i <= len2; i++) {
            LCD_drawPixel((x+100)-i,y+j,color2); 
        }
    }      
    
}
