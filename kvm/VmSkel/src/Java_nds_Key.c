#include <nds.h>
#include <nds/arm9/input.h>
#include <kni.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "standalone_game.h"

#ifndef STANDALONE_OUTPUT_BASENAME
#define STANDALONE_OUTPUT_BASENAME "game"
#endif

static char g_uiPath[256] = "fat:/pstro_launcher.keys";
static char g_uiGameName[32] = "Pstro Laucher";

#define PSTROS_VKEY_NUM7   (1u << 17)
#define PSTROS_VKEY_NUM9   (1u << 19)

#define UI_TAB_SINGLE 0
#define UI_TAB_COMBO  1
#define UI_TAB_LOG    2

#define UI_ROW_LIST_START 3
#define UI_ROWS_VISIBLE  10
#define UI_LOG_LINES     14
#define UI_TARGET_COUNT  10
#define UI_LOG_CAP       24

typedef struct {
    const char *name;
    unsigned int logicalBit;
} UiTargetDef;

static const UiTargetDef g_targets[UI_TARGET_COUNT] = {
    {"LS", KEY_L},
    {"RS", KEY_R},
    {"1", KEY_Y},
    {"3", KEY_A},
    {"5", KEY_B},
    {"7", PSTROS_VKEY_NUM7},
    {"9", PSTROS_VKEY_NUM9},
    {"*", KEY_SELECT},
    {"0", KEY_X},
    {"#", KEY_START}
};

static const unsigned int g_physicalKeys[] = {
    KEY_L, KEY_R, KEY_A, KEY_B, KEY_X, KEY_Y,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_START, KEY_SELECT
};
static const char *g_physicalNames[] = {
    "L", "R", "A", "B", "X", "Y", "UP", "DOWN", "LEFT", "RIGHT", "START", "SELECT"
};
#define PHYSICAL_COUNT ((int)(sizeof(g_physicalKeys)/sizeof(g_physicalKeys[0])))

static unsigned int g_singleMap[UI_TARGET_COUNT];
static unsigned int g_comboMap1[UI_TARGET_COUNT];
static unsigned int g_comboMap2[UI_TARGET_COUNT];
static unsigned int g_prevLogicalHeld;
static unsigned int g_lastHeld;
static unsigned int g_lastDown;
static unsigned int g_lastUp;
static int g_uiReady;
static int g_uiDirty;
static int g_uiTab;
static int g_uiScrollSingle;
static int g_uiScrollCombo;
static int g_uiSelectedSingle;
static int g_uiSelectedCombo;
static int g_uiCaptureSingle;
static int g_uiCaptureCombo;
static char g_statusAudio[32] = "AUDIO: disabled";
static char g_statusHang[32] = "HANG: none";
static char g_logLines[UI_LOG_CAP][32];
static int g_logCount;
static int g_logHead;
static int g_touchWasHeld;

void pstrosUiSetGameId(const char *gameId, const char *displayName) {
    char safe[96];
    int i = 0;
    int j = 0;
    if (gameId == NULL) gameId = "j2me_game";
    while (gameId[i] && j < (int)sizeof(safe) - 1) {
        unsigned char c = (unsigned char)gameId[i++];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
            safe[j++] = (char)c;
        } else if (j > 0 && safe[j - 1] != '_') {
            safe[j++] = '_';
        }
    }
    while (j > 0 && safe[j - 1] == '_') j--;
    if (j == 0) {
        strcpy(safe, "j2me_game");
    } else {
        safe[j] = 0;
    }
    snprintf(g_uiPath, sizeof(g_uiPath), "fat:/%s.keys", safe);
    if (displayName != NULL && displayName[0] != 0) {
        snprintf(g_uiGameName, sizeof(g_uiGameName), "%s", displayName);
    } else {
        snprintf(g_uiGameName, sizeof(g_uiGameName), "%s", safe);
    }
    g_uiReady = 0;
    g_uiDirty = 1;
}

static void uiSetRow(int row, const char *text) {
    iprintf("\x1b[%d;0H%-31.31s", row, text ? text : "");
}

