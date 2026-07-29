//Torlus - based on main.c in VmExtra directory

/*
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 */

/*=========================================================================
 * KVM
 *=========================================================================
 * SYSTEM:    KVM
 * SUBSYSTEM: Main program
 * FILE:      main.c
 * OVERVIEW:  Main program for command-line based environments
 * AUTHOR:    Antero Taivalsaari, Sun Labs
 *            Edited by Doug Simon 11/1998
 *            JAM integration by Sheng Liang
 *
 * NOTE:      KVM does not have a portable main() function.  This is
 *            because the VM may be used in very different kinds of 
 *            target environments.  Some of the environments may provide
 *            command line support, while many don't; some environments
 *            may launch the VM from a GUI or a micro-browser, etc.
 *
 *            The portable VM startup and shutdown operations are defined
 *            in file VmCommon/src/StartJVM.c. The main() function defined
 *            in this file is applicable only to those target systems that
 *            support VM startup from a command line.
 *=======================================================================*/

/*=========================================================================
 * Include files
 *=======================================================================*/

//#include "gba/basetype.h"
//#include "gba/gba.h"
//#include "gba/dispcnt.h"

#include "gba/minigba.h"
#include "gba/timers.h"

extern const u8 gba_font[];

#include <global.h>

#include <stdio.h>
#include <string.h>
#include "gbfs.h"

GBFS_FILE *gbfs;

void AgbMain (void) {
    int result;
    char className[128];
    char *kvm_argv[1] = { className };
    int kvm_argc = 1;
 
    char *p;
    int x;

    // Set up display and tiles    
    LCDMODE = LCDMODE_BLANK;
    PALRAM[0] = RGB(0, 0, 0);
    PALRAM[1] = RGB(0, 31, 0);

    BGSCROLL[0].x = 0;
    BGSCROLL[0].y = 0;
#define BGCTRL_256C      0X0080
    BGCTRL[0] =  BGCTRL_PAT(0) | BGCTRL_256C | BGCTRL_NAME(8)
               | BGCTRL_H32 | BGCTRL_V32;
        
    u16 *src = VRAM;
    u16 *dst = gba_font;
    for(x = 0; x < 256*16*2; x++) {
      *src++ = *dst++;
    }

    for(x = 0; x < 1024; x++)
      MAP[8][0][x] = ' ';
    
    while(LCD_Y < 160) ;
    LCDMODE = 0 | LCDMODE_BG0;

    // Start timers
    REG_TM2CNT = 0x82;
    REG_TM3CNT = 0x84;    

    // Find beginning of gbfs
    gbfs = find_first_gbfs_file(find_first_gbfs_file);
    if (gbfs == NULL) {
        printf("\n   GBA Java Virtual Machine\n");
        printf("\n\n     a KVM port by Torlus\n  based on Sun's CLDC 1.1 RI\n");
        printf("\n\n\n  See the README file for\n  details");
        printf("\n\n\n  http://heliscar.com/greg/\n");

        while(1);
    }
    
    // Find first file name
    GBFS_ENTRY *e;
    if (gbfs->dir_nmemb > 1) {
        // Torlus ??? gbfs seems to swap 1st and 2nd entry in the archive
        e = (GBFS_ENTRY *)((char *)gbfs + gbfs->dir_off + sizeof(GBFS_ENTRY));
    } else {
        // One entry only
        e = (GBFS_ENTRY *)((char *)gbfs + gbfs->dir_off);        
    }

    //printf("First file [%s]\n",e->name);

    // Extract class name
    strcpy(className,e->name);
    p = strchr(className,'.');
    *p = 0;
        
    RequestedHeapSize = DEFAULTHEAPSIZE;
    UserClassPath = ".";
       
    /* Call the portable KVM startup routine */
    result = StartJVM(kvm_argc, kvm_argv);

    /* Loop forever */    
    while(1);

}
