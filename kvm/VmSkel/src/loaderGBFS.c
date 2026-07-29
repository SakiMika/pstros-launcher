//Torlus - based on loaderFile.c in VmExtra directory

/*
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 */

/*=========================================================================
 * SYSTEM:    KVM
 * SUBSYSTEM: Class loader
 * FILE:      loaderFile.c
 * OVERVIEW:  Structures and operations needed for loading
 *            Java classfiles from the file system.  This file
 *            (loaderFile.c) provides the default file system
 *            operations that are typically very useful on all those
 *            target platforms that have a regular file system.
 *
 *            These operations have been separated from the actual
 *            class loading operations (VmCommon/src/loader.c), since
 *            the mapping between the KVM and the Java classfile
 *            storage mechanisms of the host operating system can vary
 *            considerably in different embedded devices.
 * AUTHOR:    Tasneem Sayeed, Consumer & Embedded division
 *            Frank Yellin
 *            Kinsley Wong
 *=======================================================================*/

/*=========================================================================
 * Include files
 *=======================================================================*/

#include <global.h>
#include <stdio.h>
#include <jar.h>

#include "gbfs.h"
extern GBFS_FILE *gbfs;

/*=========================================================================
 * Definitions and declarations
 *=======================================================================*/

struct filePointerStruct {
    /* If set to true, indicates a JAR file */
    bool_t isJarFile;
};

struct stdioPointerStruct {
    bool_t isJarFile;      /* Always TRUE */
    long   size;           /* size of file in bytes */
//Torlus
    unsigned char   *start;
    int   offset;
};

struct jarPointerStruct {
    bool_t isJarFile;      /* always FALSE */
    long dataLen;          /* length of data stream */
    long dataIndex;        /* current position for reading */
    unsigned char data[1];
};

typedef struct classPathEntryStruct {
    union {
        struct jarInfoStruct jarInfo; /* if it's a jar file */
        /* Leave possibility of other types, for later */
    } u;
    char  type;
    char  name[1];
} *CLASS_PATH_ENTRY, **CLASS_PATH_ENTRY_HANDLE;

/*=========================================================================
 * Variables
 *=======================================================================*/

/* A table for storing the individual directory paths along classpath.
 * Each entry is one element in the list.
 *
 * An entry consists of the letter 'j' or 'd' followed by more information.
 * 'd' means this is a directory.  It is immediately followed by the name
 *      of the directory.
 * 'j' means a zip file.  It is followed by 3 ignored spaces, then 4 bytes
 *      giving the location of the first local header, 4 bytes giving
 *      the location of the first central header, and then the name of the
 *      zip file.
 */

/* Remember: This table contains object pointers (= GC root) */
static POINTERLIST ClassPathTable = NIL;

/* Set in main() to classpath environment */
char* UserClassPath = NULL;

static unsigned int MaxClassPathTableLength = 0;

/*
 * pointerlist that maintains mapping from file descriptor to filepointer for
 * resource files in MIDP
 */

POINTERLIST filePointerRoot = NULL;

/*=========================================================================
 * Static operations (used only in this file)
 *=======================================================================*/

static FILEPOINTER openClassfileInternal(BYTES_HANDLE);

/*=========================================================================
 * Class loading file operations
 *=======================================================================*/

/*=========================================================================
 * NOTE:      The functions below provide an implementation of
 *            the abstract class loading interface defined in
 *            VmCommon/h/loader.h.  The implementation here is
 *            suitable only for those target platforms that have
 *            a conventional file system.
 *=======================================================================*/

/*=========================================================================
 * FUNCTION:      openClassFile
 * TYPE:          class reading
 * OVERVIEW:      Returns a FILEPOINTER object to read the bytes of the
 *                given class name
 * INTERFACE:
 *   parameters:  clazz:  Clazz to open
 *   returns:     a FILEPOINTER, or error if none exists
 *=======================================================================*/

