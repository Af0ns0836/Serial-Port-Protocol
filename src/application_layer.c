// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

unsigned int bytesToRepresent(long int n)
{
    return ceil(log2f((float)n) / 8.0); // log2 get the highest bit set, and division by 8 finds how many bytes it takes to represent it
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;
    strcpy(connectionParameters.serialPort, serialPort);
    connectionParameters.role = strcmp(role, "tx") ? LlRx : LlTx;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    llopen(connectionParameters);

    // Send Start Control Packet
    switch (connectionParameters.role)
    {
    case LlTx:
    {
        // Create start control packet
        FILE *file = fopen(filename, "rb");
        if (file == NULL)
        {
            perror("[application_layer] File not found\n");
            exit(-1);
        }

        fseek(file, 0L, SEEK_END);
        long int fileSize = ftell(file); // File size in bytes
        fseek(file, 0L, SEEK_SET);       // Reset file descriptor to start of file

        unsigned char L1 = bytesToRepresent(fileSize);
        unsigned char L2 = strlen(filename);
        int ctrlsize = 3 + L1 + 2 + L2;

        unsigned char control[ctrlsize];

        control[0] = (unsigned char)CTRL_START;
        control[1] = (unsigned char)T_SIZE;
        control[2] = L1;
        unsigned int idx = 3;

        for (int i = L1; i > 0; i--, idx++)
        {
            control[idx] = (fileSize >> (8 * (i - 1))) & 0xff;
        }
        control[idx++] = T_NAME;
        control[idx++] = L2;

        for (int i = 0; i < L2; i++, idx++)
        {
            control[idx] = filename[i];
        }

        if (llwrite(control, ctrlsize) == -1)
        {
            printf("[application_layer] Error: Transmitter obtained no response after alarms. Exiting program.\n");
            exit(-1);
        }
        printf("[application_layer] Wrote start control packet\n\n");

        unsigned char *fileContent = (unsigned char *)malloc(sizeof(unsigned char) * fileSize);
        fread(fileContent, sizeof(unsigned char), fileSize, file);

        long int leftoverBytes = fileSize;

        while (leftoverBytes > 0)
        {
            unsigned int numBytes = leftoverBytes < MAX_PAYLOAD_SIZE - 3 ? leftoverBytes : MAX_PAYLOAD_SIZE - 3;
            unsigned char data[numBytes];

            data[0] = 1;
            data[1] = (numBytes >> 8) & 0xff;
            data[2] = numBytes & 0xff;
            memcpy(data + 3, fileContent, numBytes);

            if (llwrite(data, 3 + numBytes) == -1)
            {
                printf("[application_layer] Error: Transmitter obtained no response after alarms. Exiting program.\n");
                fclose(file);
                exit(-1);
            }

            printf("[application_layer] Wrote frame of %d byte(s)\n\n", numBytes);
            leftoverBytes -= numBytes;
            fileContent += numBytes;
        }

        control[0] = 3;
        if (llwrite(control, ctrlsize) == -1)
        {
            printf("[application_layer] Error: Transmitter obtained no response after alarms. Exiting program.\n");
            fclose(file);
            exit(-1);
        }
        printf("[application_layer] Wrote end control packet\n\n");
        fclose(file);
        break;
    }
    case LlRx:
    {
        unsigned char packet[MAX_PAYLOAD_SIZE];
        while (llread(packet) == -1) // Read start control packet
            ;
        printf("[application_layer] Read start control packet\n");

        unsigned int numFieldBytes = packet[2];
        long int fileSize = 0;

        for (int i = 3, exp = numFieldBytes - 1; i < 3 + numFieldBytes; i++, exp--)
        {
            fileSize += packet[i] << (8 * exp);
        }

        unsigned int numNameBytes = packet[2 + numFieldBytes + 2];
        unsigned char receiverFilename[numNameBytes + 10]; // Initial filename + "-received" + '\0'
        unsigned char addon[9] = "-received";

        // Add "-received" to filename
        for (int i = numFieldBytes + 5, j = 0; i <= numFieldBytes + 5 + numNameBytes; i++, j++)
        {
            if (packet[i] == '.')
            {
                for (int k = 0; k < 9; k++)
                {
                    receiverFilename[j++] = addon[k];
                }
            }
            receiverFilename[j] = packet[i];
        }
        receiverFilename[numNameBytes + 9] = '\0';

        unsigned int leftoverBytes = fileSize;
        FILE *outputFile = fopen((char *)receiverFilename, "wb+");

        while (packet[0] != 3) // While we haven't received a control end packet
        {
        
            while (llread(packet) == -1)
                ;
            numFieldBytes = 256 * packet[1] + packet[2];
            leftoverBytes -= numFieldBytes;

            if (packet[0] != 3) // If the packet is not a stop control packet, write the data to the output file
            {
                printf("[application_layer] Read packet\n");
                unsigned char *dataStart = packet + 3;
                fwrite(dataStart, sizeof(unsigned char), numFieldBytes, outputFile);
            }
        }
        printf("[application_layer] Read end control packet\n\n");
        break;
    }
    }

    llclose(SHOW_STATISTICS, connectionParameters);

}
