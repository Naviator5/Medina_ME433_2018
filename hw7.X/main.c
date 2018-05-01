#include <xc.h>
#include "i2c_master_noint.h"
#include <stdio.h>
#include "ST7735.h"
#include <math.h>

#define LSM6DS33 0b1101011 // chip address

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
void IMU_init(void); // initialize IMU accelerometer, gyroscope, multi-reader (includes temp)
void i2c_read_multiple(unsigned char, unsigned char, unsigned char *, int); 


/* Main Function */
int main(void) {
    __builtin_disable_interrupts();
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583); // CP0 CONFIG register: kseg0 is cacheable (0x3)
    BMXCONbits.BMXWSDRM = 0x0;                                   // 0 data RAM access wait states
    INTCONbits.MVEC = 0x1;                                       // enable multi vector interrupts
    DDPCONbits.JTAGEN = 0;                                       // disable JTAG to get pins back
    i2c_master_setup();                                          // set up I2C2 as master, at 400 kHz
    IMU_init();                                                  // initialize LSM6DS33
    LCD_init();                                                  // initialize LCD/SPI communication
    TRISAbits.TRISA4 = 0;                                        // set up green LED heartbeat check
    LATAbits.LATA4 = 0;   
    __builtin_enable_interrupts();
    
    // define data array and LCD string
    unsigned char imudata[14];
    char lcd[30];
    
    // setup LCD
    LCD_clearScreen(BLACK);
    drawHorizontalProgressBar(64,100,4,60,0,WHITE,60,MAGENTA);
    drawHorizontalProgressBar(4,100,4,60,0,WHITE,60,MAGENTA);
    drawVerticalProgressBar(62,40,4,60,0,WHITE,60,MAGENTA);
    drawVerticalProgressBar(62,100,4,60,0,WHITE,60,MAGENTA);
    
    // IMU WHOAMI check
    i2c_master_start();
    i2c_master_send((LSM6DS33 << 1) | 0); // notify via write
    i2c_master_send(0x0F); // WHOAMI register
    i2c_master_restart();
    i2c_master_send((LSM6DS33 << 1) | 1); // master is reading
    int whoami = (int) i2c_master_recv();
    i2c_master_ack(1);
    i2c_master_stop();
    sprintf(lcd,"WHOAMI = %d",whoami);
    drawString(10,10,lcd,WHITE,BLACK);
    
    while(1) {
        _CP0_SET_COUNT(0);
        LATAbits.LATA4 = !LATAbits.LATA4; // green LED heartbeat
     
        // get data and convert to shorts
        i2c_read_multiple(LSM6DS33,0x20,imudata,14); // 0x20 = temp_L register
        signed short temperature = (imudata[1] << 8) | imudata[0];   
        signed short gyroX = (imudata[3] << 8) | imudata[2];   
        signed short gyroY = (imudata[5] << 8) | imudata[4];   
        signed short gyroZ = (imudata[7] << 8) | imudata[6];   
        signed short accelX = (imudata[9] << 8) | imudata[8];   
        signed short accelY = (imudata[11] << 8) | imudata[10]; 
        signed short accelZ = (imudata[13] << 8) | imudata[12]; 
        
        // print data to LCD
        sprintf(lcd,"AX = %d   ",accelX);
        drawString(10,20,lcd,WHITE,BLACK);
        sprintf(lcd,"AY = %d   ",accelY);
        drawString(10,30,lcd,WHITE,BLACK); 
        
        // active LCD display (with progress bars)
        float pcntX = abs(accelX)/16000.0;  // put into % with respect to drawn bars (max in one direction = 16000 counts)
        float pcntY = abs(accelY)/16000.0;  // floats needed so that numbers less than 1 don't get rounded up/down
        
        if (accelX < 0) {
            drawHorizontalProgressBar(64,100,4,60,pcntX*60,WHITE,60-(pcntX*60),MAGENTA); // right bar, fills left --> right
        } else if (accelX > 0) {
            drawHorizontalProgressBar(4,100,4,60,60-(pcntX*60),MAGENTA,pcntX*60,WHITE);  // left bar, fills right --> left 
        } else {Nop();}
        if (accelY > 0) {
            drawVerticalProgressBar(62,40,4,60,60-(pcntY*60),MAGENTA,pcntY*60,WHITE);    // top bar, fills bottom --> top 
        } else if (accelY < 0) {
            drawVerticalProgressBar(62,100,4,60,pcntY*60,WHITE,60-(pcntY*60),MAGENTA);   // bottom bar, fills top --> bottom
        } else {Nop();}
        
        while(_CP0_GET_COUNT() < 1200000) {Nop();} // update speed = 20 Hz
    }
    return 0;
}


/* Helper Functions */
void IMU_init() {
    // accelerometer init: 1.66 kHz sample rate, 2g sensitivity, 100 Hz anti-aliasing LPF
    i2c_master_start();
    i2c_master_send((0b1101011 << 1) | 0); // chip address
    i2c_master_send(0x10); // CTRL1_XL register 
    i2c_master_send(0b10000010);
    i2c_master_stop();
    
    // gyroscope init: 1.66 kHz sample rate, 1000 dps sensitivity
    i2c_master_start();
    i2c_master_send((0b1101011 << 1) | 0); // chip address
    i2c_master_send(0x11); // CTRL2_G register 
    i2c_master_send(0b10001000);
    i2c_master_stop();
    
    // CTRL3_C: make IF_INC = 1 to read multiple registers in a row w/o specifying every address
    i2c_master_start();
    i2c_master_send((0b1101011 << 1) | 0); // chip address
    i2c_master_send(0x12); // CTRL3_C register 
    i2c_master_send(0b00000100);
    i2c_master_stop();
}

void i2c_read_multiple(unsigned char address, unsigned char reg, unsigned char *data, int length) {
    // set register to read from 
    i2c_master_start();
    i2c_master_send((address << 1) | 0); // notify via write
    i2c_master_send(reg); // 
    i2c_master_restart();
    
    // loop to get all data
    int i = 0;
    i2c_master_send((address << 1) | 1); // master is reading
    for (i = 0; i <= length-1; i++) {
        data[i] = i2c_master_recv();
        if (i < length-1) {
            i2c_master_ack(0);      // not done getting all the data
        } else {i2c_master_ack(1);} // done getting all data    
    }
    i2c_master_stop();
}