//Torlus - based on runtime_md.c in VmUnix directory

/*
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 * 
 */

/*=========================================================================
 * KVM
 *=========================================================================
 * SYSTEM:    KVM
 * SUBSYSTEM: Unix-specific functions needed by the virtual machine
 * FILE:      runtime_md.c
 * AUTHOR:    Frank Yellin
 *            Andreas Heilwagen, Kinsley Wong (Linux port)
 *=======================================================================*/

#include <global.h>
#include <stdlib.h>

#include "gba/minigba.h"
#include "gba/timers.h"

#define MAXCALENDARFLDS 15

#define YEAR 1
#define MONTH 2
#define DAY_OF_MONTH 5
#define HOUR 10
#define MINUTE 12
#define SECOND 13
#define MILLISECOND 14

static unsigned long date[MAXCALENDARFLDS];

// Console stuff

int cursor_row = 0;
int cursor_col = 0;

void gba_putc(char c) {

  if (c == '\n') {
    while(cursor_col < 30)
      MAP[8][cursor_row][cursor_col++] = ' ';
    cursor_row++;
    cursor_col = 0;
    return;
  }
  if (c < ' ') return;
  MAP[8][cursor_row][cursor_col++] = c;
  if (cursor_col >= 30) {
    cursor_row++;
    cursor_col = 0;
  }

}

// Timer stuff

/*=========================================================================
 * FUNCTION:      CurrentTime_md()
 * TYPE:          machine-specific implementation of native function
 * OVERVIEW:      Returns the current time. 
 * INTERFACE:
 *   parameters:  none
 *   returns:     current time, in milliseconds since startup
 *=======================================================================*/

ulong64
CurrentTime_md(void)
{
    char buf[100];

	  long long seconds = REG_TM3D;
	  long long milliSeconds = REG_TM2D / (65536/1000);

	  long long result = (seconds * (long long)1000) + milliSeconds;
	  return result;
}

void gba_sleep(long delta) {

  ulong64 start = CurrentTime_md();
  while( CurrentTime_md() < (start + delta) );

}


// Other stuff

void AlertUser(const char* message)
{
    fprintf(stderr, "ALERT: %s\n", message);
}

cell *allocateHeap(long *sizeptr, void **realresultptr) { 
    void *space = malloc(*sizeptr + sizeof(cell) - 1);
    *realresultptr = space;
    return (void *) ((((long)space) + (sizeof(cell) - 1)) & ~(sizeof(cell) - 1));
}

void *
allocateVirtualMemory_md(long size) {
}

void 
freeVirtualMemory_md(void *address, long size) { 
}

void  
protectVirtualMemory_md(void *address, long size, int protection) {
}

static void signal_handler(int sig) {
}

void InitializeFloatingPoint() {
}

void InitializeNativeCode() {
}

void FinalizeNativeCode() {
}

/*=========================================================================
 * FUNCTION:      Calendar_md()
 * TYPE:          machine-specific implementation of native function
 * OVERVIEW:      Initializes the calendar fields, which represent the 
 *                Calendar related attributes of a date. 
 * INTERFACE:
 *   parameters:  none
 *   returns:     none
 * AUTHOR:        Tasneem Sayeed
 *=======================================================================*/

unsigned long *
Calendar_md(void)
{
    return date;
}

