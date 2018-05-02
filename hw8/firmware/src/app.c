// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "app.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback functions. */

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


/* TODO:  Add any necessary local functions. */
#define LSM6DS33 0b1101011


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;

    
    /* TODO: Initialize your application's state machine and other parameters. */
    // initializations
    BMXCONbits.BMXWSDRM = 0x0;                                   // 0 data RAM access wait states
    INTCONbits.MVEC = 0x1;                                       // enable multi vector interrupts
    DDPCONbits.JTAGEN = 0;                                       // disable JTAG to get pins back
    i2c_master_setup();                                          // set up I2C2 as master, at 400 kHz
    IMU_init();                                                  // initialize LSM6DS33
    LCD_init();                                                  // initialize LCD/SPI communication
    TRISAbits.TRISA4 = 0;                                        // set up green LED heartbeat check
    LATAbits.LATA4 = 0;   
     
    // setup LCD
    LCD_clearScreen(BLACK);
    drawHorizontalProgressBar(64,100,4,60,0,WHITE,60,MAGENTA);
    drawHorizontalProgressBar(4,100,4,60,0,WHITE,60,MAGENTA);
    drawVerticalProgressBar(62,40,4,60,0,WHITE,60,MAGENTA);
    drawVerticalProgressBar(62,100,4,60,0,WHITE,60,MAGENTA);
    
    // IMU WHOAMI check
    char lcd[30];
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
}


void APP_Tasks ( void )
{

    /* Check the application's current state. */
    switch ( appData.state )
    {
        /* Application's initial state. */
        case APP_STATE_INIT:
        {
            bool appInitialized = true;
       
        
            if (appInitialized)
            {
            
                appData.state = APP_STATE_SERVICE_TASKS;
            }
            break;
        }

        case APP_STATE_SERVICE_TASKS:
        {
            _CP0_SET_COUNT(0);
            LATAbits.LATA4 = !LATAbits.LATA4; // green LED heartbeat
            
            // define data array and LCD string
            unsigned char imudata[14];
            char lcd[30];
            
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
            
            break;
        }

        /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}

/* Other Standalone Helper Functions */
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
 

/*******************************************************************************
 End of File
 */
