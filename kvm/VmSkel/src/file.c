// Standalone property storage for Pstros/KVM.
// No JAD browser or external game file is used in this build.

#include <global.h>
#include <jam.h>
#include <stdlib.h>
#include <string.h>

char fileName[256];

static char *propertyBuffer = NULL;
static char propertyEntry[512];

char *getPlatformProperty(char *key) {
    int entryLength;
    char *value;

    if (propertyBuffer == NULL || key == NULL) {
        return NULL;
    }

    value = JamGetProp(propertyBuffer, key, &entryLength);
    if (value == NULL || entryLength < 0) {
        return NULL;
    }
    if (entryLength >= (int)sizeof(propertyEntry)) {
        entryLength = (int)sizeof(propertyEntry) - 1;
    }

    memcpy(propertyEntry, value, entryLength);
    propertyEntry[entryLength] = 0;
    return propertyEntry;
}

void initJadBuffer(void) {
    propertyBuffer = NULL;
}

void freeJadBuffer(void) {
    if (propertyBuffer != NULL) {
        free(propertyBuffer);
        propertyBuffer = NULL;
    }
}

int setJadBufferText(const char *text) {
    size_t length;

    if (text == NULL) {
        return 0;
    }

    freeJadBuffer();
    length = strlen(text);
    propertyBuffer = (char *)malloc(length + 1);
    if (propertyBuffer == NULL) {
        return 0;
    }
    memcpy(propertyBuffer, text, length + 1);
    return 1;
}

char *loadFile(void) {
    return NULL;
}
