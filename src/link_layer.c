// Link layer protocol implementation
#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;

int fd;
unsigned char chr[1] = {0};

struct termios oldtio;
struct termios newtio;

bool frameCountTx = 0;
bool frameCountRx = 0;

int tries;
int timeout;
stateNames state = Start;

struct timeval stop, start;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("\nAlarm #%d\n", alarmCount);
}

int sendRxResponse(unsigned char C)
{
    switch (C)
    {
    case C_RR0:
        printf("Sent response RR0\n");
        break;
    case C_RR1:
        printf("Sent response RR1\n");
        break;
    case C_REJ0:
        printf("Sent response REJ0\n");
        break;
    case C_REJ1:
        printf("Sent response REJ1\n");
        break;
    }

    unsigned char frame[5] = {FLAG, ANS_RX, C, ANS_RX ^ C, FLAG};
    return write(fd, frame, 5);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    gettimeofday(&start, NULL);
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // the read() func will return immediately, with either the number of bytes currently available in the receiver buffer, or the number of bytes requested

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    tries = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    if(connectionParameters.role == LlTx){

        unsigned char buf[5] = {FLAG, CMD_TX, C_SET, CMD_TX ^ C_SET, FLAG};

        // Set alarm function handler
        (void)signal(SIGALRM, alarmHandler);

        
        alarmEnabled = FALSE;
        alarmCount = 0;

        while (alarmCount <= tries)
        {
            if (alarmEnabled == FALSE)
            {
                alarm(timeout); // Set alarm to be triggered in timeout seconds
                alarmEnabled = TRUE;

                int write_bytes = write(fd, buf, 5);
                sleep(1);
                printf("%d bytes written\n", write_bytes);
            }

            // Returns after a char has been input
            int read_bytes = read(fd, chr, 1);
            if (read_bytes)
            {
                switch (state)
                {
                case Start:
                {
                    if (chr[0] == FLAG)
                        state = FlagRCV;
                    break;
                }
                case FlagRCV:
                {
                    if (chr[0] == ANS_RX) // This is specific state machine for receiving UA that exits as soon as a wrong char is read, in a more general state machine, we don't verify what we receive here
                        state = ARCV;
                    else if (chr[0] != FLAG)
                        state = Start;
                    break;
                }
                case ARCV:
                {
                    if (chr[0] == C_UA)
                        state = CRCV;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                }
                case CRCV:
                {
                    if (chr[0] == (ANS_RX ^ C_UA)) // In a more general state machine, where we don't know what we are receiving, we compare the received BCC1 to the A (received) ^ C (received)
                        state = BccOK;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                }

                case BccOK:
                {
                    if (chr[0] == FLAG)
                    {
                        printf("Read UA\n\n");

                        alarm(0);
                        return 1;
                    }
                    else
                        state = Start;
                    break;
                }
                }
            }
        }
    }   
    else if(connectionParameters.role == LlRx){
            // Loop for input
        chr[0] = 0;
        state = Start;
        STOP = FALSE;

        while (STOP == FALSE)
        {
            // Returns after a char has been input
            int read_bytes = read(fd, chr, 1);

            if (read_bytes)
            {
                switch (state)
                {
                case Start:
                {
                    if (chr[0] == FLAG)
                        state = FlagRCV;
                    break;
                }
                case FlagRCV:
                {
                    if (chr[0] == CMD_TX)
                        state = ARCV;
                    else if (chr[0] != FLAG)
                        state = Start;
                    break;
                }
                case ARCV:
                {
                    if (chr[0] == CMD_TX)
                        state = CRCV;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                }
                case CRCV:
                {
                    if (chr[0] == (CMD_TX ^ C_SET))
                        state = BccOK;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                }
                case BccOK:
                {
                    if (chr[0] == FLAG)
                    {
                        printf("Read SET\n");

                        unsigned char UA[5] = {FLAG, ANS_RX, C_UA, ANS_RX ^ C_UA, FLAG};
                        int bytes = write(fd, UA, 5);
                        sleep(1);
                        printf(" %d UA bytes written\n\n", bytes);

                        STOP = TRUE;
                    }
                    else
                    {
                        state = Start;
                    }
                    break;
                }
                }
            }
        }    
    }
    return -1;
}