static void uiLog(const char *fmt, ...) {
    char line[32];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    line[31] = '\0';
    strncpy(g_logLines[g_logHead], line, 31);
    g_logLines[g_logHead][31] = '\0';
    g_logHead = (g_logHead + 1) % UI_LOG_CAP;
    if (g_logCount < UI_LOG_CAP) g_logCount++;
    g_uiDirty = 1;
}

void pstrosUiSetStatusLine(int row, const char *text) {
    if (text == NULL) text = "";
    if (row == 2) {
        strncpy(g_statusAudio, text, sizeof(g_statusAudio) - 1);
        g_statusAudio[sizeof(g_statusAudio) - 1] = '\0';
        uiLog("%s", g_statusAudio);
    } else {
        strncpy(g_statusHang, text, sizeof(g_statusHang) - 1);
        g_statusHang[sizeof(g_statusHang) - 1] = '\0';
        uiLog("%s", g_statusHang);
    }
    g_uiDirty = 1;
}

static const char *physicalName(unsigned int bit) {
    int i;
    for (i = 0; i < PHYSICAL_COUNT; i++) {
        if (g_physicalKeys[i] == bit) return g_physicalNames[i];
    }
    return "-";
}

static void formatMap(char *out, int outSize, int target, int combo) {
    if (!out || outSize <= 0) return;
    if (combo) {
        if (g_comboMap1[target] && g_comboMap2[target]) {
            snprintf(out, outSize, "%s + %s", physicalName(g_comboMap1[target]), physicalName(g_comboMap2[target]));
        } else {
            snprintf(out, outSize, "-");
        }
    } else {
        if (g_singleMap[target]) snprintf(out, outSize, "%s", physicalName(g_singleMap[target]));
        else snprintf(out, outSize, "-");
    }
    out[outSize - 1] = '\0';
}

static void uiDefaults(void) {
    memset(g_singleMap, 0, sizeof(g_singleMap));
    memset(g_comboMap1, 0, sizeof(g_comboMap1));
    memset(g_comboMap2, 0, sizeof(g_comboMap2));
    g_singleMap[0] = KEY_L;       /* LS */
    g_singleMap[1] = KEY_R;       /* RS */
    g_singleMap[2] = KEY_Y;       /* 1 */
    g_singleMap[3] = KEY_A;       /* 3 */
    g_singleMap[4] = KEY_B;       /* 5 */
    g_singleMap[7] = KEY_SELECT;  /* * */
    g_singleMap[8] = KEY_X;       /* 0 */
    g_singleMap[9] = KEY_START;   /* # */
}

static void uiLoad(void) {
    FILE *fp;
    char tag[16];
    int idx;
    unsigned int a, b;
    uiDefaults();
    fp = fopen(g_uiPath, "rb");
    if (fp == NULL) {
        uiLog("cfg: defaults");
        return;
    }
    if (fscanf(fp, "%15s", tag) != 1 || strcmp(tag, "PSTROSKEYS4") != 0) {
        fclose(fp);
        uiLog("cfg: old ignored");
        return;
    }
    while (fscanf(fp, "%15s", tag) == 1) {
        if (strcmp(tag, "S") == 0) {
            if (fscanf(fp, "%d %u", &idx, &a) == 2 && idx >= 0 && idx < UI_TARGET_COUNT) {
                g_singleMap[idx] = a;
            }
        } else if (strcmp(tag, "C") == 0) {
            if (fscanf(fp, "%d %u %u", &idx, &a, &b) == 3 && idx >= 0 && idx < UI_TARGET_COUNT) {
                g_comboMap1[idx] = a;
                g_comboMap2[idx] = b;
            }
        } else {
            char discard[64];
            fgets(discard, sizeof(discard), fp);
        }
    }
    fclose(fp);
    uiLog("cfg: loaded");
}

static void uiSave(void) {
    FILE *fp;
    int i;
    fp = fopen(g_uiPath, "wb");
    if (fp == NULL) {
        uiLog("cfg: save fail");
        return;
    }
    fprintf(fp, "PSTROSKEYS4\n");
    for (i = 0; i < UI_TARGET_COUNT; i++) {
        fprintf(fp, "S %d %u\n", i, g_singleMap[i]);
    }
    for (i = 0; i < UI_TARGET_COUNT; i++) {
        fprintf(fp, "C %d %u %u\n", i, g_comboMap1[i], g_comboMap2[i]);
    }
    fclose(fp);
    uiLog("cfg: saved");
}

