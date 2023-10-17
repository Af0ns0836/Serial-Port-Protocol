#ifndef ALARM_H
#define ALARM_H

#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#define FALSE 0
#define TRUE 1

extern int alarmEnabled;
extern int alarmCount;


// Alarm function handler
void alarmHandler(int signal);

void resetAlarm();

#endif // ALARM_H
