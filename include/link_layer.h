// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

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
    START,
    FLAG_RECEIVE,
    A_RECEIVE,
    C_RECEIVE, 
    BCC_OK,
    SM_STOP,
    DATA,
    FOUND_ESC
} MachineState;


#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256
#define ESC 0x7D


#define FLAG  0x7E
#define A_UA  0x01
#define A_SET  0x03
#define C_SET  0x03
#define C_UA 0x07
#define C_RR(Nr) ((Nr << 7) | 0x05)
#define C_REJ(Nr) ((Nr << 7) | 0x01)
#define C_N(Ns) (Ns << 6)
#define DISC 0x0B


// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

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

#endif // _LINK_LAYER_H_
