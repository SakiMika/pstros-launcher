// Minimal Nintendo DS runtime glue for the CLDC/KVM core.
// Based on Torlus' GBA runtime_md.c, adapted to libnds/devkitARM.

#include <global.h>
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXCALENDARFLDS 15

#define YEAR 1
#define MONTH 2
#define DAY_OF_MONTH 5
#define HOUR 10
#define MINUTE 12
#define SECOND 13
#define MILLISECOND 14

static unsigned long date[MAXCALENDARFLDS];
static volatile unsigned long vblank_count = 0;
static int pstrosVmConsoleEnabled = 0;

extern void pstrosAudioDiagVmError(const char *message);

void pstrosSetVmConsoleEnabled(int enabled) {
    pstrosVmConsoleEnabled = enabled != 0;
}

void kvm_vblank_handler(void) {
    vblank_count++;
}

ulong64 CurrentTime_md(void) {
    // DS VBlank is approximately 59.826 Hz. 60 Hz is good enough for KVM timers.
    return ((ulong64)vblank_count * 1000ULL) / 60ULL;
}

void gba_sleep(long delta) {
    ulong64 start = CurrentTime_md();
    while (CurrentTime_md() < start + (ulong64)delta) {
        swiWaitForVBlank();
    }
}

void AlertUser(const char* message) {
    /* Keep fatal VM information visible, but suppress normal class/debug logs. */
    pstrosAudioDiagVmError(message);
}

int putchar_md(int c) {
    if (!pstrosVmConsoleEnabled) {
        return c;
    }
    if (c == '\n') {
        iprintf("\n");
    } else {
        iprintf("%c", c);
    }
    return c;
}

cell *allocateHeap(long *sizeptr, void **realresultptr) {
    void *space = malloc(*sizeptr + sizeof(cell) - 1);
    *realresultptr = space;
    if (space == NULL) {
        return NULL;
    }
    return (cell *)((((long)space) + (sizeof(cell) - 1)) & ~(sizeof(cell) - 1));
}

void *allocateVirtualMemory_md(long size) {
    return malloc(size);
}

void freeVirtualMemory_md(void *address, long size) {
    (void)size;
    free(address);
}

void protectVirtualMemory_md(void *address, long size, int protection) {
    (void)address;
    (void)size;
    (void)protection;
}

void InitializeFloatingPoint(void) {
}

void InitializeNativeCode(void) {
}

void FinalizeNativeCode(void) {
}

unsigned long *Calendar_md(void) {
    return date;
}
