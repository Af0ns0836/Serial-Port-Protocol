#include "link_layer.h"

/*------------------------------------------------------------------------------*/

LinkLayer connectionParameters;

struct termios oldtio, newtio;

volatile int ALRM_FLAG;
volatile int STOP;
volatile int RETRANSMISSION;

int alarmCount = 0;
int timeout = 1;
int fd = -1;
int tries;
unsigned char chr[1] = {0};



char DATA[MAX_PAYLOAD_SIZE+1];
int data_length;

frameHeader frameT;
frameHeader frameR;
stateNames state;

struct timeval stop, start;

char *debugState[] = {"Start", "FlagRCV", "ARCV", "CRCV", "BccOK", "Data", "Stop"};

/*void alarm_handler()
{
    if (RETRANSMISSION)
    {
        ALRM_FLAG = TRUE;
        tries++;
        printf("Alarm %d\n", tries);
        if (tries == 4) return -1;
    }
}*/

void alarmHandler(int signal)
{
    ALRM_FLAG = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

/*------------------------------------------------------------------------------*/

int llopen(LinkLayer connectionParameters)
{    
    gettimeofday(&start, NULL);

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY );
    if (fd < 0) { perror(connectionParameters.serialPort); exit(-1); }

    if (tcgetattr(fd, &oldtio) == -1)                                             /* save current port settings */
    { 
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;                                                             /* set input mode (non-canonical, no echo,...) */

    newtio.c_cc[VTIME] = 0.1;                              /* inter-character timer unused */
    newtio.c_cc[VMIN]  = 0;

    if (connectionParameters.role == 1) {
        newtio.c_cc[VTIME] = connectionParameters.timeout*30;
        newtio.c_cc[VMIN]  = 0;
    }
        
    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }



    //printf("New termios structure set\n");
    printf("New termios structure set\n");

    /*------------------------------------------------------------------------------*/

    ALRM_FLAG = FALSE;
    STOP = FALSE;
    
    //frameHeader frame;
    state = Start;
    tries = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    /*------------------------------------------------------------------------------*/
    (void) signal(SIGALRM, alarmHandler); 
    if (connectionParameters.role == 0)  /* tx */
    {
        //(void) signal(SIGALRM, alarmHandler);        
        alarmCount = 0;
    

        frameT.A = TX; frameT.C = SET;
        if (send_ctrl_frame() == -1) return -1;
        /*
        printf("alarmCount: %d\n", alarmCount);
        printf("tries: %d\n", tries);
        printf("timeout: %d\n", timeout);
        while (alarmCount < tries)                               
        {                 
            if(ALRM_FLAG == FALSE) { 
                int bytes_read = send_ctrl_frame();
                alarm(timeout); ALRM_FLAG = TRUE; 
                if (bytes_read < 0) {
                    printf("Failed to send SET message.\n");
                    return -1;
                }

            }
            while (state != Stop)
            {
                printf("state: %s\n", debugState[state]);
               int b_rcv = read(fd, chr, 1);
                if(b_rcv <= 0){
                    break;
                }
                else frame_state_machine(chr[0], &frameR);;
               
            }
            if(state == Stop && frameR.C == UA){
                printf("vski\n");
                break;
            }
            */
        while (STOP == FALSE && alarmCount <= connectionParameters.nRetransmissions)                               
        {                  
            printf("alarmCount: %d\n", alarmCount);
            int bytes_read = read(fd, chr, 1);
            if(alarmCount > 0 && bytes_read == 0){
                if(send_ctrl_frame() == -1) return -1;
            }
            printf("bytes_read: %d\n", bytes_read);
            if (bytes_read == 0)
            {
                if(ALRM_FLAG == FALSE) { alarm(timeout); ALRM_FLAG = TRUE; }
                /*if(ALRM_FLAG) { 
                    alarm(connectionParameters.timeout); ALRM_FLAG = FALSE; 
                }*/
            }
            //else if (bytes_read == -1) { printf("Error. Timeout.\n"); return -1; }

            else frame_state_machine(chr[0], &frameR);

            if (state == Stop && frameR.C == UA) STOP = TRUE;
            
        }

        if (alarmCount >= connectionParameters.nRetransmissions) {
            printf("ua not received\n");
            return -1;
        }
        else printf("UA message recieved.\n");

        /*
        if (tries == 4) return -1;

        printf("UA message recieved.\n");
        
        */

        frameT.A = TX; frameT.C = I_0;

        return 1;
    }

    /*------------------------------------------------------------------------------*/

    else if (connectionParameters.role == 1)  /* rx */
    {

        while ( STOP == FALSE )                                                        
        {                                                       
            if (read(fd, chr, 1) <= 0) { STOP = TRUE; printf("Error. Timeout.\n"); return -1; }
            
            frame_state_machine(chr[0], &frameR);
            
            if (state == Stop && frameR.C == SET) {STOP = TRUE; printf("SET message recieved.\n");}
        }


        frameT.A = TX; frameT.C = UA; /* UA */
        if (send_ctrl_frame() == -1) return -1;

        return 1;
    }

    return -1;   
}

