#include <xc.h>
#include "i2c_master_noint.h"

// I2C Master utilities, 400 kHz, using polling rather than interrupts
// The functions must be callled in the correct order as per the I2C protocol
// I2C pins need pull-up resistors, 2k-10k

void i2c_master_setup(void) {
  // turn off I2C2 analog
  ANSELBbits.ANSB2 = 0;
  ANSELBbits.ANSB3 = 0;
  
  I2C2BRG = 53;         // I2CxBRG = [1/(2*400000) - (100E-9)]*48000000 - 2 = 53.2
                        // (let PGD = 100 ns, desired Fsck = 400 kHz; PB = 48M)
  I2C2CONbits.ON = 1;   // turn on I2C2 module
}

// Start a transmission on the I2C bus
void i2c_master_start(void) {
    I2C2CONbits.SEN = 1;             // send the start bit
    while(I2C2CONbits.SEN) {Nop();}  // wait for the start bit to be sent
}

void i2c_master_restart(void) {
    I2C2CONbits.RSEN = 1;             // send a restart
    while(I2C2CONbits.RSEN) {Nop();}  // wait for the restart to clear
}

void i2c_master_send(unsigned char byte) { // send a byte to slave
  I2C2TRN = byte;                      // if an address, bit 0 = 0 for write, 1 for read
  while(I2C2STATbits.TRSTAT) {Nop();}  // wait for the transmission to finish
  if(I2C2STATbits.ACKSTAT) {           // if this is high, slave has not acknowledged
    ;// ("I2C2 Master: failed to receive ACK\r\n");
  }
}

unsigned char i2c_master_recv(void) { // receive a byte from the slave
    I2C2CONbits.RCEN = 1;             // start receiving data
    while(!I2C2STATbits.RBF) {Nop();} // wait to receive the data
    return I2C2RCV;                   // read and return the data
}

void i2c_master_ack(int val) {        // sends ACK = 0 (slave should send another byte)
                                      // or NACK = 1 (no more bytes requested from slave)
    I2C2CONbits.ACKDT = val;          // store ACK/NACK in ACKDT
    I2C2CONbits.ACKEN = 1;            // send ACKDT
    while(I2C2CONbits.ACKEN) {Nop();} // wait for ACK/NACK to be sent
}

void i2c_master_stop(void) {          // send a STOP:
  I2C2CONbits.PEN = 1;                // comm is complete and master relinquishes bus
  while(I2C2CONbits.PEN) {Nop();}     // wait for STOP to complete
}