FILEPOINTER
openClassfile(INSTANCE_CLASS clazz) {
    FILEPOINTER ClassFile;
    START_TEMPORARY_ROOTS
        UString UPackageName = clazz->clazz.packageName;
        UString UBaseName    = clazz->clazz.baseName;
        int     baseLength = UBaseName->length;
        int     packageLength = UPackageName == 0 ? 0 : UPackageName->length;
        int     length = packageLength + 1 + baseLength + 7;
        DECLARE_TEMPORARY_ROOT(char *, fileName, mallocBytes(length));
        char    *to;

        ASSERTING_NO_ALLOCATION
            to = fileName;

            if (UPackageName != NULL) {
                int packageLength = UPackageName->length;
                memcpy(to, UStringInfo(UPackageName), packageLength);
                to += packageLength;
                *to++ = '/';
            }
            memcpy(to, UStringInfo(UBaseName), baseLength);
            to += baseLength;
            strcpy(to, ".class");
        END_ASSERTING_NO_ALLOCATION

        ClassFile = openClassfileInternal(&fileName);
        if (ClassFile == NULL) {
#if INCLUDEDEBUGCODE
            if (traceclassloadingverbose) {
                DECLARE_TEMPORARY_ROOT(char *, className,
                    getClassName((CLASS)clazz));
                sprintf(str_buffer, KVM_MSG_CLASS_NOT_FOUND_1STRPARAM, className);
                fprintf(stdout, str_buffer);
            }
#endif /* INCLUDEDEBUGCODE */
        }
    END_TEMPORARY_ROOTS
    return ClassFile;
}

/*=========================================================================
 * FUNCTION:      openResourceFile
 * TYPE:          resource reading
 * OVERVIEW:      Returns a FILEPOINTER object to read the bytes of the
 *                given resource name
 * INTERFACE:
 *   parameters:  resourceName:  Name of resource to read
 *   returns:     a FILEPOINTER, or error if none exists
 *=======================================================================*/

FILEPOINTER
openResourcefile(BYTES resourceNameArg) {
    FILEPOINTER ClassFile;
    START_TEMPORARY_ROOTS
        DECLARE_TEMPORARY_ROOT(BYTES, resourceName, resourceNameArg);
        ClassFile = openClassfileInternal(&resourceName);
        if (ClassFile == NULL) {
#if INCLUDEDEBUGCODE
            if (traceclassloadingverbose) {
                sprintf(str_buffer,
                        KVM_MSG_RESOURCE_NOT_FOUND_1STRPARAM, resourceName);
                fprintf(stdout, str_buffer);
            }
#endif /* INCLUDEDEBUGCODE */
        }
    END_TEMPORARY_ROOTS
    return ClassFile;
}

/*=========================================================================
 * FUNCTION:      openClassfileInternal()
 * TYPE:          class reading
 * OVERVIEW:      Internal function used by openClassfile().
 *                It returns a FILE* for reading the class if a file exists.
 *                NULL otherwise.
 * INTERFACE:
 *   parameters:  fileName:  Name of class file to read
 *   returns:     FILE* for reading the class, or NULL.
 *=======================================================================*/

