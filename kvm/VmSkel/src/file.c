// Standalone property storage for Pstros/KVM.
// No JAD browser or external game file is used in this build.

#include <global.h>
#include <jam.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char fileName[256];

static char *propertyBuffer = NULL;
static char propertyEntry[512];
static int pstrosNokiaFullCanvasMode = 0;

void pstrosSetNokiaFullCanvasMode(int enabled) {
    pstrosNokiaFullCanvasMode = enabled ? 1 : 0;
}

extern char *UserClassPath;

static int pstrosCopyProperty(const char *key, char *out, int outSize) {
    int entryLength;
    char *value;
    if (propertyBuffer == NULL || key == NULL || out == NULL || outSize <= 1) return 0;
    value = JamGetProp(propertyBuffer, (char *)key, &entryLength);
    if (value == NULL || entryLength <= 0) return 0;
    if (entryLength >= outSize) entryLength = outSize - 1;
    memcpy(out, value, entryLength);
    out[entryLength] = 0;
    return 1;
}

void pstrosGetGameResolution(int *width, int *height) {
    static const char *keys[] = {
        "NDS-resolution",
        "Nokia-MIDlet-Original-Display-Size",
        "MIDlet-Original-Display-Size",
        "LGE-MIDlet-Display-Size",
        "Screen-Size"
    };
    char temp[64];
    int i;
    int w = 176;
    int h = 192;
    int found = 0;
    for (i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++) {
        char *p;
        if (!pstrosCopyProperty(keys[i], temp, sizeof(temp))) continue;
        for (p = temp; *p; p++) {
            if (*p == 'X' || *p == '*' || *p == ',' || *p == ' ') *p = 'x';
        }
        if (sscanf(temp, "%dx%d", &w, &h) == 2 && w >= 64 && h >= 64 && w <= 1024 && h <= 1024) {
            found = 1;
            break;
        }
        w = 176;
        h = 192;
    }

    /* Many old JAR manifests omit the original LCD size. Accept a clear
     * WIDTHxHEIGHT token from the filename, for example game_240x320.jar. */
    if (!found && UserClassPath != NULL) {
        const char *p = UserClassPath;
        while (*p) {
            int fw = 0;
            int fh = 0;
            if (*p >= '0' && *p <= '9' &&
                sscanf(p, "%dx%d", &fw, &fh) == 2 &&
                fw >= 64 && fh >= 64 && fw <= 1024 && fh <= 1024) {
                w = fw;
                h = fh;
                found = 1;
                break;
            }
            p++;
        }
    }

    *width = w;
    *height = h;
}

void pstrosGetGameCanvasInfo(int *width, int *height, int *x, int *y) {
    int w = 176;
    int h = 192;
    pstrosGetGameResolution(&w, &h);
    if (width) *width = w;
    if (height) *height = h;
    if (x) *x = (256 - w) / 2;
    if (y) *y = (192 - h) / 2;
}

char *getPlatformProperty(char *key) {
    int entryLength;
    char *value;

    if (key == NULL) return NULL;

    /* Nokia FullCanvas compatibility: ROMized FullCanvas does not translate
     * the normal MIDP KEY_NUM5 code correctly in some old S40 games. For JARs
     * detected as extending FullCanvas, expose logical B/5 as center FIRE. */
    if (pstrosNokiaFullCanvasMode && strcmp(key, "NDS-key-B") == 0) {
        return "keyFire";
    }

    /* Always expose one resolved logical Canvas size. This keeps Java's
     * Display.WIDTH/HEIGHT and the native centering/scaler in sync even when
     * the original manifest omitted NDS-resolution and the size was inferred
     * from a WIDTHxHEIGHT token in the JAR filename. */
    if (strcmp(key, "NDS-resolution") == 0) {
        int width;
        int height;
        pstrosGetGameResolution(&width, &height);
        snprintf(propertyEntry, sizeof(propertyEntry), "%dx%d", width, height);
        return propertyEntry;
    }

    /*
     * Launcher default positioning: keep the game's own canvas size and place
     * it in the center of the 256x192 NDS screen. Oversized canvases receive a
     * negative offset so cropping remains centered instead of starting at the
     * top-left corner. Force Fit is handled dynamically in the native final
     * framebuffer blit and therefore does not require restarting the MIDlet.
     */
    if (strcmp(key, "NDS-screen-X") == 0 || strcmp(key, "NDS-screen-Y") == 0) {
        int width;
        int height;
        int position;
        pstrosGetGameResolution(&width, &height);
        position = strcmp(key, "NDS-screen-X") == 0 ? (256 - width) / 2 : (192 - height) / 2;
        snprintf(propertyEntry, sizeof(propertyEntry), "%d", position);
        return propertyEntry;
    }

    if (propertyBuffer == NULL) return NULL;

    value = JamGetProp(propertyBuffer, key, &entryLength);
    if (value == NULL || entryLength < 0) return NULL;
    if (entryLength >= (int)sizeof(propertyEntry)) entryLength = (int)sizeof(propertyEntry) - 1;
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
