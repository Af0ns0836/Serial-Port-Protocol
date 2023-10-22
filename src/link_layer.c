// Link layer protocol implementation

#include "link_layer.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// MISC

MachineState state = START;

LinkLayer connectionParameters = {0};

unsigned char tramaTx = 0;
unsigned char tramaRx = 1;
int retransmissions = 0;
int fd = -1;

int alarmEnabled = FALSE;
int alarmCount = 0;
int timeout = 0;

void alarmHandler(int signal)
{
    alarmEnabled = TRUE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
  // Open serial port device for reading and writing, and not as controlling tty
  // because we don't want to get killed if linenoise sends CTRL-C.
  
  int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

  if (fd < 0) {
    perror(connectionParameters.serialPort);
    exit(-1);
  }
 
  struct termios oldtio; 
  struct termios newtio;

  // Save current port settings
  if (tcgetattr(fd, & oldtio) == -1) {
    perror("tcgetattr");
    exit(-1);
  }

  // Clear struct for new port settings
  memset( & newtio, 0, sizeof(newtio));

  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  // Set input mode (non-canonical, no echo,...)
  newtio.c_lflag = 0;
  newtio.c_cc[VTIME] = 0; // Inter-character timer unused
  newtio.c_cc[VMIN] = 0; // Blocking read until 5 chars received

  // VTIME e VMIN should be changed in order to protect with a	
  // timeout the reception of the following character(s)	

  // Now clean the line and activate the settings for the port
  // tcflush() discards data written to the object referred to
  // by fd but not transmitted, or data received but not read,
  // depending on the value of queue_selector:
  //   TCIFLUSH - flushes data received but not read.
  tcflush(fd, TCIOFLUSH);

  
  // Set new port settings
  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");
  unsigned char set[5] = {
      FLAG,
      A_SET,
      C_SET,
      A_SET ^ C_SET,
      FLAG};
  unsigned char ua[5] = {
      FLAG,
      A_UA,
      C_UA,
      A_UA ^ C_UA,
      FLAG};

  unsigned char byte;
  retransmissions = connectionParameters.nRetransmissions;
  timeout = connectionParameters.timeout;
  if (connectionParameters.role == LlTx)
  {   // transmitter
      // initialize alarm
      state = START;
      (void)signal(SIGALRM, alarmHandler);
      while (connectionParameters.nRetransmissions != 0 && state != SM_STOP)
      {
          int bytes = write(fd, set, 5);
          alarm(connectionParameters.timeout); // Set alarm to be triggered in 3s
          alarmEnabled = FALSE;
          if (bytes == -1)
          {
              printf("Error writing to serial port.\n");
              return -1;
          }

          while (alarmEnabled == FALSE && state != SM_STOP)
          {
              if (read(fd, &byte, 1) > 0)
              {
                  switch (state)
                  {
                  case START:
                      if (byte == FLAG)
                      {
                          state = FLAG_RECEIVE;
                          printf("chegou ao start\n");
                      }
                      break;
                  case FLAG_RECEIVE:
                      if (byte == A_UA)
                      {
                          state = A_RECEIVE;
                          printf("chegou ao flag_receive\n");
                      }
                      else if (byte == FLAG)
                          ;
                      else
                      {
                          state = START;
                      }
                      break;
                  case A_RECEIVE:
                      if (byte == C_SET)
                      {
                          state = C_RECEIVE;
                          printf("chegou a a_receive\n");
                      }
                      else if (byte == FLAG)
                          state = FLAG_RECEIVE;
                      else
                          state = START;
                      break;
                  case C_RECEIVE:
                      if (byte == (A_UA ^ C_UA))
                      {
                          state = BCC_OK;
                          printf("chegou a c_receive\n");
                      }
                      else if (byte == FLAG)
                          state = FLAG_RECEIVE;
                      else
                          state = START;
                      break;
                  case BCC_OK:
                      if (byte == FLAG)
                      {
                          state = SM_STOP;
                          printf("chegou a bcc_ok LlTX\n");
                      }
                      else
                          state = START;
                      break;
                  default:
                      break;
                  }
              }
          }
          printf("%d bytes written\n", bytes);
          for (int i = 0; i < 5; i++)
          {
              printf("set = 0x%02X\n", set[i]);
          }
        connectionParameters.nRetransmissions--;
      }
        if (state != SM_STOP)
        {
            printf("Error: retransmition\n");
            return -1;
        }
    }
    else if (connectionParameters.role == LlRx) { // receiver

        state = START;
        while(state != SM_STOP){
            if(read(fd,&byte,1) > 0){
                switch(state){
                case START:
                    if(byte == FLAG){
                        state = FLAG_RECEIVE;
                        printf("chegou ao start\n");               
                        }
                    break;
                case FLAG_RECEIVE:
                    if(byte == A_SET){
                        state = A_RECEIVE;
                        printf("chegou ao flag_receive\n");
                        }
                    else if(byte == FLAG);
                    else {state = START;}
                    break;
                case A_RECEIVE:
                    if(byte == C_SET){
                        state = C_RECEIVE;
                        printf("chegou a a_receive\n");
                        }
                    else if(byte == FLAG) state = FLAG_RECEIVE;
                    else state = START;
                    break;
                case C_RECEIVE:
                    if(byte == (A_SET^C_SET)){ 
                        state = BCC_OK;
                        printf("chegou a c_receive\n");
                        }
                    else if(byte == FLAG) state = FLAG_RECEIVE;
                    else state = START;
                    break;
                case BCC_OK:
                    if(byte == FLAG){ 
                        state = SM_STOP;
                        printf("chegou a bcc_ok\n");
                        }
                    else state = START;
                    break;
                default: 
                    break;
                }
            
            } 
        }
        int bytesUA=write(fd,ua,5);
        if(bytesUA == -1){
            printf("Error writing to serial port.\n");
            return -1;
        }
        printf("%d bytes answered\n", bytesUA);
            
    }
    
  printf("Successfull connection established.\n");

  return fd;

}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{

    unsigned char tramaI[4] = { FLAG, A_SET, C_SET, A_SET ^ C_SET};
    unsigned char tramaItrailer[2] = {0,FLAG};
    unsigned char bcc2 = 0;
    
    unsigned char escapeFlag[2] = {0x7d,0x7E^0x20};
    unsigned char escapeEscape[2] = {0x7d,0x7d^0x20};


    printf("Sending packet number %d. Transmission number %d.\n", alarmCount, connectionParameters.nRetransmissions);
    alarm(connectionParameters.timeout);
    int bytes = write(fd, tramaI, bufSize);

    for(int i = 0; i < bufSize; i++){
        bcc2 ^= buf[i];
        if(buf[i] == 0x7E){
                bytes += write(fd, escapeFlag, 2); 
        }
        else if(buf[i] == 0x7D){
                bytes += write(fd, escapeEscape, 2);
        }
        else{
                bytes += write(fd, buf + i, 2);
        }
    }
    tramaItrailer[0] = bcc2;
    int sizebcc2 = 2;
    bytes += write(fd, &bcc2, sizebcc2);
    bytes += write(fd, tramaItrailer, 2);
    unsigned char byte, c = 0;
    state = START;
    int rejected, accepted = 0;

        while(alarmCount < connectionParameters.nRetransmissions){
            alarmEnabled = FALSE;
            alarm(connectionParameters.timeout);
            rejected = 0; accepted = 0;
            while(!alarmEnabled && !rejected && !accepted){

                while(state != SM_STOP){
                    if(read(fd,&byte,1) > 0){
                        switch(state){
                        case START:
                            if(byte == FLAG){
                                state = FLAG_RECEIVE;
                                printf("chegou ao start\n");               
                                }
                            break;
                        case FLAG_RECEIVE:
                            if(byte == A_UA){
                                state = A_RECEIVE;
                                printf("chegou ao flag_receive\n");
                                }
                            else if(byte == FLAG);
                            else {state = START;}
                            break;
                        case A_RECEIVE:
                            if(byte == C_RR(0) || byte == C_RR(1) || byte == C_REJ(0) || byte == C_REJ(1) || byte == DISC){
                                state = C_RECEIVE;
                                c = byte;
                                printf("chegou a a_receive\n");
                            }
                            else if(byte == FLAG) state = FLAG_RECEIVE;
                            else state = START;
                            break;
                        case C_RECEIVE:
                            if(byte == (A_UA ^ c)){ 
                                state = BCC_OK;
                                printf("chegou a c_receive\n");
                                }
                            else if(byte == FLAG) state = FLAG_RECEIVE;
                            else state = START;
                            break;
                        case BCC_OK:
                            if(byte == FLAG){ 
                                state = SM_STOP;
                                printf("chegou a bcc_ok\n");
                                }
                            else state = START;
                            break;
                        default: 
                            break;
                        }                
                    }
                }    
                if(!c){
                    continue;
                }
                    else if(c == C_RR(tramaTx)){
                        printf("Received RR\n");
                        accepted = 1;
                        tramaTx = (tramaTx + 1)%2;
                    
                        
                    }
                    else if(c == C_REJ(tramaTx)){
                        printf("Received REJ\n");
                        rejected = 1;
            
                        break;
                    }
                    else continue;
            }
        if(accepted) break;
        ++alarmCount;
        } 
        if(accepted) return bytes;
        else{
            llclose(fd);
            printf("Error: retransmition\n");
            return -1;
        }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) 
{
    unsigned char byte, c;
    int i = 0;
    state = START;

    while (state != SM_STOP)
    {
        if (read(fd, &byte, 1) > 0)
        {
            switch (state)
            {
            case START:
                if (byte == FLAG)
                    state = FLAG_RECEIVE;
                break;
            case FLAG_RECEIVE:
                if (byte == A_SET)
                    state = A_RECEIVE;
                else if (byte != FLAG)
                    state = START;
                break;
            case A_RECEIVE:
                if (byte == C_N(tramaRx))
                {
                    state = C_RECEIVE;
                    c = byte;
                }
                else if (byte == FLAG)
                    state = FLAG_RECEIVE;
                else if (byte == DISC)
                {
                    unsigned char discon[5] = {FLAG, A_UA, DISC, A_UA ^ DISC, FLAG};
                    write(fd, discon, 5);
                    return 0;
                }
                else
                    state = START;
                break;
            case C_RECEIVE:
                if (byte == (A_SET ^ c))
                    state = DATA;
                else if (byte == FLAG)
                    state = FLAG_RECEIVE;
                else
                    state = START;
                break;
            case DATA:
                if (byte == ESC)
                    state = FOUND_ESC;
                else if (byte == FLAG)
                {
                    unsigned char bcc2 = packet[i - 1];
                    i--;
                    packet[i] = '\0';
                    unsigned char acc = packet[0];

                    for (unsigned int j = 1; j < i; j++)
                        acc ^= packet[j];

                    if (bcc2 == acc)
                    {
                        state = SM_STOP;
                        unsigned char rready[5] = {FLAG, A_UA, C_RR(tramaRx), A_UA ^ C_RR(tramaRx), FLAG};
                        write(fd, rready, 5);
                        tramaRx = (tramaRx + 1) % 2;
                        return i;
                    }
                    else
                    {
                        printf("Error: retransmition\n");
                        unsigned char rrej[5] = {FLAG, A_UA, C_REJ(tramaRx), A_UA ^ C_REJ(tramaRx), FLAG};
                        write(fd, rrej, 5);
                        return -1;
                    };
                }
                else
                {
                    packet[i++] = byte;
                }
                break;
            case FOUND_ESC:
                state = DATA;
                if (byte == ESC || byte == FLAG)
                    packet[i++] = byte;
                else
                {
                    packet[i++] = ESC;
                    packet[i++] = byte;
                }
                break;
            default:
                break;
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    state = START;
    unsigned char byte;
    (void) signal(SIGALRM, alarmHandler);
    
    while (connectionParameters.nRetransmissions != 0 && state != SM_STOP) {
                
        unsigned char discon[5] = {FLAG, A_SET, DISC, A_SET ^ DISC, FLAG};
    	write(fd, discon, 5);
        alarm(connectionParameters.timeout);
        alarmEnabled = FALSE;
                
        while (alarmEnabled == FALSE && state != SM_STOP) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case START:
                        if (byte == FLAG) state = FLAG_RECEIVE;
                        break;
                    case FLAG_RECEIVE:
                        if (byte == A_UA) state = A_RECEIVE;
                        else if (byte != FLAG) state = START;
                        break;
                    case A_RECEIVE:
                        if (byte == DISC) state = C_RECEIVE;
                        else if (byte == FLAG) state = FLAG_RECEIVE;
                        else state = START;
                        break;
                    case C_RECEIVE:
                        if (byte == (A_UA ^ DISC)) state = BCC_OK;
                        else if (byte == FLAG) state = FLAG_RECEIVE;
                        else state = START;
                        break;
                    case BCC_OK:
                        if (byte == FLAG) state = SM_STOP;
                        else state = START;
                        break;
                    default: 
                        break;
                }
            }
        } 
        connectionParameters.nRetransmissions--;
    }

    if (state != SM_STOP) return -1;
    unsigned char closing[5] = {FLAG, A_SET, C_UA, A_SET ^ C_UA, FLAG};
    write(fd, closing, 5);
    return close(fd);
}