static FILEPOINTER
openClassfileInternal(BYTES_HANDLE filenameH) {
    int filenameLength = strlen(unhand(filenameH));
    int paths = ClassPathTable->length;
    int i;
    //Torlus
    unsigned char *gbfs_entry_start = NULL;
    long gbfs_entry_length = 0;
    FILEPOINTER fp = NULL;

    START_TEMPORARY_ROOTS
        int fullnameLength = MaxClassPathTableLength + filenameLength + 2;
        DECLARE_TEMPORARY_ROOT(char*, fullname,
            (char*)mallocBytes(fullnameLength));
        DECLARE_TEMPORARY_ROOT(CLASS_PATH_ENTRY, entry, NULL);
        for (i = 0; i < paths && fp == NULL; i++) {
            entry = (CLASS_PATH_ENTRY)ClassPathTable->data[i].cellp;
	    switch (entry->type) {
            case 'd':                      /* A directory */
//Torlus - there's no support for classpath for the moment
//We only have one path entry "."
		//sprintf(fullname, "%s/%s", entry->name, unhand(filenameH));
		sprintf(fullname, "%s", unhand(filenameH));
		    
		//Torlus
	        gbfs_entry_start = (unsigned char *)gbfs_get_obj(gbfs, fullname, &gbfs_entry_length);
                if (gbfs_entry_start != NULL) {
                    fp = (FILEPOINTER)
                        mallocBytes(sizeof(struct stdioPointerStruct));
                    fp->isJarFile = FALSE;
                    ((struct stdioPointerStruct *)fp)->size = gbfs_entry_length;
                    //Torlus
		    ((struct stdioPointerStruct *)fp)->start = gbfs_entry_start;
		    ((struct stdioPointerStruct *)fp)->offset = 0;
                }
                break;

            case 'j':  {                   /* A JAR file */
                long length;
                struct jarPointerStruct *result = (struct jarPointerStruct*)
                    loadJARFileEntry(&entry->u.jarInfo, unhand(filenameH),
                                     &length,
                                     offsetof(struct jarPointerStruct, data[0]));
                if (result != NULL) {
                    result->isJarFile = TRUE;
                    result->dataLen = length;
                    result->dataIndex = 0;
                    fp = (FILEPOINTER)result;
                }
                break;
            }
            }
        }
    END_TEMPORARY_ROOTS
    return fp;
}

/*=========================================================================
 * FUNCTION:      loadByte(), loadShort(), loadCell()
 *                loadBytes, skipBytes()
 * TYPE:          private class file reading operations
 * OVERVIEW:      Read the next 1, 2, 4 or n bytes from the
 *                given class file.
 * INTERFACE:
 *   parameters:  classfile pointer
 *   returns:     unsigned char, short or integer
 * NOTE:          For safety it might be a good idea to check
 *                explicitly for EOF in these operations.
 *=======================================================================*/

int
loadByteNoEOFCheck(FILEPOINTER_HANDLE ClassFileH)
{
    FILEPOINTER ClassFile = unhand(ClassFileH);
    if (!ClassFile->isJarFile) {
	//Torlus
        unsigned char *start = ((struct stdioPointerStruct*)ClassFile)->start;
        int offset = ((struct stdioPointerStruct*)ClassFile)->offset;
        int size = ((struct stdioPointerStruct*)ClassFile)->size;
	if (offset < size) {
	  int c = (int)start[offset++];
          ((struct stdioPointerStruct*)ClassFile)->offset = offset;
	  return c;
	} else {
	  return EOF;
	}
    } else {
        struct jarPointerStruct *ds = (struct jarPointerStruct *)ClassFile;
        if (ds->dataIndex < ds->dataLen) {
            return ds->data[ds->dataIndex++];
        } else {
            return -1;
        }
    }
}

unsigned char
loadByte(FILEPOINTER_HANDLE ClassFileH)
{
    int c = loadByteNoEOFCheck(ClassFileH);
    if (c == -1) {
        raiseExceptionWithMessage(ClassFormatError,
            KVM_MSG_CLASSFILE_SIZE_DOES_NOT_MATCH);
    }
    return (unsigned char)c;
}

unsigned short
loadShort(FILEPOINTER_HANDLE ClassFileH)
{
    unsigned char c1 = loadByte(ClassFileH);
    unsigned char c2 = loadByte(ClassFileH);
    unsigned short c  = c1 << 8 | c2;
    return c;
}

unsigned long
loadCell(FILEPOINTER_HANDLE ClassFileH)
{
    unsigned char c1 = loadByte(ClassFileH);
    unsigned char c2 = loadByte(ClassFileH);
    unsigned char c3 = loadByte(ClassFileH);
    unsigned char c4 = loadByte(ClassFileH);
    unsigned int c;
    c  = c1 << 24 | c2 << 16 | c3 << 8 | c4;
    return c;
}

