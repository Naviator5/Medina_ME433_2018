// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include <stdio.h>
#include <xc.h>

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

#define LSM6DS33 0b1101011

uint8_t APP_MAKE_BUFFER_DMA_READY dataOut[APP_READ_BUFFER_SIZE];
uint8_t APP_MAKE_BUFFER_DMA_READY readBuffer[APP_READ_BUFFER_SIZE];
int len, i = 0;
int rawdata[100], MAFdata[100], FIRdata[100], IIRdata[100]; // declare data arrays
int ii = 0;
int sum = 0;       // summation variable for MAF filter
int j = 0;         // 2nd index variable for FIR filter
float weights[6] = [0.0264, 0.1405, 0.3331, 0.3331, 0.1405, 0.0264]; // FIR weights
int dataFlag = 0;
int startTime = 0; // to remember the loop time

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

/* TODO:  Add any necessary callback functions.
 */

/*******************************************************
 * USB CDC Device Events - Application Event Handler
 *******************************************************/

USB_DEVICE_CDC_EVENT_RESPONSE APP_USBDeviceCDCEventHandler
(
        USB_DEVICE_CDC_INDEX index,
        USB_DEVICE_CDC_EVENT event,
        void * pData,
        uintptr_t userData
        ) {
    APP_DATA * appDataObject;
    appDataObject = (APP_DATA *) userData;
    USB_CDC_CONTROL_LINE_STATE * controlLineStateData;

    switch (event) {
        case USB_DEVICE_CDC_EVENT_GET_LINE_CODING:

            /* This means the host wants to know the current line
             * coding. This is a control transfer request. Use the
             * USB_DEVICE_ControlSend() function to send the data to
             * host.  */

            USB_DEVICE_ControlSend(appDataObject->deviceHandle,
                    &appDataObject->getLineCodingData, sizeof (USB_CDC_LINE_CODING));

            break;

        case USB_DEVICE_CDC_EVENT_SET_LINE_CODING:

            /* This means the host wants to set the line coding.
             * This is a control transfer request. Use the
             * USB_DEVICE_ControlReceive() function to receive the
             * data from the host */

            USB_DEVICE_ControlReceive(appDataObject->deviceHandle,
                    &appDataObject->setLineCodingData, sizeof (USB_CDC_LINE_CODING));

            break;

        case USB_DEVICE_CDC_EVENT_SET_CONTROL_LINE_STATE:

            /* This means the host is setting the control line state.
             * Read the control line state. We will accept this request
             * for now. */

            controlLineStateData = (USB_CDC_CONTROL_LINE_STATE *) pData;
            appDataObject->controlLineStateData.dtr = controlLineStateData->dtr;
            appDataObject->controlLineStateData.carrier = controlLineStateData->carrier;

            USB_DEVICE_ControlStatus(appDataObject->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);

            break;

        case USB_DEVICE_CDC_EVENT_SEND_BREAK:

            /* This means that the host is requesting that a break of the
             * specified duration be sent. Read the break duration */

            appDataObject->breakData = ((USB_DEVICE_CDC_EVENT_DATA_SEND_BREAK *) pData)->breakDuration;

            /* Complete the control transfer by sending a ZLP  */
            USB_DEVICE_ControlStatus(appDataObject->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);

            break;

        case USB_DEVICE_CDC_EVENT_READ_COMPLETE:

            /* This means that the host has sent some data*/
            appDataObject->isReadComplete = true;
            break;

        case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_RECEIVED:

            /* The data stage of the last control transfer is
             * complete. For now we accept all the data */

            USB_DEVICE_ControlStatus(appDataObject->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);
            break;

        case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_SENT:

            /* This means the GET LINE CODING function data is valid. We dont
             * do much with this data in this demo. */
            break;

        case USB_DEVICE_CDC_EVENT_WRITE_COMPLETE:

            /* This means that the data write got completed. We can schedule
             * the next read. */

            appDataObject->isWriteComplete = true;
            break;

        default:
            break;
    }

    return USB_DEVICE_CDC_EVENT_RESPONSE_NONE;
}

/***********************************************
 * Application USB Device Layer Event Handler.
 ***********************************************/