static void uiInit(void) {
    if (g_uiReady) return;
    memset(g_logLines, 0, sizeof(g_logLines));
    g_uiTab = UI_TAB_SINGLE;
    g_uiSelectedSingle = 0;
    g_uiSelectedCombo = 0;
    g_uiCaptureSingle = -1;
    g_uiCaptureCombo = -1;
    g_uiScrollSingle = 0;
    g_uiScrollCombo = 0;
    g_prevLogicalHeld = 0;
    g_touchWasHeld = 0;
    uiLoad();
    g_uiReady = 1;
    g_uiDirty = 1;
}

static void drawTabs(void) {
    char row[32];
    snprintf(row, sizeof(row), "%csingle%c %ccombo%c %clog%c",
             g_uiTab == UI_TAB_SINGLE ? '[' : ' ', g_uiTab == UI_TAB_SINGLE ? ']' : ' ',
             g_uiTab == UI_TAB_COMBO ? '[' : ' ', g_uiTab == UI_TAB_COMBO ? ']' : ' ',
             g_uiTab == UI_TAB_LOG ? '[' : ' ', g_uiTab == UI_TAB_LOG ? ']' : ' ');
    uiSetRow(0, row);
}

static void appendText(char *out, int outSize, const char *text) {
    int used;
    int left;
    if (out == NULL || outSize <= 1 || text == NULL) return;
    used = (int)strlen(out);
    left = outSize - used - 1;
    if (left <= 0) return;
    strncat(out, text, left);
    out[outSize - 1] = '\0';
}

static void appendSpaces(char *out, int outSize, int count) {
    while (count-- > 0 && (int)strlen(out) < outSize - 1) appendText(out, outSize, " ");
}

static void buildMapRow(char *row, int rowSize, int selected, const char *target, const char *mapping) {
    int targetWidth = 8;
    int len;
    if (row == NULL || rowSize <= 0) return;
    row[0] = '\0';
    appendText(row, rowSize, selected ? "> " : "  ");
    appendText(row, rowSize, target ? target : "-");
    len = target ? (int)strlen(target) : 1;
    if (len < targetWidth) appendSpaces(row, rowSize, targetWidth - len);
    appendText(row, rowSize, mapping ? mapping : "-");
}

static void drawSingleTab(void) {
    int i;
    char row[32];
    char map[16];
    if (g_uiCaptureSingle >= 0) uiSetRow(1, "single: touch row, press key");
    else { char gameLine[32]; snprintf(gameLine, sizeof(gameLine), "game: %.24s", g_uiGameName); uiSetRow(1, gameLine); }
    for (i = 0; i < UI_ROWS_VISIBLE; i++) {
        int idx = g_uiScrollSingle + i;
        if (idx >= UI_TARGET_COUNT) {
            uiSetRow(UI_ROW_LIST_START + i, "");
            continue;
        }
        formatMap(map, sizeof(map), idx, 0);
        buildMapRow(row, sizeof(row), idx == g_uiSelectedSingle, g_targets[idx].name, map);
        uiSetRow(UI_ROW_LIST_START + i, row);
    }
    uiSetRow(20, "D-pad = 2/4/6/8 fixed");
    uiSetRow(21, "[save]                ");
    uiSetRow(22, g_statusHang);
}

static void drawComboTab(void) {
    int i;
    char row[32];
    char map[16];
    if (g_uiCaptureCombo >= 0) uiSetRow(1, "combo: press 2 keys together");
    else { char gameLine[32]; snprintf(gameLine, sizeof(gameLine), "game: %.24s", g_uiGameName); uiSetRow(1, gameLine); }
    for (i = 0; i < UI_ROWS_VISIBLE; i++) {
        int idx = g_uiScrollCombo + i;
        if (idx >= UI_TARGET_COUNT) {
            uiSetRow(UI_ROW_LIST_START + i, "");
            continue;
        }
        formatMap(map, sizeof(map), idx, 1);
        buildMapRow(row, sizeof(row), idx == g_uiSelectedCombo, g_targets[idx].name, map);
        uiSetRow(UI_ROW_LIST_START + i, row);
    }
    uiSetRow(20, "D-pad = 2/4/6/8 fixed");
    uiSetRow(21, "[save]                ");
    uiSetRow(22, g_statusHang);
}