static int
loadBytesInternal(FILEPOINTER_HANDLE ClassFileH, char *buffer, int pos,
    int length, bool_t checkEOF)
{
    int n;
    FILEPOINTER ClassFile = unhand(ClassFileH);

    if (!ClassFile->isJarFile) {
        //Torlus
	unsigned char *start = ((struct stdioPointerStruct*)ClassFile)->start;
	int offset = ((struct stdioPointerStruct*)ClassFile)->offset;
	int size = ((struct stdioPointerStruct*)ClassFile)->size;
        
	/*
         * If 'pos' is -1 then just read sequentially.  Used internally by
         * loadBytes() which is called from classloader.
         */

        if (pos != -1) {
	  //Torlus (replaces : fseek(file, pos, SEEK_SET))
	  offset = pos;
	  ((struct stdioPointerStruct*)ClassFile)->offset = pos;
	}
	//Torlus (replaces : n = fread(buffer, 1, length, file))
	n = 0;
	if ( (offset + length) <= size ) {
	  n = length;
	} else {
	  n = (size - offset);
	}
	memcpy(buffer, (start + offset), n);
	offset += n;
	((struct stdioPointerStruct*)ClassFile)->offset = offset;
	
        if (n != length) {
            if (checkEOF) {
                raiseExceptionWithMessage(ClassFormatError,
                    KVM_MSG_CLASSFILE_SIZE_DOES_NOT_MATCH);
            } else {
                if (n > 0)
                    return (n);
                else
                    return (-1);        /* eof */
            }
        } else {
            return (n);
        }
    } else {
        struct jarPointerStruct *ds = (struct jarPointerStruct *)ClassFile;
        int avail;
        if (pos == -1)
            pos = ds->dataIndex;
        avail = ds->dataLen - pos;
        if (avail < length && checkEOF) /* inconceivable? */{
                raiseExceptionWithMessage(ClassFormatError,
                    KVM_MSG_CLASSFILE_SIZE_DOES_NOT_MATCH);
        } else {
            if (avail) {
                int readLength = (avail > length) ? length : avail;
                memcpy(buffer, &ds->data[pos], readLength);
                ds->dataIndex = pos + readLength;
                return (readLength);
            } else {
                return (-1);
            }
        }
    }
    return (0);
}

void
loadBytes(FILEPOINTER_HANDLE ClassFileH, char *buffer, int length)
{
    /* check for eof */

  (void)loadBytesInternal(ClassFileH, buffer, -1, length, TRUE);
}

int
loadBytesNoEOFCheck(FILEPOINTER_HANDLE ClassFileH, char *buffer, int pos, int length)
{
    /* don't check for EOF */
    return (loadBytesInternal(ClassFileH, buffer, pos, length, FALSE));
}

int getBytesAvailable(FILEPOINTER_HANDLE ClassFileH)
{
    FILEPOINTER ClassFile = unhand(ClassFileH);
    if (!ClassFile->isJarFile) {
        int pos;
        struct stdioPointerStruct *ds = (struct stdioPointerStruct *)ClassFile;
        //Torlus
	//FILE *file = ds->file;
        //pos = ftell(file);
	pos = ds->offset;
	
        if (ds->size == 0 || pos == -1L) {
            return 0;    /* if error state, then just return 0, system will */
                         /* read byte by byte */
        } else {
            return (ds->size - pos);
        }
    } else {
        struct jarPointerStruct *ds = (struct jarPointerStruct *)ClassFile;
        return (ds->dataLen - ds->dataIndex);
    }
}

