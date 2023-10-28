// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;


typedef enum {
    Start,
    FlagRCV,
    ARCV,
    CRCV,
    BccOK,
    Data,
    Stop
} stateNames;

typedef struct frameHeader {
    unsigned char A;
    unsigned char C;
} frameHeader;

//ROLE
#define NOT_DEFINED -1
#define TRANSMITTER 0
#define RECEIVER 1

//SIZE of maximum acceptable payload; maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

//CONNECTION deafault values
#define BAUDRATE_DEFAULT B38400
#define MAX_RETRANSMISSIONS_DEFAULT 3
#define TIMEOUT_DEFAULT 4
#define _POSIX_SOURCE 1 /* POSIX compliant source */

//MISC
#define FALSE 0
#define TRUE 1

//FLAG
#define FLAG 0x5c
//ADDRESS
#define TX 0x01
#define RX 0x03
//CONTROL
#define SET 0x03
#define UA 0x07
#define DISC 0x0b
#define I_0 0x00
#define I_1 0x02
#define RR_0 0x01
#define RR_1 0x21
#define REJ_0 0x05
#define REJ_1 0x25


// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

int send_ctrl_frame();

int send_info_frame();

int stuffing(char *str, int len);
 
int destuffing(char *str, int len);

void frame_state_machine(char ch, frameHeader *fh);




#endif // _LINK_LAYER_H_