int llwrite(const unsigned char* buf, int bufSize)
{
    alarmCount = 0;
    (void) signal(SIGALRM, alarmHandler);


    printf("\n/*----------------------------------*/\n");

    /*------------------------------------------------------------------------------*/

    memset(DATA, 0, MAX_PAYLOAD_SIZE+1);

    for(int i = 0; i<bufSize; i++) DATA[i] = buf[i];

    data_length = bufSize;

    printf("LLWRITE: %d\n", data_length+1);

    int len = send_info_frame();
    
    /*------------------------------------------------------------------------------*/

    ALRM_FLAG = FALSE;
    STOP = FALSE;
    //frameHeader frame;
    state = Start;
    //tries = 0;
    /*
    while(alarmCount < tries){
        if(ALRM_FLAG == FALSE){
            int len = send_info_frame();
            printf("\nInfo frame sent Ns = %d\n",len);
            alarm(timeout); 
            ALRM_FLAG = TRUE;
        }
        printf("exitou\n");
        int response_bytes = read(fd, chr, 1);
        if(response_bytes == 0){
            printf("timeout\n");
            continue;
        }
        else  frame_state_machine(chr[0], &frameR);
    }
    if(alarmCount == 4){
        return -1;
    }*/
    
    while (STOP == FALSE && alarmCount < connectionParameters.nRetransmissions)
    {

        int bytes_read = read(fd, chr, 1);
        if(alarmCount > 0 && bytes_read == 0){
            if(send_info_frame() == -1) return -1;
        }
        
        if (bytes_read == 0)
        {
            if(ALRM_FLAG == FALSE) { alarm(timeout); ALRM_FLAG = TRUE; }
        }

        else frame_state_machine(chr[0], &frameR);

        if (state == Stop) STOP = TRUE;
    }

    if(alarmCount == 4) return -1;
    
    /*------------------------------------------------------------------------------*/

    switch(frameR.C)
    {
        case RR_0 : frameT.A = TX; frameT.C = I_0; printf("RR_0 message recieved.\n"); break;

        case RR_1 : frameT.A = TX; frameT.C = I_1; printf("RR_1 message recieved.\n"); break;

        case REJ_0 : frameT.A = TX; frameT.C = I_1; printf("REJ_0 message recieved.\n"); break;

        case REJ_1 : frameT.A = TX; frameT.C = I_0; printf("REJ_1 message recieved.\n"); break;
    }

    printf("\n/*----------------------------------*/\n");
    printf("LLWRITE: %d\n", len);
    return len;
}

int llread(unsigned char* packet)
{
    ALRM_FLAG = FALSE;
    STOP = FALSE;
    
    //frameHeader frame;
    state = Start;
    //tries = 0;

    int len = 0;
    int res = 0;

    char aux[MAX_PAYLOAD_SIZE*2+1] = {0};

    /*------------------------------------------------------------------------------*/

    while (STOP == FALSE)
    {                                                       
        if (read(fd, chr, 1) <= 0) { STOP = TRUE; printf("Error. Timeout.\n"); return -1; }

        frame_state_machine(chr[0], &frameR);

        if (state == Data)
        {
            aux[len] = chr[0];
            len++;
        }
        
        if (state == Stop)
        {
            printf("\n/*----------------------------------*/\n");
            printf("LLREAD: %d\n", len);
            res = destuffing(aux, len);
            STOP = TRUE;  
        }
    }

    /*------------------------------------------------------------------------------*/

    if (res == -1)
    {
        switch(frameR.C)
        {
            case I_0 : frameT.A = TX; frameT.C = REJ_1; printf("\nI0 message recieved."); break;

            case I_1 : frameT.A = TX; frameT.C = REJ_0; printf("\nI1 message recieved."); break;
        }
        if (send_ctrl_frame() == -1) return -1;
        
        return 0;
    }

    switch(frameR.C)
    {
        case I_0 : frameT.A = TX; frameT.C = RR_1; printf("\nI0 message recieved."); break;

        case I_1 : frameT.A = TX; frameT.C = RR_0; printf("\nI1 message recieved."); break;
    }

    if (send_ctrl_frame() == -1) return -1;

    /*------------------------------------------------------------------------------*/

    printf("\n/*----------------------------------*/\n");

    for (int i = 0; i<res; i++) packet[i] = aux[i];

    return res;
}