static void drawLogTab(void) {
    int i;
    int start = g_logHead - g_logCount;
    if (start < 0) start += UI_LOG_CAP;
    uiSetRow(1, "recent log");
    for (i = 0; i < UI_LOG_LINES; i++) {
        int idx = g_logCount - UI_LOG_LINES + i;
        if (idx < 0 || idx >= g_logCount) {
            uiSetRow(UI_ROW_LIST_START + i, "");
        } else {
            int pos = (start + idx) % UI_LOG_CAP;
            uiSetRow(UI_ROW_LIST_START + i, g_logLines[pos]);
        }
    }
    uiSetRow(20, g_statusAudio);
    uiSetRow(21, "[save]                ");
    uiSetRow(22, g_statusHang);
}

static void uiRedraw(void) {
    if (!g_uiDirty) return;
    iprintf("\x1b[2J\x1b[0;0H");
    drawTabs();
    if (g_uiTab == UI_TAB_SINGLE) drawSingleTab();
    else if (g_uiTab == UI_TAB_COMBO) drawComboTab();
    else drawLogTab();
    g_uiDirty = 0;
}

void pstrosUiActivate(void) {
    uiInit();
    g_uiDirty = 1;
    uiRedraw();
}

static int firstPhysicalBit(unsigned int mask) {
    int i;
    for (i = 0; i < PHYSICAL_COUNT; i++) if (mask & g_physicalKeys[i]) return (int)g_physicalKeys[i];
    return 0;
}

static void firstTwoPhysicalBits(unsigned int mask, unsigned int *a, unsigned int *b) {
    int i;
    *a = 0; *b = 0;
    for (i = 0; i < PHYSICAL_COUNT; i++) {
        if (mask & g_physicalKeys[i]) {
            if (*a == 0) *a = g_physicalKeys[i];
            else if (*b == 0) { *b = g_physicalKeys[i]; return; }
        }
    }
}

static void uiHandleTouch(unsigned int rawDown, unsigned int rawHeld) {
    touchPosition pos;
    int yrow;
    int pressed;
    if ((rawHeld & KEY_TOUCH) == 0) {
        g_touchWasHeld = 0;
        return;
    }
    touchRead(&pos);
    if (pos.px < 0 || pos.py < 0) {
        g_touchWasHeld = 1;
        return;
    }
    pressed = ((rawDown & KEY_TOUCH) != 0) || !g_touchWasHeld;
    g_touchWasHeld = 1;
    if (!pressed) return;
    if (pos.py < 18) {
        if (pos.px < 80) g_uiTab = UI_TAB_SINGLE;
        else if (pos.px < 160) g_uiTab = UI_TAB_COMBO;
        else g_uiTab = UI_TAB_LOG;
        g_uiDirty = 1;
        return;
    }
    if (pos.py >= 168) {
        if (pos.px < 96) {
            uiSave();
            g_uiDirty = 1;
        }
        return;
    }
    if ((g_uiTab == UI_TAB_SINGLE || g_uiTab == UI_TAB_COMBO) && pos.py >= 24 && pos.py < 160) {
        if (pos.px > 232) {
            if (pos.py < 96) {
                if (g_uiTab == UI_TAB_SINGLE && g_uiScrollSingle > 0) g_uiScrollSingle--;
                if (g_uiTab == UI_TAB_COMBO && g_uiScrollCombo > 0) g_uiScrollCombo--;
            } else {
                if (g_uiTab == UI_TAB_SINGLE && g_uiScrollSingle < UI_TARGET_COUNT - UI_ROWS_VISIBLE) g_uiScrollSingle++;
                if (g_uiTab == UI_TAB_COMBO && g_uiScrollCombo < UI_TARGET_COUNT - UI_ROWS_VISIBLE) g_uiScrollCombo++;
            }
            g_uiDirty = 1;
            return;
        }
        yrow = (pos.py - 24) / 10;
        if (yrow < 0) yrow = 0;
        if (yrow >= UI_ROWS_VISIBLE) yrow = UI_ROWS_VISIBLE - 1;
        if (g_uiTab == UI_TAB_SINGLE) {
            g_uiSelectedSingle = g_uiScrollSingle + yrow;
            if (g_uiSelectedSingle >= UI_TARGET_COUNT) g_uiSelectedSingle = UI_TARGET_COUNT - 1;
            g_uiCaptureSingle = g_uiSelectedSingle;
            uiLog("single: %s", g_targets[g_uiSelectedSingle].name);
        } else {
            g_uiSelectedCombo = g_uiScrollCombo + yrow;
            if (g_uiSelectedCombo >= UI_TARGET_COUNT) g_uiSelectedCombo = UI_TARGET_COUNT - 1;
            g_uiCaptureCombo = g_uiSelectedCombo;
            uiLog("combo: %s", g_targets[g_uiSelectedCombo].name);
        }
        g_uiDirty = 1;
    }
}

