// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/time.h>

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef enum
{
    CMD_TX, // Commands sent by the transmitter
    CMD_RX, // Commands sent by the receiver
    ANS_TX, // Replies sent by the transmitter
    ANS_RX, // Replies sent by the receiver

} AddressFieldType;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

typedef struct
{
    bool stuffed;
    unsigned char byte1;
    unsigned char byte2;

} StuffingAux;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer

// 1000 - 3 from PA means each image chunk will have 997 bytes
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1
#define FLAG 0x7E
#define ESC 0x7D

// ADDRESS FIELD TYPES
#define CMD_TX 0x03
#define CMD_RX 0x01
#define ANS_TX 0x01
#define ANS_RX 0x03

// SUPERVISION FRAMES
#define C_SET 0x03
#define C_UA 0x07
#define C_RR0 0x05
#define C_RR1 0x85
#define C_REJ0 0x01
#define C_REJ1 0x81
#define C_DISC 0x0B

// INFORMATION FRAMES
#define C_FRAME0 0x00
#define C_FRAME1 0x40

// STATE MACHINES

typedef enum {
    Start,
    FlagRCV,
    ARCV,
    CRCV,
    BccOK,
    Data,
    Destuffing
} stateNames;

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Auxiliary function that sends a frame and waits for the appropriate RR response from the receiver
// Returns 1 if it got a succesful response, 0 if the frame got rejected or -1 if no response was read
int llwriteSendFrame(unsigned char *frame, int frameSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics, LinkLayer connectionParameters);

// Escapes flags and escape characters in byte
// Returns a struct containg a bool, that indicates if the byte required stuffing or not, and the corresponding stuffed sequence
StuffingAux stuffByte(unsigned char byte);

//Creates a supervision frame for receiver replies to information frames.
// Returns the number of bytes written (5) in success or -1 if theres any errors
int sendRxResponse(unsigned char C);

// Handles alarm interrupts
void alarmHandler(int signal);

#endif // _LINK_LAYER_H_
