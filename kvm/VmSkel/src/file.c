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

    if (key == NULL) {
        return NULL;
    }

    /*
     * Launcher policy: every game receives the native Nintendo DS canvas.
     * Do not inherit NDS-resolution / position values from the selected JAR,
     * because launcher mode is intended for quick full-screen compatibility
     * testing after the game has been adapted to 256x192.
     */
    if (strcmp(key, "NDS-resolution") == 0) {
        return "256x192";
    }
    if (strcmp(key, "NDS-screen-X") == 0 || strcmp(key, "NDS-screen-Y") == 0) {
        return "0";
    }

    if (propertyBuffer == NULL) {
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