int llclose(int showStatistics)
{
    ALRM_FLAG = FALSE;
    STOP = FALSE;
    
    //frameHeader frame;
    state = Start;
    alarmCount = 0;

    /*------------------------------------------------------------------------------*/

    if (connectionParameters.role == 0)  /* tx */
    {
        (void) signal(SIGALRM, alarmHandler);

       
        frameT.A = TX; frameT.C = DISC;
        if (send_ctrl_frame() == -1) return -1;                                     
        
        while (STOP == FALSE && alarmCount < connectionParameters.nRetransmissions)                               
        {  
            printf("tries: %d\n", alarmCount);
            if (read(fd, chr, 1) == 0)
            {
                //if(ALRM_FLAG) { alarm(connectionParameters.timeout); ALRM_FLAG = FALSE; }
                 if(ALRM_FLAG == FALSE) { alarm(connectionParameters.timeout); ALRM_FLAG = TRUE; }
            }
                
            else frame_state_machine(chr[0], &frameR);

            if (state == Stop && frameR.C == DISC) STOP = TRUE;
        }

        if (tries == 4) return -1;

        printf("\nDISC message recieved.\n");


        frameT.A = TX; frameT.C = UA;
        if (send_ctrl_frame() == -1) return -1;   
    }

    /*------------------------------------------------------------------------------*/

    else if (connectionParameters.role == 1)  /* rx */
    {
        while (STOP == FALSE)                                                        
        {                                                       
            if (read(fd, chr, 1) <= 0) { STOP = TRUE; printf("Error. Timeout.\n"); return -1; }
            
            frame_state_machine(chr[0], &frameR);
            
            if (state == Stop && frameR.C == DISC) STOP = TRUE;
        }

        printf("\nDISC message recieved.\n");

        frameT.A = TX; frameT.C = DISC;
        if (send_ctrl_frame() == -1) return -1;

        STOP = FALSE;
        state = Start;

        while (STOP == FALSE)                                                        
        {                                                       
            if (read(fd, chr, 1) == 0) STOP = TRUE;
            
            frame_state_machine(chr[0], &frameR);
            
            if (state == Stop && frameR.C == UA) STOP = TRUE;
        }

        printf("\nUA message recieved.\n");
    }

    /*------------------------------------------------------------------------------*/

    /*if (showStatistics) 
    {
        int i; //TODO
    }*/

    /*------------------------------------------------------------------------------*/

    sleep(1);  

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }
    
    gettimeofday(&stop, NULL);
    printf("took %lu us\n", (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec);

    return 0;
}

// Sends control frame with the frameHeader set before | Used as the alarm callback function
int send_ctrl_frame()
{

    char CTRL[5];
    unsigned char Bcc = frameT.A^frameT.C;

    CTRL[0] = FLAG;
    CTRL[1] = frameT.A;
    CTRL[2] = frameT.C;
    CTRL[3] = Bcc;
    CTRL[4] = FLAG;                                                                     

    if (write(fd, CTRL, 5) != 5) return -1;
    //sleep(1);
    switch(frameT.C)
    {
        case SET : {
            printf("\nSent SET message...\n"); 
            break;
        }   

        case UA : printf("\nSent UA message...\n"); break;

        case DISC : printf("\nSent DISC message...\n"); break;

        case RR_0 : printf("\nSent RR_0 message...\n"); break;

        case RR_1 : printf("\nSent RR_1 message...\n"); break;

        default : printf("\nSent CTRL message...\n"); break;
    }
    
    return 0;
}

