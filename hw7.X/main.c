#include <xc.h>
#include "i2c_master_noint.h"
#include <stdio.h>
#include "ST7735.h"

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
    // set the CP0 CONFIG register to indicate that kseg0 is cacheable (0x3)
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583); 
    // 0 data RAM access wait states
    BMXCONbits.BMXWSDRM = 0x0;
    // enable multi vector interrupts
    INTCONbits.MVEC = 0x1;
    // disable JTAG to get pins back
    DDPCONbits.JTAGEN = 0;

    i2c_master_setup();            // set up I2C2 as master, at 400 kHz
    IMU_init();                    // initialize LSM6DS33
    LCD_init();                    // initialize LCD/SPI communication
    TRISAbits.TRISA4 = 0;          // set up green LED heartbeat check
    LATAbits.LATA4 = 0;   
    __builtin_enable_interrupts();
    
    // setup LCD
    LCD_clearScreen(BLACK);
    drawProgressBar(62,40,120,4,BLUE,0,WHITE);
    drawProgressBar(6,100,4,116,BLUE,0,WHITE);
    
    // define data arrays and string
    unsigned char *imudata[14];
    unsigned int IMU[14];
    char lcd[30];
    
    // IMU WHOAMI check
    i2c_master_start();
    i2c_master_send((LSM6DS33 << 1) | 0); // notify via write
    i2c_master_send(0x0F); // WHOAMI register
    i2c_master_restart();
    i2c_master_send((LSM6DS33 << 1) | 1); // master is reading
    char whoami = i2c_master_recv();
    i2c_master_ack(1);
    i2c_master_stop();
    sprintf(lcd,"WHOAMI = %c",whoami);
    drawString(10,10,lcd,WHITE,BLACK);
    
    while(1) {
        // green LED heartbeat
        _CP0_SET_COUNT(0);
        LATAbits.LATA4 = !LATAbits.LATA4; 
        while(_CP0_GET_COUNT() < 24000) {;} 
        LATAbits.LATA4 = !LATAbits.LATA4; 
        while(_CP0_GET_COUNT() < 48000) {;} 
     
        // get data and convert to shorts
        i2c_read_multiple(LSM6DS33,0x20,imudata,14); 
        IMU[0] = (imudata[0] << 8) | imudata[1];   // temperature
        IMU[1] = (imudata[2] << 8) | imudata[3];   // gyroX
        IMU[2] = (imudata[4] << 8) | imudata[5];   // gyroY
        IMU[3] = (imudata[6] << 8) | imudata[7];   // gyroZ
        IMU[4] = (imudata[8] << 8) | imudata[9];   // accelX
        IMU[5] = (imudata[10] << 8) | imudata[11]; // accelY
        IMU[6] = (imudata[12] << 8) | imudata[13]; // accelZ
        
        // print data to LCD
        sprintf(lcd,"AX = %d",IMU[4]);
        drawString(10,20,lcd,WHITE,BLACK);
        sprintf(lcd,"AY = %d",IMU[5]);
        drawString(10,30,lcd,WHITE,BLACK); 
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