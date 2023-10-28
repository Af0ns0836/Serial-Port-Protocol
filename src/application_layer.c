 // Application layer protocol implementation

#include "application_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "link_layer.h"


void applicationLayer(const char *serialPort, const char *roleStr, int baudRate,
                      int nTries, int timeout, const char *filename) {

    if (strcmp(roleStr, "tx") == 0)
    {
        // ***********
        // tx mode
        printf("tx mode\n");

        // open connection
        LinkLayer ll;
        sprintf(ll.serialPort, "%s", serialPort);
        ll.role = 0;
        ll.baudRate = baudRate;
        ll.nRetransmissions = nTries;
        ll.timeout = timeout;

        if(llopen(ll)==-1) {
            fprintf(stderr, "Could not initialize link layer connection\n");
            exit(1);
        }

        printf("connection opened\n");
        fflush(stdout);
        fflush(stderr);

        // open file to read
        const char *file_path = filename;
        int file_desc = open(file_path, O_RDONLY);
        if(file_desc < 0) {
            fprintf(stderr, "Error opening file: %s\n", file_path);
            exit(1);
        }

        // cycle through
        const int buf_size = MAX_PAYLOAD_SIZE-1;
        unsigned char buffer[buf_size+1];
        int write_result = 0;
        int bytes_read = 1;
        while (bytes_read > 0)
        {
            bytes_read = read(file_desc, buffer+1, buf_size);
            if(bytes_read < 0) {
                    fprintf(stderr, "Error receiving from link layer\n");
                    break;
            }
            else if (bytes_read > 0) {
                // continue sending data
                buffer[0] = 1;
                write_result = llwrite(buffer, bytes_read+1);
                if(write_result < 0) {
                    fprintf(stderr, "Error sending data to link layer\n");
                    break;
                }
                printf("read from file -> write to link layer, %d\n", bytes_read);
            }
            else if (bytes_read == 0) {
                // stop receiver
                buffer[0] = 0;
                llwrite(buffer, 1);
                printf("App layer: done reading and sending file\n");
                break;
            }

            sleep(1);
        }
        // close connection
        llclose(0);
        close(file_desc);
        //return 0;
    }
    else
    {
        // ***************
        // rx mode
        printf("rx mode\n");

        LinkLayer ll;
        sprintf(ll.serialPort, "%s", serialPort);
        ll.role = 1;
        ll.baudRate = baudRate;
        ll.nRetransmissions = nTries;
        ll.timeout = timeout;

        if(llopen(ll)==-1) {
            fprintf(stderr, "Could not initialize link layer connection\n");
            exit(1);
        }

        const char *file_path = filename;
        int file_desc = open(file_path, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if(file_desc < 0) {
            fprintf(stderr, "Error opening file: %s\n", file_path);
            exit(1);
        }

        int bytes_read = 0;
        int write_result = 0;
        const int buf_size = MAX_PAYLOAD_SIZE;
        unsigned char buffer[buf_size];
        int total_bytes = 0;

        while (bytes_read >= 0)
        {
            bytes_read = llread(buffer);
            if(bytes_read < 0) {
                fprintf(stderr, "Error receiving from link layer\n");
                break;
            }
            else if (bytes_read > 0) {
                if (buffer[0] == 1) {
                    write_result = write(file_desc, buffer+1, bytes_read-1);
                    if(write_result < 0) {
                        fprintf(stderr, "Error writing to file\n");
                        break;
                    }
                    total_bytes = total_bytes + write_result;
                    printf("read from file -> write to link layer, %d %d %d\n", bytes_read, write_result, total_bytes);
                }
                else if (buffer[0] == 0) {
                    printf("App layer: done receiving file\n");
                    break;
                }
            }
        }

        llclose(0);
        close(file_desc);

    }
}