// Sends information frame with the frameHeader set before and data passed to llwrite | Used as the alarm callback function
int send_info_frame()
{
    ALRM_FLAG = FALSE;
    alarmCount++;
    if(alarmCount == 4){
        return -1;
    }
    /*
    ALRM_FLAG = TRUE;
    tries++;
    if (tries == 4) return -1;*/

    /*------------------------------------------------------------------------------*/

    char newDATA[MAX_PAYLOAD_SIZE*2] = {0};
 
    unsigned char Bcc1 = frameT.A^frameT.C;
    unsigned char Bcc2 = newDATA[0] = DATA[0];

    for(int i = 1; i<data_length; i++)
    {
        Bcc2 = Bcc2^DATA[i];
        newDATA[i] = DATA[i];
    }

    newDATA[data_length] = Bcc2;

    int stuflen = stuffing(newDATA, data_length+1);

    printf("STUFFING: %d\n", stuflen);

    /*------------------------------------------------------------------------------*/

    char INFO[MAX_PAYLOAD_SIZE+500] = {0};

    INFO[0] = FLAG;
    INFO[1] = frameT.A;
    INFO[2] = frameT.C;
    INFO[3] = Bcc1;
    for(int i = 0; i<stuflen; i++) INFO[i+4] = newDATA[i];
    INFO[stuflen + 4] = FLAG;

    int res = write(fd, INFO, stuflen+5);

    switch(frameT.C)
    {
        case I_0 : printf("\nSent I0 message...\n"); break;

        case I_1 : printf("\nSent I1 message...\n"); break;
    }

    return res;
}

int stuffing(char *str, int len)
{
    char aux[MAX_PAYLOAD_SIZE*2+1] = {0};
    int count = 0;

    /*------------------------------------------------------------------------------*/

    for (int i = 0; i<len; i++)  //stores 'str' with stuffed bytes in 'aux'
    {
        aux[count] = str[i];

        if (str[i]==0x5c)
        {
            aux[count] = 0x5d;
            aux[count+1] = 0x7c;
            count++;
        }
        else if (str[i]==0x5d)
        {
            aux[count] = 0x5d;
            aux[count+1] = 0x7d;
            count++;
        }
        count++;
    }

    for (int i = 0; i<count; i++) { //stores 'aux' in 'str'
        str[i] = aux[i];
    }

    return count;
}

int destuffing(char *str, int len)
{
    char aux[MAX_PAYLOAD_SIZE+1] = {0};
    int count = 0;

    /*------------------------------------------------------------------------------*/

    for (int i = 0; i<len; i++)  //stores 'str' with destuffed bytes in 'aux'
    {
        aux[count] = str[i];

        if (str[i]==0x5d && str[i+1]==0x7c) { aux[count] = 0x5c; i++; }

        else if (str[i]==0x5d && str[i+1]==0x7d) { aux[count] = 0x5d; i++; }

        count++;
    }

    memset(str, 0, len);
    for (int i=0; i<len; i++) { //stores 'aux' in 'str'
        str[i] = aux[i];
    }

    /*------------------------------------------------------------------------------*/

    printf("DESTUFFING: %d\n", count);

    char Bcc2 = str[0];
    for (int i = 1; i<count-1; i++) { //compute Bcc2
        Bcc2 = Bcc2^str[i];
    }

    if (Bcc2 == aux[count-1]) return count-1;

    return -1;
}

void frame_state_machine(char ch, frameHeader *frame)
{
    switch (state) {
        
        case Start :
        
            if (ch == FLAG) state = FlagRCV;
            
            break;
    
    
        case FlagRCV :
    
            switch(ch) {

                case TX : frame->A = TX; state = ARCV; break;
                
                case RX : frame->A = RX; state = ARCV; break;
                
                case FLAG : state = FlagRCV; break;

                default : state = Start;
            }
            
            break;
    
        case ARCV :

            switch (ch) {

                case I_0 : frame->C = I_0; state = CRCV; break;
                
                case I_1 : frame->C = I_1; state = CRCV; break;

                case SET : frame->C = SET; state = CRCV; break;
                
                case UA : frame->C = UA; state = CRCV; break;
                
                case DISC : frame->C = DISC; state = CRCV; break;
                
                case RR_0 : frame->C = RR_0; state = CRCV; break;
                
                case RR_1 : frame->C = RR_1; state = CRCV; break;
                
                case REJ_0 : frame->C = REJ_0; state = CRCV; break;
                
                case REJ_1 : frame->C = REJ_1; state = CRCV; break;

                case FLAG : state = FlagRCV; break;
                
                default : state = Start; break;
            }

            break;
            
        case CRCV :

            if (ch == FLAG) state = FlagRCV;
            
            if (ch == (frame->A^frame->C)) state = BccOK;

            else state = Start;
            
            break;
            
        case BccOK :

            if (ch == FLAG) state = Stop; 

            else if (frame->C == I_0 || frame->C == I_1) state = Data;

            else state = Start;

            break;

        case Data :

            if (ch == FLAG) state = Stop;
        case Stop :
            break;
    }
}

