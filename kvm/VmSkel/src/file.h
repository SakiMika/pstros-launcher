#ifndef PSTROS_FILE_H
#define PSTROS_FILE_H

char *loadFile(void);
extern char fileName[256];
char *getPlatformProperty(char *key);
void initJadBuffer(void);
int setJadBufferText(const char *text);
void freeJadBuffer(void);

#endif