int llwriteSendFrame(unsigned char *frame, int frameSize)
{  
    unsigned char expectedResponse;
    unsigned char rejection;
    if(frameCountTx == 1){
        expectedResponse = C_RR0;
        rejection = C_REJ1;
    }
    else {
        expectedResponse = C_RR1;
        rejection = C_REJ0;
    }

    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    chr[0] = 0;
    state = Start;
    alarmEnabled = FALSE;
    alarmCount = 0;

    while (alarmCount <= tries)
    {
        if (alarmEnabled == FALSE)
        {
            alarm(timeout); // Set alarm to be triggered in timeout seconds
            alarmEnabled = TRUE;

            int write_bytes = write(fd, frame, frameSize);
            sleep(1);

        }

        // Returns after a char has been input
        int read_bytes = read(fd, chr, 1);
        if (read_bytes)
        {
            switch (state)
            {
            case Start:
            {
                if (chr[0] == FLAG)
                    state = FlagRCV;
                break;
            }
            case FlagRCV:
            {
                if (chr[0] == ANS_RX)
                    state = ARCV;
                else if (chr[0] != FLAG)
                    state = Start;
                break;
            }
            case ARCV:
            {
                if (chr[0] == expectedResponse)
                    state = CRCV;
                else if (chr[0] == FLAG)
                    state = FlagRCV;
                else if (chr[0] == rejection)
                {
                    printf("Read RJ\n");
                    alarm(0);
                    return 0;
                }
                else
                    state = Start;
                break;
            }
            case CRCV:
            {
                if (chr[0] == (ANS_RX ^ expectedResponse))
                    state = BccOK;
                else if (chr[0] == FLAG)
                    state = FlagRCV;
                else
                    state = Start;
                break;
            }
            case BccOK:
            {
                if (chr[0] == FLAG)
                {
                    printf("Read RR%d\n", !frameCountTx);
                    alarm(0);
                    frameCountTx = !frameCountTx;
                    return 1;
                }
                else
                    state = Start;
                break;
            }
            }
        }
    }
    printf("Obtained no appropriate response after numRetransmission alarms\n");
    alarm(0);
    return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    
    unsigned char infFrame[MAX_PAYLOAD_SIZE * 2];
    unsigned char controlField;

    if(frameCountTx == 1){
        controlField = C_FRAME1;
    }
    else{
        controlField = C_FRAME0; 
    }
    
    unsigned char header[4] = {FLAG,
                               CMD_TX,
                               controlField,
                               CMD_TX ^ controlField};

    unsigned result_idx = 4; // Next idx on which to write data
    memcpy(infFrame, header, 4);

    unsigned char bcc2 = buf[0];

    // Stuffing Data
    StuffingAux stuffData;

    for (int i = 0; i < bufSize; i++)
    {
        stuffData = stuffByte(buf[i]);

        if (stuffData.stuffed)
        {
            infFrame[result_idx++] = stuffData.byte1;
            infFrame[result_idx] = stuffData.byte2;
        }
        else
        {
            infFrame[result_idx] = buf[i];
        }
        result_idx++;

        if (i)
        { // If it is not the first chunk of data, XOR it with the rest
            bcc2 = bcc2 ^ buf[i];
        }
    }

    StuffingAux stuffBCC2 = stuffByte(bcc2);

    if (stuffBCC2.stuffed)
    {
        infFrame[result_idx++] = stuffBCC2.byte1;
        infFrame[result_idx++] = stuffBCC2.byte2;
    }
    else
    {
        infFrame[result_idx++] = bcc2;
    }

    infFrame[result_idx] = FLAG;

    int result = llwriteSendFrame(infFrame, 2 * bufSize + 6);

    while (result == 0)
    {
        result = llwriteSendFrame(infFrame, 2 * bufSize + 6); // Resend frame while REJs are being read
    }
    return result == 1 ? bufSize + 6 : -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    chr[0] = 0;
    state = Start;
    STOP = FALSE;

    bool receivedFrame;

    unsigned int packetIdx = 0;
    unsigned char bcc2;

    while (STOP == FALSE)
    {
        // Returns after a char has been input
        int read_bytes = read(fd, chr, 1);
        if (read_bytes)
        {
            switch (state)
            {
            case Start:
            {
                if (chr[0] == FLAG)
                    state = FlagRCV;
                break;
            }
            case FlagRCV:
            {
                if (chr[0] == CMD_TX)
                    state = ARCV;
                else if (chr[0] != FLAG)
                    state = Start;
                break;
            }
            case ARCV:
            {
                if (chr[0] == C_FRAME0)
                {
                    receivedFrame = 0;
                    state = CRCV;
                }
                else if (chr[0] == C_FRAME1)
                {
                    receivedFrame = 1;
                    state = CRCV;
                }
                else if (chr[0] == FLAG)
                    state = FlagRCV;
                else
                    state = Start;
                break;
            }
            case CRCV:
            {
                unsigned int receivedC = receivedFrame ? 0x40 : 0x00;
                if (chr[0] == (CMD_TX ^ receivedC))
                    state = Data;
                else if (chr[0] == FLAG)
                    state = FlagRCV;
                else
                    state = Start;
                break;
            }
            case Data:
            {
                if (chr[0] == ESC)
                    state = Destuffing;
                else if (chr[0] == FLAG)
                {
                    printf("Read whole input\n");
                    unsigned char receivedBcc2 = packet[packetIdx - 1];
                    bcc2 = packet[0];

                    packet[packetIdx - 1] = '\0'; // Remove read bcc2 because the application layer only wants the data packets

                    for (int i = 1; i < packetIdx - 1; i++) // Start in index 1 because index 0 is THE value that initializes bcc2
                    {
                        bcc2 = bcc2 ^ packet[i];
                    }

                    if (bcc2 == receivedBcc2) // Frame data has NO errors
                    {
                        if (receivedFrame == frameCountRx)
                        {
                            frameCountRx = !frameCountRx;
                            if(frameCountRx==1){ // Updated value with what frame receiver expects next
                                sendRxResponse(C_RR1);
                            }
                            else{
                                sendRxResponse(C_RR0);
                            }
                            printf("Read expected frame\n");
                            return packetIdx;
                        }
                        else // Duplicate Frame
                        {
                            if(frameCountRx==1){ 
                                sendRxResponse(C_RR1);
                            }
                            else{
                                sendRxResponse(C_RR0);
                            } // Receiver is still expecting the same frame, because it just got a duplicate
                            printf("Read duplicate frame\n");
                            return -1;
                        }
                    }
                    else // Frame data HAS errors
                    {
                        if (receivedFrame == frameCountRx)
                        {
                            if(frameCountRx==1){ 
                                sendRxResponse(C_REJ1);
                            }
                            else{
                                sendRxResponse(C_REJ0);
                            } // Receiver is still expecting the same frame, because it just got a frame with errors
                            printf("Read expected frame with errors\n");
                            return -1;
                        }
                        else // Duplicate Frame with errors
                        {
                            if(frameCountRx==1){
                                sendRxResponse(C_RR1);
                            }
                            else{
                                sendRxResponse(C_RR0);
                            } 
                            printf("Read duplicate frame with errors\n");
                            return -1;
                        }
                    }
                }
                else
                    packet[packetIdx++] = chr[0];
                break;
            }
            case Destuffing:
            {
                if (chr[0] == 0x5E)       // in case what was stuffed was the equivalent of a FLAG
                {
                    packet[packetIdx++] = 0x7E;
                    state = Data;
                }
                else if (chr[0] == 0x5D)  // in case what was stuffed was the equivalent of an ESC
                {
                    packet[packetIdx++] = 0x7D;
                    state = Data;
                }
                else                      // in case there was something wrong with stuffing and an ESC was found alone
                {
                    printf("An ESC wasn't stuffed\n");   
                    state = Data;
                }
                break;
            }
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////

int llclose(int showStatistics, LinkLayer connectionParameters)
{
    if(connectionParameters.role == LlTx){

        unsigned char buf[5] = {FLAG, CMD_TX, C_DISC, CMD_TX ^ C_DISC, FLAG};

        // Set alarm function handler
        (void)signal(SIGALRM, alarmHandler);

        chr[0] = 0;
        state = Start;
        alarmEnabled = FALSE;
        alarmCount = 0;

        while (alarmCount <= tries)
        {
            if (alarmEnabled == FALSE)
            {
                alarm(timeout); // Set alarm to be triggered in timeout seconds
                alarmEnabled = TRUE;

                int write_bytes = write(fd, buf, 5);
                sleep(1);
                printf(" %d bytes written\n", write_bytes);
            }

            // Returns after a char has been input
            int read_bytes = read(fd, chr, 1);
            if (read_bytes)
            {
                switch (state)
                {
                case Start:
                    if (chr[0] == FLAG)
                        state = FlagRCV;
                    break;
                case FlagRCV:
                    if (chr[0] == CMD_RX)
                        state = ARCV;
                    else if (chr[0] != FLAG)
                        state = Start;
                    break;
                case ARCV:
                    if (chr[0] == C_DISC)
                        state = CRCV;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                case CRCV:
                    if (chr[0] == (C_DISC ^ CMD_RX))
                        state = BccOK;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                case BccOK:
                    if (chr[0] == FLAG)
                    {
                        printf("Read C_DISC\n");

                        unsigned char UA[5] = {FLAG, ANS_TX, C_UA, ANS_TX ^ C_UA, FLAG};
                        int bytes = write(fd, UA, 5);
                        sleep(1);
                        printf(" %d UA bytes written\n", bytes);

                        alarm(0);
                        return 1;
                    }
                    else
                        state = Start;
                    break;
                }
            }
        }
    }    
    else if(connectionParameters.role == LlRx){
        state = Start;
        chr[0] = 0;

        STOP = FALSE;

        while (STOP == FALSE)
        {
            // Returns after a char has been input
            int read_bytes = read(fd, chr, 1);
            if (read_bytes)
            {
                switch (state)
                {
                case Start:
                    if (chr[0] == FLAG)
                        state = FlagRCV;
                    break;
                case FlagRCV:
                    if (chr[0] == CMD_TX)
                        state = ARCV;
                    else if (chr[0] != FLAG)
                        state = Start;
                    break;
                case ARCV:
                    if (chr[0] == C_DISC)
                        state = CRCV;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                case CRCV:
                    if (chr[0] == (C_DISC ^ CMD_TX))
                        state = BccOK;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                case BccOK:
                    if (chr[0] == FLAG)
                    {
                        printf("Read C_DISC\n");
                        STOP = true;
                    }
                    else
                        state = Start;
                }
            }
        }

        unsigned char buf[5] = {FLAG, CMD_RX, C_DISC, CMD_RX ^ C_DISC, FLAG};

        (void)signal(SIGALRM, alarmHandler);

        chr[0] = 0;
        state = Start;
        alarmEnabled = FALSE;
        alarmCount = 0;

        // Write C_C_DISC and wait for UA from transmitter
        while (alarmCount <= tries)
        {
            if (alarmEnabled == FALSE)
            {
                alarm(timeout); // Set alarm to be triggered in timeout seconds
                alarmEnabled = TRUE;

                int write_bytes = write(fd, buf, 5);
                sleep(1);
                printf(" %d bytes written\n", write_bytes);
            }

            // Returns after a char has been input
            int read_bytes = read(fd, chr, 1);
            if (read_bytes)
            {
                switch (state)
                {
                case Start:
                {
                    if (chr[0] == FLAG)
                        state = FlagRCV;
                    break;
                }
                case FlagRCV:
                {
                    if (chr[0] == ANS_TX) // This is specific state machine for receiving UA that exits as soon as a wrong char is read, in a more general state machine, we don't verify what we receive here
                        state = ARCV;
                    else if (chr[0] != FLAG)
                        state = Start;
                    break;
                }
                case ARCV:
                {
                    if (chr[0] == C_UA)
                        state = CRCV;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                }
                case CRCV:
                {
                    if (chr[0] == (ANS_TX ^ C_UA)) // In a more general state machine, where we don't know what we are receiving, we compare the received BCC1 to the A (received) ^ C (received)
                        state = BccOK;
                    else if (chr[0] == FLAG)
                        state = FlagRCV;
                    else
                        state = Start;
                    break;
                }

                case BccOK:
                {
                    if (chr[0] == FLAG)
                    {
                        printf("Read UA\n");
                        alarm(0);
                        gettimeofday(&stop, NULL);

                        printf("took %lu us to transfer the file\n", (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec);
                        return 1;
                    }
                    else
                        state = Start;
                    break;
                }
                }
            }
        }
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    
    
    return 1;
}

StuffingAux stuffByte(unsigned char byte)
{
    StuffingAux result;

    switch (byte)
    {
    case FLAG:
    {
        result.stuffed = true;
        result.byte1 = ESC;
        result.byte2 = 0x5E;
        return result;
    }
    case ESC:
    {
        result.stuffed = true;
        result.byte1 = ESC;
        result.byte2 = 0x5D;
        return result;
    }
    default:
    {
        result.stuffed = FALSE;
        result.byte1 = byte;
        return result;
    }
    }
}