void APP_USBDeviceEventHandler(USB_DEVICE_EVENT event, void * eventData, uintptr_t context) {
    USB_DEVICE_EVENT_DATA_CONFIGURED *configuredEventData;

    switch (event) {
        case USB_DEVICE_EVENT_SOF:

            /* This event is used for switch debounce. This flag is reset
             * by the switch process routine. */
            appData.sofEventHasOccurred = true;
            break;

        case USB_DEVICE_EVENT_RESET:

            /* Update LED to show reset state */

            appData.isConfigured = false;

            break;

        case USB_DEVICE_EVENT_CONFIGURED:

            /* Check the configuration. We only support configuration 1 */
            configuredEventData = (USB_DEVICE_EVENT_DATA_CONFIGURED*) eventData;
            if (configuredEventData->configurationValue == 1) {
                /* Update LED to show configured state */

                /* Register the CDC Device application event handler here.
                 * Note how the appData object pointer is passed as the
                 * user data */

                USB_DEVICE_CDC_EventHandlerSet(USB_DEVICE_CDC_INDEX_0, APP_USBDeviceCDCEventHandler, (uintptr_t) & appData);

                /* Mark that the device is now configured */
                appData.isConfigured = true;

            }
            break;

        case USB_DEVICE_EVENT_POWER_DETECTED:

            /* VBUS was detected. We can attach the device */
            USB_DEVICE_Attach(appData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_POWER_REMOVED:

            /* VBUS is not available any more. Detach the device. */
            USB_DEVICE_Detach(appData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_SUSPENDED:

            /* Switch LED to show suspended state */
            break;

        case USB_DEVICE_EVENT_RESUMED:
        case USB_DEVICE_EVENT_ERROR:
        default:
            break;
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

/*****************************************************
 * This function is called in every step of the
 * application state machine.
 *****************************************************/

bool APP_StateReset(void) {
    /* This function returns true if the device
     * was reset  */

    bool retVal;

    if (appData.isConfigured == false) {
        appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
        appData.readTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
        appData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
        appData.isReadComplete = true;
        appData.isWriteComplete = true;
        retVal = true;
    } else {
        retVal = false;
    }

    return (retVal);
}

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

void APP_Initialize(void) {
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;

    /* Device Layer Handle  */
    appData.deviceHandle = USB_DEVICE_HANDLE_INVALID;

    /* Device configured status */
    appData.isConfigured = false;

    /* Initial get line coding state */
    appData.getLineCodingData.dwDTERate = 9600;
    appData.getLineCodingData.bParityType = 0;
    appData.getLineCodingData.bParityType = 0;
    appData.getLineCodingData.bDataBits = 8;

    /* Read Transfer Handle */
    appData.readTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;

    /* Write Transfer Handle */
    appData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;

    /* Intialize the read complete flag */
    appData.isReadComplete = true;

    /*Initialize the write complete flag*/
    appData.isWriteComplete = true;

    /* Reset other flags */
    appData.sofEventHasOccurred = false;
    //appData.isSwitchPressed = false;

    /* Set up the read buffer */
    appData.readBuffer = &readBuffer[0];

    /* PUT YOUR LCD, IMU, AND PIN INITIALIZATIONS HERE */
    //initializations
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
    
    //IMU WHOAMI check
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
    
    
    /* STARTTIME*/
    startTime = _CP0_GET_COUNT();
}

/******************************************************************************
  Function:
    void APP_Tasks ( void )
  Remarks:
    See prototype in app.h.
 */

void APP_Tasks(void) {
    /* Update the application state machine based
     * on the current state */

    switch (appData.state) {
        case APP_STATE_INIT:

            /* Open the device layer */
            appData.deviceHandle = USB_DEVICE_Open(USB_DEVICE_INDEX_0, DRV_IO_INTENT_READWRITE);

            if (appData.deviceHandle != USB_DEVICE_HANDLE_INVALID) {
                /* Register a callback with device layer to get event notification (for end point 0) */
                USB_DEVICE_EventHandlerSet(appData.deviceHandle, APP_USBDeviceEventHandler, 0);

                appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            } else {
                /* The Device Layer is not ready to be opened. We should try
                 * again later. */
            }

            break;

        case APP_STATE_WAIT_FOR_CONFIGURATION:

            /* Check if the device was configured */
            if (appData.isConfigured) {
                /* If the device is configured then lets start reading */
                appData.state = APP_STATE_SCHEDULE_READ;
            }
            break;

        case APP_STATE_SCHEDULE_READ:

            if (APP_StateReset()) {
                break;
            }

            /* If a read is complete, then schedule a read
             * else wait for the current read to complete */

            appData.state = APP_STATE_WAIT_FOR_READ_COMPLETE;
            if (appData.isReadComplete == true) {
                appData.isReadComplete = false;
                appData.readTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;

                USB_DEVICE_CDC_Read(USB_DEVICE_CDC_INDEX_0,
                        &appData.readTransferHandle, appData.readBuffer,
                        APP_READ_BUFFER_SIZE);

                        /* AT THIS POINT, appData.readBuffer[0] CONTAINS A LETTER
                        THAT WAS SENT FROM THE COMPUTER */
                        /* YOU COULD PUT AN IF STATEMENT HERE TO DETERMINE WHICH LETTER
                        WAS RECEIVED (USUALLY IT IS THE NULL CHARACTER BECAUSE NOTHING WAS
                      TYPED) */
                if(appData.readBuffer[0] == 'r') {
                    sum = 0;                        // reset MAF sum
                    for (ii = 0; ii < 100; ii++) {
                        rawdata[ii] = 0;
                        MAFdata[ii] = 0;
                        FIRdata[ii] = 0;
                        IIRdata[ii] = 0;
                    }
                    dataFlag = 1;  // send data (after it's collected)
                }

                if (appData.readTransferHandle == USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID) {
                    appData.state = APP_STATE_ERROR;
                    break;
                }
            }

            break;

        case APP_STATE_WAIT_FOR_READ_COMPLETE:
        case APP_STATE_CHECK_TIMER:

            if (APP_StateReset()) {
                break;
            }

            /* Check if a character was received or a switch was pressed.
             * The isReadComplete flag gets updated in the CDC event handler. */

             /* WAIT FOR 100 HZ TO PASS */
            if (appData.isReadComplete || _CP0_GET_COUNT() - startTime > (48000000 / 2 / 400)) {
                appData.state = APP_STATE_SCHEDULE_WRITE;
            }


            break;


        case APP_STATE_SCHEDULE_WRITE:

            if (APP_StateReset()) {
                break;
            }

            /* Setup the write */

            appData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
            appData.isWriteComplete = false;
            appData.state = APP_STATE_WAIT_FOR_WRITE_COMPLETE;

            /* PUT THE TEXT YOU WANT TO SEND TO THE COMPUTER IN dataOut
            AND REMEMBER THE NUMBER OF CHARACTERS IN len */
            /* THIS IS WHERE YOU CAN READ YOUR IMU, PRINT TO THE LCD, ETC */
            
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
            sprintf(lcd,"AZ = %d   ",accelZ);
            drawString(10,40,lcd,WHITE,BLACK);
            
            
            /* Filter accelZ data */
            rawdata[i] = accelZ;
            
            // MAF
            sum += rawdata[i];
            MAFdata[i] = sum/(i+1);
            
            // FIR
            for (j = 0; j < 6; j++) {
                FIRdata[i] += weights[j]*rawdata[i-j];
                if (i-j < 0) {
                    rawdata[i-j] = 0;
                }
            }
            
            // IIR
            if (i == 0) {rawdata[i-1] = 0;}
            IIRdata[i] = 0.9*rawdata[i-1] + 0.1*rawdata[i];
            if (i == 100) {rawdata[i] = IIRdata[i];}
            
            
            /* Send Data to computer (only if 'r' is received, i.e. flag = 1) */
            if (dataFlag == 1) {
                len = sprintf(dataOut, "%d %d %d %d %d\r\n", i, accelZ, MAFdata[i], FIRdata[i], IIRdata[i]);
                i++;             // increment the index 
                if (i == 100) {  // after 100 data points, stop sending data
                    i = 0;
                    dataFlag = 0;
                }
            } else {
                len = 1;
                dataOut[0] = 0;
            }
            USB_DEVICE_CDC_Write(USB_DEVICE_CDC_INDEX_0,
                        &appData.writeTransferHandle, dataOut, len,
                        USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
            
            
            /* Reset Timer for accurate delays */
            startTime = _CP0_GET_COUNT(); 
            break;

        case APP_STATE_WAIT_FOR_WRITE_COMPLETE:

            if (APP_StateReset()) {
                break;
            }

            /* Check if a character was sent. The isWriteComplete
             * flag gets updated in the CDC event handler */

            if (appData.isWriteComplete == true) {
                appData.state = APP_STATE_SCHEDULE_READ;
            }

            break;

        case APP_STATE_ERROR:
            break;
        default:
            break;
    }
}

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