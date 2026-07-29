//Torlus - intermediate work file

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

#include <global.h>

#include <stdio.h>
#include <string.h>
#include "gbfs.h"

GBFS_FILE *gbfs;

char fs[256*1024];

int main (int argc, char *argv[]) {
    int result;
    char className[128];
    char *kvm_argv[1] = { className };
    int kvm_argc = 1;
  
    // Reads the contents of gbfs archive
    FILE *f;
    int b;
    char *p = fs;
    
    f = fopen(argv[1],"rb");
    while( (b=fgetc(f)) >= 0 ) {
      *p++ = (unsigned char)b;
    }
    fclose(f);

    // Find beginning of gbfs
    gbfs = find_first_gbfs_file(fs);
    // Find first file name
    GBFS_ENTRY *e = (GBFS_ENTRY *)((char *)gbfs + gbfs->dir_off);
  
    // Extract class name
    strcpy(className,e->name);
    p = strchr(className,'.');
    *p = 0;
    
    RequestedHeapSize = DEFAULTHEAPSIZE;
    UserClassPath = ".";
   
    /* Call the portable KVM startup routine */
    result = StartJVM(kvm_argc, kvm_argv);
    return result;
}

