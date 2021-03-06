#ifndef _APP_H
#define _APP_H


// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "system_config.h"
#include "system_definitions.h"
#include "mouse.h"
#include "i2c_master_noint.h"
#include "ST7735.h" 

// *****************************************************************************
// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application States
  Summary:
    Application states enumeration
  Description:
    This enumeration defines the valid application states.  These states
    determine the behavior of the application at various times.
*/

typedef enum
{
	/* Application's state machine's initial state. */
	APP_STATE_INIT=0,

	/* Application waits for configuration in this state */
    APP_STATE_WAIT_FOR_CONFIGURATION,

    /* Application runs mouse emulation in this state */
    APP_STATE_MOUSE_EMULATE,

    /* Application error state */
    APP_STATE_ERROR

} APP_STATES;


// *****************************************************************************
/* Application Data
  Summary:
    Holds application data
  Description:
    This structure holds the application's data.
  Remarks:
    Application strings and buffers are be defined outside this structure.
 */

typedef struct
{
    /* The application's current state */
    APP_STATES state;

    /* device layer handle returned by device layer open function */
    USB_DEVICE_HANDLE  deviceHandle;

    /* Is device configured */
    bool isConfigured;

    /* Mouse x coordinate*/
    MOUSE_COORDINATE xCoordinate;

    /* Mouse y coordinate*/
    MOUSE_COORDINATE yCoordinate;

    /* Mouse buttons*/
    MOUSE_BUTTON_STATE mouseButton[MOUSE_BUTTON_NUMBERS];

    /* HID instance associated with this app object*/
    SYS_MODULE_INDEX hidInstance;

    /* Transfer handle */
    USB_DEVICE_HID_TRANSFER_HANDLE reportTransferHandle;

    /* Device Layer System Module Object */
    SYS_MODULE_OBJ deviceLayerObject;

    /* USB HID active Protocol */
    uint8_t activeProtocol;

    /* USB HID current Idle */
    uint8_t idleRate;

    /* Tracks the progress of the report send */
    bool isMouseReportSendBusy;

    /* Flag determines SOF event has occured */
    bool sofEventHasOccurred;

    /* SET IDLE timer */
    uint16_t setIdleTimer;

} APP_DATA;


// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Routines
// *****************************************************************************
// *****************************************************************************
/* These routines are called by drivers when certain events occur.
*/

	
// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )
  Summary:
     MPLAB Harmony application initialization routine.
  Description:
    This function initializes the Harmony application.  It places the 
    application in its initial state and prepares it to run so that its 
    APP_Tasks function can be called.
  Precondition:
    All other system initialization routines should be called before calling
    this routine (in "SYS_Initialize").
  Parameters:
    None.
  Returns:
    None.
  Example:
    <code>
    APP_Initialize();
    </code>
  Remarks:
    This routine must be called from the SYS_Initialize function.
*/

void APP_Initialize ( void );


/*******************************************************************************
  Function:
    void APP_Tasks ( void )
  Summary:
    MPLAB Harmony Demo application tasks function
  Description:
    This routine is the Harmony Demo application's tasks function.  It
    defines the application's state machine and core logic.
  Precondition:
    The system and application initialization ("SYS_Initialize") should be
    called before calling this.
  Parameters:
    None.
  Returns:
    None.
  Example:
    <code>
    APP_Tasks();
    </code>
  Remarks:
    This routine must be called from SYS_Tasks() routine.
 */

void APP_Tasks ( void );

void IMU_init(void);
void i2c_read_multiple(unsigned char, unsigned char, unsigned char *, int);

#endif /* _APP_H */
/*******************************************************************************
 End of File
 */