void
skipBytes(FILEPOINTER_HANDLE ClassFileH, unsigned long length)
{
    int pos;
    FILEPOINTER ClassFile = unhand(ClassFileH);

    if (!ClassFile->isJarFile) {
        struct stdioPointerStruct *ds = (struct stdioPointerStruct *)ClassFile;
        //Torlus
	//FILE *file = ds->file;
        //pos = ftell(file);
	unsigned char *start = ds->start;
	int offset = ds->offset;
	int size = ds->size;
        pos = offset;
	
        if (ds->size == 0 || pos == -1) {
            /* In some kind of error state, just do it byte by byte
             * and the EOF error will get returned if appropriate.
             */
            while (length-- > 0) {
                //Torlus
		//int c = getc(file);
		int c;
		if (offset < size) {
	  	  c = (int)(start[offset++]);
		  ds->offset = offset;
	        } else {
	          c = EOF;
	        }

                if (c == EOF) {
                    raiseExceptionWithMessage(ClassFormatError,
                        KVM_MSG_CLASSFILE_SIZE_DOES_NOT_MATCH);
                }
            }
        } else {
            if (pos + length > (unsigned int)ds->size) {
                raiseExceptionWithMessage(ClassFormatError,
                    KVM_MSG_CLASSFILE_SIZE_DOES_NOT_MATCH);
            } else {
		//Torlus
                //fseek(file, (pos + length), SEEK_SET);
		offset = pos + length;
	        ds->offset = offset;	
            }
        }
    } else {
        struct jarPointerStruct *ds = (struct jarPointerStruct *)ClassFile;
        unsigned int avail = ds->dataLen - ds->dataIndex;

        if (avail < length) {
            raiseExceptionWithMessage(ClassFormatError,
                KVM_MSG_CLASSFILE_SIZE_DOES_NOT_MATCH);
        } else {
            ds->dataIndex += length;
        }
    }
}

/*=========================================================================
 * FUNCTION:      closeClassfile()
 * TYPE:          private class file load operation
 * OVERVIEW:      Close the given classfile. Ensure that we have
 *                reached the end of the classfile.
 * INTERFACE:
 *   parameters:  file pointer
 *   returns:     <nothing>
 *=======================================================================*/

void closeClassfile(FILEPOINTER_HANDLE ClassFileH)
{
    FILEPOINTER ClassFile = unhand(ClassFileH);

#if INCLUDEDEBUGCODE
    if (traceclassloadingverbose) {
        fprintf(stdout, "Closing classfile\n");
    }
#endif /* INCLUDEDEBUGCODE */

    if (ClassFile->isJarFile) {
        /* Close the JAR datastream.   Don't need to do anything */
    } else {
        /* Close the classfile */
	//Torlus (do nothing)
        //FILE *file = ((struct stdioPointerStruct*)ClassFile)->file;
        //if (file != NULL) {
        //    fclose(file);
        //    ((struct stdioPointerStruct*)ClassFile)->file = NULL;
        //}
    }
}

/*=========================================================================
 * FUNCTION:      InitializeClassLoading()
 * TYPE:          constructor (kind of)
 * OVERVIEW:      Set up the dynamic class loader.
 *                Read the optional environment variable CLASSPATH
 *                and set up the class path for classfile loading
 *                correspondingly. Actual implementation of this
 *                function is platform-dependent.
 * INTERFACE:
 *   parameters:  <none>
 *   returns:     nothing directly, but the operation initializes
 *                the global classpath variables.
 *=======================================================================*/

void InitializeClassLoading()
{
    char *classpath = UserClassPath;
    int length, pathCount, i;

    int tableIndex;
    int previousI;

    /*  Count the number of individual directory paths along CLASSPATH */
    length = strlen(classpath);
    pathCount = 1;

    for (i = 0; i < length; i++) {
        if (classpath[i] == PATH_SEPARATOR) pathCount++;
    }

    /* We use callocObject() since we'll be allocating new objects to fill
     * up the table */
    ClassPathTable = (POINTERLIST)callocObject(SIZEOF_POINTERLIST(pathCount),
                                               GCT_POINTERLIST);
    makeGlobalRoot((cell **)&ClassPathTable);

    ClassPathTable->length = pathCount;
    MaxClassPathTableLength = 0;

    tableIndex = 0;
    previousI = 0;

    for (i = 0; i <= length; i++) {
       if (classpath[i] == PATH_SEPARATOR || classpath[i] == 0) {
           /* Create a new string containing the individual directory path.
            * We prepend either a 'j' (jar file) or 'd' (directory) indicating
            * the type of classpath element this is.
            */
           unsigned int length = i - previousI;
           CLASS_PATH_ENTRY result =
               (CLASS_PATH_ENTRY)mallocBytes(
                   sizeof(struct classPathEntryStruct) + length);

           /* Copy the name into the result buffer */
           memcpy(result->name, classpath + previousI, length);
           result->name[length] = '\0';
           result->type = '\0'; /* indicates a problem */

//Torlus - there's no support for classpath for the moment
//We only have one path entry "."
	   result->type = 'd';
	   
	   if (result->type == '\0') {
               /* bad entry  */
               ClassPathTable->length--;
           } else {
               ClassPathTable->data[tableIndex].cellp = (cell*)result;
               tableIndex++;
           }
           previousI = i+1;
       }
    }
}

