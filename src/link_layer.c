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

int fd = -1;

// flags do write

enum FlagWrite{
    FLAG = 0x7E,
    A_Write = 0x03,
    C_Write = 0x03,
};

// flags do read
enum FlagRead{
    A_Read = 0x01,
    C_Read = 0x07,
    START = 0,
    FLAG_RECEIVE = 1,
    A_RECEIVE = 2,
    C_RECEIVE = 3, 
    BCC_OK = 4,
    SM_STOP = 5,
    BUF_SIZE =256
};

volatile int STOP = FALSE;
int state = 0;



void setStateMachine(char byte){
	
	switch(state){
		
		case START:
			if(byte == FLAG){
                 state = FLAG_RECEIVE;
                 printf("chegou ao start\n");               
                }
			break;
		case FLAG_RECEIVE:
			if(byte == A_Write){
                 state = A_RECEIVE;
                 printf("chegou ao flag_receive\n");
                }
			else if(byte == FLAG);
			else {state = START;}
			break;
		case A_RECEIVE:
			if(byte == C_Write){
                 state = C_RECEIVE;
                 printf("chegou a a_receive\n");
                }
			else if(byte == FLAG) state = FLAG_RECEIVE;
			else state = START;
			break;
		case C_RECEIVE:
			if(byte == (A_Write^C_Write)){ 
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

  // initialize alarm
  (void)signal(SIGALRM, alarmHandler);

  // Set new port settings
  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");
  unsigned char set[5] = {
      FLAG,
      A_Write,
      C_Write,
      A_Write ^ C_Write,
      FLAG
    };
  unsigned char ua[5] = {
      FLAG,
      A_Read,
      C_Read,
      A_Read ^ C_Read,
      FLAG
    };
    while(alarmCount < 4) {
        int flag = 0;
        int counter = 0;
        if(connectionParameters.role == LlTx) {
                //transmitter  
                //sendSET(set,ua);
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
        else if (connectionParameters.role == LlRx) {
            //receiver
            //sendUA(set,ua);
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
        }
    }
  printf("Successfull connection established.\n");

  return 1;

}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    
    return 0;
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
    // TODO

    return 1;
}