static unsigned int buildLogicalHeld(unsigned int rawHeld) {
    unsigned int activeComboBits = 0;
    unsigned int out = rawHeld & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT);
    int i;
    for (i = 0; i < UI_TARGET_COUNT; i++) {
        if (g_comboMap1[i] && g_comboMap2[i]) {
            unsigned int need = g_comboMap1[i] | g_comboMap2[i];
            if ((rawHeld & need) == need) {
                out |= g_targets[i].logicalBit;
                activeComboBits |= need;
            }
        }
    }
    for (i = 0; i < UI_TARGET_COUNT; i++) {
        if (g_singleMap[i]) {
            if ((rawHeld & g_singleMap[i]) != 0 && (activeComboBits & g_singleMap[i]) == 0) {
                out |= g_targets[i].logicalBit;
            }
        }
    }
    return out;
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Key_scan() {
    unsigned int rawHeld, rawDown, logicalHeld;
    uiInit();
    scanKeys();
    rawHeld = keysHeld() & ~(KEY_TOUCH | KEY_LID);
    rawDown = keysDown() & ~(KEY_TOUCH | KEY_LID);
    uiHandleTouch(keysDown(), keysHeld());
    if (g_uiCaptureSingle >= 0) {
        int key = firstPhysicalBit(rawDown);
        if (key) {
            g_singleMap[g_uiCaptureSingle] = (unsigned int)key;
            uiLog("set %s=%s", g_targets[g_uiCaptureSingle].name, physicalName((unsigned int)key));
            g_uiCaptureSingle = -1;
            g_uiDirty = 1;
        }
    }
    if (g_uiCaptureCombo >= 0) {
        unsigned int a, b;
        firstTwoPhysicalBits(rawHeld, &a, &b);
        if (a && b) {
            g_comboMap1[g_uiCaptureCombo] = a;
            g_comboMap2[g_uiCaptureCombo] = b;
            uiLog("set %s=%s + %s", g_targets[g_uiCaptureCombo].name, physicalName(a), physicalName(b));
            g_uiCaptureCombo = -1;
            g_uiDirty = 1;
        }
    }
    if (g_uiCaptureSingle >= 0 || g_uiCaptureCombo >= 0 || (keysHeld() & KEY_TOUCH)) {
        rawHeld = 0;
    }
    logicalHeld = buildLogicalHeld(rawHeld);
    g_lastDown = logicalHeld & ~g_prevLogicalHeld;
    g_lastUp = g_prevLogicalHeld & ~logicalHeld;
    g_lastHeld = logicalHeld;
    g_prevLogicalHeld = logicalHeld;
    uiRedraw();
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Key_held() {
    KNI_ReturnInt((jint)g_lastHeld);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Key_down() {
    KNI_ReturnInt((jint)g_lastDown);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Key_downRepeat() {
    KNI_ReturnInt((jint)g_lastDown);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Key_up() {
    KNI_ReturnInt((jint)g_lastUp);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Key_setRepeat() {
    jint delay = KNI_GetParameterAsInt(1);
    jint repeat = KNI_GetParameterAsInt(2);
    keysSetRepeat(delay, repeat);
    KNI_ReturnVoid();
}
