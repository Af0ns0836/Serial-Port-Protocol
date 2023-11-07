// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#include <math.h>

// SHOW FINAL STATISTICS

#define SHOW_STATISTICS TRUE

// CONTROL PACKET

#define CTRL_START 2
#define CTRL_END 3
#define T_SIZE 0
#define T_NAME 1

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);

// Auxiliary function to calculate how many bytes a number takes up
// Returns the number of bytes of n
unsigned int bytesToRepresent(long int n);

#endif // _APPLICATION_LAYER_H_
