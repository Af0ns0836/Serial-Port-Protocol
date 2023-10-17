// Link layer protocol implementation

#include "link_layer.h"
#include "alarm.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>


// MISC

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

// Globais

LinkLayer connectionParameters = {0};

int fd = -1;

// flags do write

enum FlagWrite{
    FLAG = 0x7E,
    A_SET = 0x03
};

// flags do read
enum FlagRead{
    A_UA = 0x01,
    START = 0,
    FLAG_RECEIVE = 1,
    A_RECEIVE = 2,
    C_RECEIVE = 3, 
    BCC_OK = 4,
    SM_STOP = 5,
};

enum Control {
    C_SET = 0x03,
    C_UA = 0x07,
    C_RR0 = 0x05,
    C_RR1 = 0x85,
    C_REJ0 = 0x81,
    DISC = 0x0B
};


volatile int STOP = FALSE;
int state = 0;


struct termios oldtio; 
struct termios newtio;

void setStateMachine(char byte){
	
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
		case SM_STOP:
            printf("chegou a stop\n");
			break;
		}
		
}
/*
int sendSET(unsigned char* set,unsigned char* ua){
    int counter = 0;
    int flag = 0;
    while (alarmCount < 4) {
        if (flag == 1) {
            break;
        }
        if (alarmEnabled == FALSE) {

            int bytes = write(fd, set, 5);

            // Wait until all bytes have been written to the serial port
            sleep(1);

            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            printf("%d bytes written\n", bytes);

        }

        for (int i = 0; i < 5; i++) {
            printf("set = 0x%02X\n", set[i]);
        }

        while (alarmEnabled == TRUE) {
            int bytes_ua = read(fd, ua, BUF_SIZE);
            counter++;

            if (bytes_ua == 0) {
                alarm(3);
                printf("0 bytes read\n");
            } else {
                flag = 1;
                for (int i = 0; i < bytes_ua; i++) {
                    printf("ua = 0x%02X\n", ua[i]);
                }
                break;
            }

        }
    }
    return 1;
}*/

/*
int sendUA(unsigned char* set,unsigned char* ua){
    int bytesset = read(fd, set, BUF_SIZE);
    if(bytesset != 5){
        printf("Invalid number of bytes");
        return -1;
    }
    else{
        for (int i = 0; i<bytesset; i++){
        printf("set = 0x%02X\n", set[i]);
        }
    }
    int bytesUA=write(fd,ua,5);

    printf("%d bytes answered\n", bytesUA);

    return 1;
}*/
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
  newtio.c_cc[VTIME] = 30; // Inter-character timer unused
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
      FLAG
    };
  unsigned char ua[5] = {
      FLAG,
      A_UA,
      C_UA,
      A_UA ^ C_UA,
      FLAG
    };
    int flag = 0;
    int counter = 0;
    unsigned char byte;
    if(connectionParameters.role == LlTx) {    // transmitter
         // initialize alarm
        (void)signal(SIGALRM, alarmHandler);
        while(connectionParameters.nRetransmissions != 0 && state != STOP){
            
            int byte = write(fd, set, 5);
            alarm(connectionParameters.timeout); // Set alarm to be triggered in 3s
            alarmEnabled = FALSE;
            if(read(fd,&byte,1) < 0){
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
                    case SM_STOP:
                        printf("chegou a stop\n");
                        break;
                }
            }

            printf("%d bytes written\n", byte);
            for (int i = 0; i < 5; i++) {
                printf("set = 0x%02X\n", set[i]);
            }
            connectionParameters.nRetransmissions--;
        }
            
    }
    else if (connectionParameters.role == LlRx) { // receiver
    
        while(state != STOP){
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
                case SM_STOP:
                    printf("chegou a stop\n");
                    break;
                }
            
            } 
        }
        int bytesUA=write(fd,ua,5);

        printf("%d bytes answered\n", bytesUA);
            
    }
    
  printf("Successfull connection established.\n");

  return 1;

}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{

    unsigned char tramaI[4] = { FLAG, A_SET, C_SET, A_SET ^ C_SET};
    unsigned char tramaItrailer[2] = {0,FLAG};
    unsigned char bcc2 = 0;
    
    int bytes = write(fd, tramaI, bufSize);
    
    unsigned char escapeFlag[2] = {0x7d,0x7E^0x20};
    unsigned char escapeEscape[2] = {0x7d,0x7d^0x20};
    
    while(alarmCount < connectionParameters.nRetransmissions){
    
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
        bytes += write(fd, bcc2, sizebcc2);
    }    
    alarm(3);
    for(int i = 0; i < bytes; i++){
        ;
    }
    return bytes;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    printf("Terminating connection...\n");
    if(connectionParameters.role == LlTx){
    
    }
    else if (connectionParameters.role == LlRx){
    
    } 
    // Restore the old port settings
	if (tcsetattr(fd, TCSANOW, & oldtio) == -1) {
	  perror("tcsetattr");
	  exit(-1);
	}

	close(fd);
    printf("Terminating connection...\n");
    return 1;
}