/*=========================================================================
 * FUNCTION:      FinalizeClassLoading()
 * TYPE:          destructor (kind of)
 * OVERVIEW:      Shut down the dynamic class loader.
 *                Actual implementation of this function is
 *                platform-dependent.
 * INTERFACE:
 *   parameters:  <none>
 *   returns:     <nothing> (but closes JAR files as a side-effect)
 *=======================================================================*/

void FinalizeClassLoading()
{
    int paths = ClassPathTable->length;
    int i;
    for (i = 0; i < paths; i++) {
        CLASS_PATH_ENTRY entry =
            (CLASS_PATH_ENTRY)ClassPathTable->data[i].cellp;
        if (entry->type == 'j') {
            closeJARFile(&entry->u.jarInfo);
        }
    }
}

/*=========================================================================
 * FUNCTION:      setFilePointer
 * CLASS:
 * TYPE:
 * OVERVIEW:      given a filepointer store it in the global root and return
 *                a file descriptor
 * INTERFACE (operand stack manipulation):
 *   parameters:  filepointer
 *   returns:     integer file descriptor
 *=======================================================================*/

int setFilePointer(FILEPOINTER_HANDLE fph)
{
    int i;
    long length;
    FILEPOINTER fp;

    if (filePointerRoot == NULL) {
        /* This pointerlist maintains a mapping between an integer file
         * descriptor and filepointers for resource files.  Added to support
         * MIDP changes to ResourceInputStream.java for commonality between KVM
         * and Project Monty
         */

        filePointerRoot = (POINTERLIST)callocObject(SIZEOF_POINTERLIST(FILE_OBJECT_SIZE),
           GCT_POINTERLIST);
        filePointerRoot->length = FILE_OBJECT_SIZE;
        makeGlobalRoot((cell **)&filePointerRoot);
    }

    fp = unhand(fph);
    length = filePointerRoot->length;
    for (i = 0; i < length; i++) {
        if (filePointerRoot->data[i].cellp == NULL) {
            filePointerRoot->data[i].cellp = (cell *)fp;
            if (i < length - 1) {
                return i;
            } else {
                break;
            }
        }
    }
    {
    const int expansion = FILE_OBJECT_SIZE;
    int newLength = length + expansion;
    POINTERLIST tmpList = (POINTERLIST)callocObject(SIZEOF_POINTERLIST(newLength),
        GCT_POINTERLIST);
    tmpList->length = newLength;
    memcpy(tmpList->data, filePointerRoot->data, length << log2CELL);
    filePointerRoot = tmpList;
    return i;
    }
}

/*=========================================================================
 * FUNCTION:      getFilePointer
 * CLASS:
 * TYPE:
 * OVERVIEW:      given a file descriptor, return a file pointer
 * INTERFACE (operand stack manipulation):
 *   parameters:  file descriptor
 *   returns:     file pointer
 *=======================================================================*/

FILEPOINTER getFilePointer(int fd)
{
    if (fd == -1 || fd >= filePointerRoot->length) {
        return NULL;
    }
    return (FILEPOINTER)(filePointerRoot->data[fd].cellp);
}

/*=========================================================================
 * FUNCTION:      clearFilePointer
 * CLASS:
 * TYPE:
 * OVERVIEW:      given a file descriptor, clear this entry
 * INTERFACE (operand stack manipulation):
 *   parameters:  file descriptor
 *   returns:     nothing
 *=======================================================================*/

void clearFilePointer(int fd)
{
    if (fd == -1 || fd >= filePointerRoot->length) {
        return;
    }
    filePointerRoot->data[fd].cellp = NULL;
}

