/* Pstro Laucher: FAT JAR chooser for quick J2ME compatibility testing. */

#include <global.h>
#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "file.h"
#include "standalone_game.h"

#define LAUNCHER_MAX_GAMES       128
#define LAUNCHER_PATH_MAX        255
#define LAUNCHER_LABEL_MAX       63
#define LAUNCHER_SCAN_DEPTH      12
#define LAUNCHER_VISIBLE_ROWS    17
#define LAUNCHER_MANIFEST_MAX    (64 * 1024)
#define LAUNCHER_ZIP_TAIL_MAX    (0xFFFF + 22)

extern char *UserClassPath;
extern void kvm_vblank_handler(void);
extern void pstrosSetVmConsoleEnabled(int enabled);
extern void pstrosAudioDiagVmError(const char *message);
extern void pstrosAudioDiagKvmExit(int result);
extern int pstrosConfigureSaveStorageForGame(const char *gameId);
extern const char *pstrosGetSavePath(void);
extern void pstrosUiSetGameId(const char *gameId, const char *displayName);
extern void pstrosUiSetGamePath(const char *jarPath);
extern void pstrosUiActivate(void);
extern void pstrosSetNokiaFullCanvasMode(int enabled);
extern int pstrosLauncherInflateRaw(const unsigned char *compressed,
                                    int compressedLength,
                                    unsigned char *output,
                                    int outputLength);

typedef struct {
    char path[LAUNCHER_PATH_MAX + 1];
    char label[LAUNCHER_LABEL_MAX + 1];
} LauncherGame;

static LauncherGame g_games[LAUNCHER_MAX_GAMES];
static int g_gameCount;
static int g_selected;
static int g_scroll;
static int g_lastTouchRow = -1;
static int g_scanDirsOpened;
static int g_scanEntriesSeen;
static int g_scanRootOpened;
static char g_scanCwd[LAUNCHER_PATH_MAX + 1];
static char g_launcherDir[LAUNCHER_PATH_MAX + 1];

static unsigned int readLe16(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned long readLe32(const unsigned char *p) {
    return (unsigned long)p[0] |
           ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}

static int endsWithJar(const char *name) {
    size_t n;
    if (name == NULL) return 0;
    n = strlen(name);
    if (n < 4) return 0;
    return name[n - 4] == '.' &&
           tolower((unsigned char)name[n - 3]) == 'j' &&
           tolower((unsigned char)name[n - 2]) == 'a' &&
           tolower((unsigned char)name[n - 1]) == 'r';
}

static int nameEqualsIgnoreCase(const char *a, const char *b) {
    if (a == NULL || b == NULL) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static const char *pathBaseName(const char *path) {
    const char *a;
    const char *b;
    const char *base;
    if (path == NULL) return "game.jar";
    a = strrchr(path, '/');
    b = strrchr(path, '\\');
    base = path;
    if (a != NULL && a + 1 > base) base = a + 1;
    if (b != NULL && b + 1 > base) base = b + 1;
    return base;
}

static void makeGameId(const char *path, char *out, int outSize) {
    const char *base = pathBaseName(path);
    int i = 0;
    int j = 0;
    if (out == NULL || outSize <= 0) return;
    while (base[i] && j < outSize - 1) {
        unsigned char c = (unsigned char)base[i++];
        if (c == '.' && nameEqualsIgnoreCase(base + i - 1, ".jar")) break;
        if (isalnum(c)) out[j++] = (char)tolower(c);
        else if (j > 0 && out[j - 1] != '_') out[j++] = '_';
    }
    while (j > 0 && out[j - 1] == '_') j--;
    if (j == 0) {
        strncpy(out, "j2me_game", outSize - 1);
        out[outSize - 1] = 0;
    } else {
        out[j] = 0;
    }
}

static int gameAlreadyAdded(const char *path) {
    int i;
    if (path == NULL) return 1;
    for (i = 0; i < g_gameCount; i++) {
        if (nameEqualsIgnoreCase(g_games[i].path, path)) return 1;
    }
    return 0;
}

static void addGame(const char *path) {
    const char *base;
    if (g_gameCount >= LAUNCHER_MAX_GAMES || path == NULL || gameAlreadyAdded(path)) return;
    base = pathBaseName(path);
    snprintf(g_games[g_gameCount].path, sizeof(g_games[g_gameCount].path), "%s", path);
    snprintf(g_games[g_gameCount].label, sizeof(g_games[g_gameCount].label), "%s", base);
    g_gameCount++;
}

/* Do not depend on stat() here. Some real DLDI/libfat combinations enumerate
 * directory entries correctly but reject stat("fat:/...") for those same
 * entries. JAR names are accepted directly from readdir(); every other entry
 * is probed with opendir(), which is enough to detect subdirectories. */
static void scanDirectory(const char *directory, int depth) {
    DIR *dir;
    struct dirent *entry;
    if (directory == NULL || directory[0] == 0 || depth > LAUNCHER_SCAN_DEPTH ||
        g_gameCount >= LAUNCHER_MAX_GAMES) return;
    dir = opendir(directory);
    if (dir == NULL) return;
    if (depth == 0) g_scanRootOpened = 1;
    g_scanDirsOpened++;
    while ((entry = readdir(dir)) != NULL && g_gameCount < LAUNCHER_MAX_GAMES) {
        char path[LAUNCHER_PATH_MAX + 1];
        DIR *child;
        size_t len;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (entry->d_name[0] == '.') continue;
        g_scanEntriesSeen++;
        len = strlen(directory);
        snprintf(path, sizeof(path), "%s%s%s",
                 directory,
                 (len > 0 && directory[len - 1] == '/') ? "" : "/",
                 entry->d_name);
        path[sizeof(path) - 1] = 0;

        if (endsWithJar(entry->d_name)) {
            addGame(path);
            continue;
        }
        if (depth >= LAUNCHER_SCAN_DEPTH) continue;
        child = opendir(path);
        if (child != NULL) {
            closedir(child);
            scanDirectory(path, depth + 1);
        }
    }
    closedir(dir);
}

static int compareGames(const void *a, const void *b) {
    const LauncherGame *ga = (const LauncherGame *)a;
    const LauncherGame *gb = (const LauncherGame *)b;
    const unsigned char *pa = (const unsigned char *)ga->label;
    const unsigned char *pb = (const unsigned char *)gb->label;
    while (*pa && *pb) {
        int ca = tolower(*pa++);
        int cb = tolower(*pb++);
        if (ca != cb) return ca - cb;
    }
    return (int)*pa - (int)*pb;
}

static void dirnameFromPath(const char *path, char *out, int outSize) {
    const char *slashA;
    const char *slashB;
    const char *slash;
    int len;
    if (out == NULL || outSize <= 0) return;
    out[0] = 0;
    if (path == NULL || path[0] == 0) return;
    slashA = strrchr(path, '/');
    slashB = strrchr(path, '\\');
    slash = slashA;
    if (slashB != NULL && (slash == NULL || slashB > slash)) slash = slashB;
    if (slash == NULL) return;
    len = (int)(slash - path);
    if (len <= 0) len = 1;
    if (len >= outSize) len = outSize - 1;
    memcpy(out, path, len);
    out[len] = 0;
}

static void scanGames(void) {
    g_gameCount = 0;
    g_selected = 0;
    g_scroll = 0;
    g_lastTouchRow = -1;
    g_scanDirsOpened = 0;
    g_scanEntriesSeen = 0;
    g_scanRootOpened = 0;

    /* The canonical libfat mount. */
    scanDirectory("fat:/", 0);

    /* Fallbacks for loaders that set a useful working directory or expose the
     * same volume through a slash-root path. Only use them when the canonical
     * scan found nothing, avoiding duplicate entries. */
    if (g_gameCount == 0 && g_launcherDir[0] != 0) scanDirectory(g_launcherDir, 0);
    if (g_gameCount == 0 && g_scanCwd[0] != 0) scanDirectory(g_scanCwd, 0);
    if (g_gameCount == 0) scanDirectory("/", 0);
    if (g_gameCount == 0) scanDirectory("fat:", 0);
    if (g_gameCount == 0) scanDirectory("sd:/", 0);

    if (g_gameCount > 1) qsort(g_games, g_gameCount, sizeof(g_games[0]), compareGames);
}

static void setConsoleRow(int row, const char *text) {
    char padded[32];
    int i = 0;
    const char *src = text ? text : "";
    while (i < 31 && src[i] != 0) {
        padded[i] = src[i];
        i++;
    }
    while (i < 31) padded[i++] = ' ';
    padded[31] = 0;
    iprintf("\x1b[%d;0H", row);
    iprintf("%s", padded);
}

/* Draw sequentially instead of relying on many cursor-positioned writes.
 * Some real DS console/DLDI combinations showed only the first two rows even
 * though the scanner had found games. Sequential output is the most robust
 * path and also makes the selection marker clearly visible. */
static void drawLauncher(void) {
    int i;
    char line[96];
    iprintf("\x1b[2J\x1b[0;0H");
    iprintf("Pstro Laucher - FAT JAR\n");

    if (g_gameCount == 0) {
        iprintf("No .jar found on FAT\n\n");
        iprintf("Scanned fat:/ recursively\n");
        iprintf("dirs=%d entries=%d\n", g_scanDirsOpened, g_scanEntriesSeen);
        if (g_scanCwd[0] != 0) iprintf("cwd: %s\n", g_scanCwd);
        iprintf("\nB = rescan\n");
        return;
    }

    iprintf("%d game(s)  A=run B=rescan\n", g_gameCount);
    iprintf("-------------------------------\n");

    for (i = 0; i < LAUNCHER_VISIBLE_ROWS; i++) {
        int index = g_scroll + i;
        const char *name;
        if (index >= g_gameCount) {
            iprintf("\n");
            continue;
        }
        name = g_games[index].label[0] ? g_games[index].label : pathBaseName(g_games[index].path);
        snprintf(line, sizeof(line), "%c %d. %s",
                 index == g_selected ? '>' : ' ', index + 1, name);
        line[31] = 0;
        iprintf("%s\n", line);
    }

    iprintf("%d/%d UP/DOWN  A=run\n", g_selected + 1, g_gameCount);
    iprintf("Touch twice to run\n");
}

static void clampSelection(void) {
    if (g_gameCount <= 0) {
        g_selected = 0;
        g_scroll = 0;
        return;
    }
    if (g_selected < 0) g_selected = 0;
    if (g_selected >= g_gameCount) g_selected = g_gameCount - 1;
    if (g_selected < g_scroll) g_scroll = g_selected;
    if (g_selected >= g_scroll + LAUNCHER_VISIBLE_ROWS) {
        g_scroll = g_selected - LAUNCHER_VISIBLE_ROWS + 1;
    }
}

static int launcherChooseGame(void) {
    int launch = 0;
    drawLauncher();
    while (!launch) {
        unsigned int down;
        unsigned int held;
        scanKeys();
        down = keysDown();
        held = keysHeld();
        if (down & KEY_UP) {
            g_selected--;
            clampSelection();
            drawLauncher();
        }
        if (down & KEY_DOWN) {
            g_selected++;
            clampSelection();
            drawLauncher();
        }
        if (down & KEY_LEFT) {
            g_selected -= LAUNCHER_VISIBLE_ROWS;
            clampSelection();
            drawLauncher();
        }
        if (down & KEY_RIGHT) {
            g_selected += LAUNCHER_VISIBLE_ROWS;
            clampSelection();
            drawLauncher();
        }
        if (down & KEY_B) {
            scanGames();
            drawLauncher();
        }
        if (g_gameCount > 0 && (down & (KEY_A | KEY_START))) {
            launch = 1;
        }
        if ((held & KEY_TOUCH) != 0) {
            touchPosition pos;
            touchRead(&pos);
            if ((down & KEY_TOUCH) != 0 && pos.py >= 24 && pos.py < 160) {
                int row = (pos.py - 24) / 8;
                int index;
                if (row < 0) row = 0;
                if (row >= LAUNCHER_VISIBLE_ROWS) row = LAUNCHER_VISIBLE_ROWS - 1;
                index = g_scroll + row;
                if (index >= 0 && index < g_gameCount) {
                    if (g_lastTouchRow == index && g_selected == index) {
                        launch = 1;
                    } else {
                        g_selected = index;
                        g_lastTouchRow = index;
                        clampSelection();
                        drawLauncher();
                    }
                }
            }
        }
        swiWaitForVBlank();
    }
    return g_selected;
}

static int readAt(FILE *fp, long offset, void *buffer, int length) {
    if (fp == NULL || buffer == NULL || length < 0) return 0;
    if (fseek(fp, offset, SEEK_SET) != 0) return 0;
    return fread(buffer, 1, length, fp) == (size_t)length;
}

static int loadJarManifest(const char *path, char **manifestOut, int *lengthOut) {
    FILE *fp = NULL;
    unsigned char *tail = NULL;
    unsigned char *compressed = NULL;
    unsigned char *manifest = NULL;
    long fileSize;
    long tailStart;
    int tailSize;
    int eocd = -1;
    unsigned long centralOffset;
    unsigned int entryCount;
    unsigned int i;
    int ok = 0;

    if (manifestOut == NULL || lengthOut == NULL) return 0;
    *manifestOut = NULL;
    *lengthOut = 0;
    fp = fopen(path, "rb");
    if (fp == NULL) goto done;
    if (fseek(fp, 0, SEEK_END) != 0) goto done;
    fileSize = ftell(fp);
    if (fileSize < 22) goto done;
    tailSize = (int)(fileSize < LAUNCHER_ZIP_TAIL_MAX ? fileSize : LAUNCHER_ZIP_TAIL_MAX);
    tailStart = fileSize - tailSize;
    tail = (unsigned char *)malloc(tailSize);
    if (tail == NULL || !readAt(fp, tailStart, tail, tailSize)) goto done;
    for (i = (unsigned int)(tailSize - 22); ; i--) {
        if (readLe32(tail + i) == 0x06054b50UL) {
            eocd = (int)i;
            break;
        }
        if (i == 0) break;
    }
    if (eocd < 0) goto done;
    entryCount = readLe16(tail + eocd + 10);
    centralOffset = readLe32(tail + eocd + 16);
    if (fseek(fp, (long)centralOffset, SEEK_SET) != 0) goto done;

    for (i = 0; i < entryCount; i++) {
        unsigned char header[46];
        unsigned int method;
        unsigned long compSize;
        unsigned long uncompSize;
        unsigned int nameLen;
        unsigned int extraLen;
        unsigned int commentLen;
        unsigned long localOffset;
        char name[260];
        long nextCentral;
        if (fread(header, 1, sizeof(header), fp) != sizeof(header)) goto done;
        if (readLe32(header) != 0x02014b50UL) goto done;
        method = readLe16(header + 10);
        compSize = readLe32(header + 20);
        uncompSize = readLe32(header + 24);
        nameLen = readLe16(header + 28);
        extraLen = readLe16(header + 30);
        commentLen = readLe16(header + 32);
        localOffset = readLe32(header + 42);
        if (nameLen >= sizeof(name)) goto done;
        if (fread(name, 1, nameLen, fp) != nameLen) goto done;
        name[nameLen] = 0;
        nextCentral = ftell(fp) + extraLen + commentLen;
        if (!nameEqualsIgnoreCase(name, "META-INF/MANIFEST.MF")) {
            if (fseek(fp, nextCentral, SEEK_SET) != 0) goto done;
            continue;
        }
        if (uncompSize == 0 || uncompSize > LAUNCHER_MANIFEST_MAX || compSize > LAUNCHER_MANIFEST_MAX) goto done;
        {
            unsigned char local[30];
            unsigned int localNameLen;
            unsigned int localExtraLen;
            long dataOffset;
            if (!readAt(fp, (long)localOffset, local, sizeof(local))) goto done;
            if (readLe32(local) != 0x04034b50UL) goto done;
            localNameLen = readLe16(local + 26);
            localExtraLen = readLe16(local + 28);
            dataOffset = (long)localOffset + 30 + localNameLen + localExtraLen;
            compressed = (unsigned char *)calloc((size_t)compSize + 8, 1);
            manifest = (unsigned char *)malloc((size_t)uncompSize + 1);
            if (compressed == NULL || manifest == NULL) goto done;
            if (!readAt(fp, dataOffset, compressed, (int)compSize)) goto done;
            if (method == 0) {
                if (compSize != uncompSize) goto done;
                memcpy(manifest, compressed, uncompSize);
            } else if (method == 8) {
                if (!pstrosLauncherInflateRaw(compressed, (int)compSize, manifest, (int)uncompSize)) goto done;
            } else {
                goto done;
            }
            manifest[uncompSize] = 0;
            *manifestOut = (char *)manifest;
            *lengthOut = (int)uncompSize;
            manifest = NULL;
            ok = 1;
            goto done;
        }
    }

done:
    if (fp != NULL) fclose(fp);
    free(tail);
    free(compressed);
    free(manifest);
    return ok;
}


static int launcherEndsWithClass(const char *name) {
    size_t length;
    if (name == NULL) return 0;
    length = strlen(name);
    if (length < 6) return 0;
    return name[length - 6] == '.' &&
           tolower((unsigned char)name[length - 5]) == 'c' &&
           tolower((unsigned char)name[length - 4]) == 'l' &&
           tolower((unsigned char)name[length - 3]) == 'a' &&
           tolower((unsigned char)name[length - 2]) == 's' &&
           tolower((unsigned char)name[length - 1]) == 's';
}

static int launcherBufferContains(const unsigned char *buffer,
                                  unsigned long bufferLength,
                                  const char *needle) {
    unsigned long i;
    size_t needleLength;
    if (buffer == NULL || needle == NULL) return 0;
    needleLength = strlen(needle);
    if (needleLength == 0 || bufferLength < needleLength) return 0;
    for (i = 0; i + needleLength <= bufferLength; i++) {
        if (buffer[i] == (unsigned char)needle[0] &&
            memcmp(buffer + i, needle, needleLength) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Detect old Nokia games that subclass com.nokia.mid.ui.FullCanvas. The
 * launcher cannot rewrite a FAT JAR, so it inspects one class at a time and
 * enables a native property override only for the selected game. */
static int jarUsesNokiaFullCanvas(const char *path) {
    static const char fullCanvasName[] = "com/nokia/mid/ui/FullCanvas";
    FILE *fp = NULL;
    unsigned char *tail = NULL;
    unsigned char *compressed = NULL;
    unsigned char *classData = NULL;
    long fileSize;
    long tailStart;
    int tailSize;
    int eocd = -1;
    unsigned long centralOffset;
    unsigned int entryCount;
    unsigned int i;
    int found = 0;

    if (path == NULL) return 0;
    fp = fopen(path, "rb");
    if (fp == NULL) goto done;
    if (fseek(fp, 0, SEEK_END) != 0) goto done;
    fileSize = ftell(fp);
    if (fileSize < 22) goto done;
    tailSize = (int)(fileSize < LAUNCHER_ZIP_TAIL_MAX ? fileSize : LAUNCHER_ZIP_TAIL_MAX);
    tailStart = fileSize - tailSize;
    tail = (unsigned char *)malloc((size_t)tailSize);
    if (tail == NULL || !readAt(fp, tailStart, tail, tailSize)) goto done;
    for (i = (unsigned int)(tailSize - 22); ; i--) {
        if (readLe32(tail + i) == 0x06054b50UL) {
            eocd = (int)i;
            break;
        }
        if (i == 0) break;
    }
    if (eocd < 0) goto done;
    entryCount = readLe16(tail + eocd + 10);
    centralOffset = readLe32(tail + eocd + 16);
    if (fseek(fp, (long)centralOffset, SEEK_SET) != 0) goto done;

    for (i = 0; i < entryCount; i++) {
        unsigned char header[46];
        unsigned int method;
        unsigned long compSize;
        unsigned long uncompSize;
        unsigned int nameLen;
        unsigned int extraLen;
        unsigned int commentLen;
        unsigned long localOffset;
        char name[260];
        long nextCentral;

        if (fread(header, 1, sizeof(header), fp) != sizeof(header)) goto done;
        if (readLe32(header) != 0x02014b50UL) goto done;
        method = readLe16(header + 10);
        compSize = readLe32(header + 20);
        uncompSize = readLe32(header + 24);
        nameLen = readLe16(header + 28);
        extraLen = readLe16(header + 30);
        commentLen = readLe16(header + 32);
        localOffset = readLe32(header + 42);
        if (nameLen >= sizeof(name)) goto done;
        if (fread(name, 1, nameLen, fp) != nameLen) goto done;
        name[nameLen] = 0;
        nextCentral = ftell(fp) + extraLen + commentLen;

        if (!launcherEndsWithClass(name) || uncompSize == 0 ||
            uncompSize > (512UL * 1024UL) || compSize > (512UL * 1024UL) ||
            (method != 0 && method != 8)) {
            if (fseek(fp, nextCentral, SEEK_SET) != 0) goto done;
            continue;
        }

        {
            unsigned char local[30];
            unsigned int localNameLen;
            unsigned int localExtraLen;
            long dataOffset;
            int decoded = 0;

            if (!readAt(fp, (long)localOffset, local, sizeof(local))) goto done;
            if (readLe32(local) != 0x04034b50UL) goto done;
            localNameLen = readLe16(local + 26);
            localExtraLen = readLe16(local + 28);
            dataOffset = (long)localOffset + 30 + localNameLen + localExtraLen;

            compressed = (unsigned char *)calloc((size_t)compSize + 8, 1);
            classData = (unsigned char *)malloc((size_t)uncompSize + 1);
            if (compressed == NULL || classData == NULL) goto done;
            if (!readAt(fp, dataOffset, compressed, (int)compSize)) goto done;

            if (method == 0) {
                if (compSize == uncompSize) {
                    memcpy(classData, compressed, (size_t)uncompSize);
                    decoded = 1;
                }
            } else {
                decoded = pstrosLauncherInflateRaw(compressed, (int)compSize,
                                                    classData, (int)uncompSize);
            }

            if (decoded && launcherBufferContains(classData, uncompSize, fullCanvasName)) {
                found = 1;
                goto done;
            }

            free(compressed);
            compressed = NULL;
            free(classData);
            classData = NULL;
            if (fseek(fp, nextCentral, SEEK_SET) != 0) goto done;
        }
    }

done:
    if (fp != NULL) fclose(fp);
    free(tail);
    free(compressed);
    free(classData);
    return found;
}

static char *unfoldManifest(const char *input, int inputLength) {
    char *out;
    int inPos = 0;
    int outPos = 0;
    int firstLine = 1;
    if (input == NULL || inputLength < 0) return NULL;
    out = (char *)malloc((size_t)inputLength + 3);
    if (out == NULL) return NULL;
    while (inPos < inputLength) {
        int lineStart = inPos;
        int lineEnd;
        int continuation;
        while (inPos < inputLength && input[inPos] != '\n' && input[inPos] != '\r') inPos++;
        lineEnd = inPos;
        while (inPos < inputLength && (input[inPos] == '\n' || input[inPos] == '\r')) inPos++;
        continuation = lineEnd > lineStart && input[lineStart] == ' ';
        if (continuation && !firstLine) {
            if (outPos > 0 && out[outPos - 1] == '\n') outPos--;
            lineStart++;
        }
        while (lineStart < lineEnd) out[outPos++] = input[lineStart++];
        out[outPos++] = '\n';
        firstLine = 0;
    }
    out[outPos] = 0;
    return out;
}

static int getManifestProperty(const char *manifest, const char *key, char *out, int outSize) {
    int keyLen;
    const char *p;
    if (manifest == NULL || key == NULL || out == NULL || outSize <= 0) return 0;
    keyLen = (int)strlen(key);
    p = manifest;
    while (*p) {
        const char *line = p;
        const char *end = strchr(line, '\n');
        const char *value;
        int len;
        if (end == NULL) end = line + strlen(line);
        if ((end - line) > keyLen && strncmp(line, key, keyLen) == 0 && line[keyLen] == ':') {
            value = line + keyLen + 1;
            while (value < end && (*value == ' ' || *value == '\t')) value++;
            len = (int)(end - value);
            while (len > 0 && (value[len - 1] == ' ' || value[len - 1] == '\t')) len--;
            if (len >= outSize) len = outSize - 1;
            memcpy(out, value, len);
            out[len] = 0;
            return 1;
        }
        p = *end ? end + 1 : end;
    }
    return 0;
}

static void trimText(char *text) {
    char *start;
    char *end;
    if (text == NULL) return;
    start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != text) memmove(text, start, strlen(start) + 1);
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) end--;
    *end = 0;
}

static int parseMidletInfo(const char *manifest,
                           char *appName, int appNameSize,
                           char *mainClass, int mainClassSize) {
    char midlet1[512];
    char fallbackName[128];
    char *firstComma;
    char *lastComma;
    appName[0] = 0;
    mainClass[0] = 0;
    fallbackName[0] = 0;
    getManifestProperty(manifest, "MIDlet-Name", fallbackName, sizeof(fallbackName));
    if (getManifestProperty(manifest, "MIDlet-1", midlet1, sizeof(midlet1))) {
        firstComma = strchr(midlet1, ',');
        lastComma = strrchr(midlet1, ',');
        if (firstComma != NULL) {
            *firstComma = 0;
            trimText(midlet1);
            snprintf(appName, appNameSize, "%s", midlet1);
        }
        if (lastComma != NULL) {
            trimText(lastComma + 1);
            snprintf(mainClass, mainClassSize, "%s", lastComma + 1);
        }
    }
    if (appName[0] == 0 && fallbackName[0] != 0) snprintf(appName, appNameSize, "%s", fallbackName);
    if (mainClass[0] == 0) getManifestProperty(manifest, "MIDlet-Class", mainClass, mainClassSize);
    trimText(appName);
    trimText(mainClass);
    return appName[0] != 0 && mainClass[0] != 0;
}

static void waitForever(void) {
    while (1) swiWaitForVBlank();
}

int main(int argc, char **argv) {
    int result = 0;

    g_scanCwd[0] = 0;
    g_launcherDir[0] = 0;
    if (argc > 0 && argv != NULL && argv[0] != NULL) {
        dirnameFromPath(argv[0], g_launcherDir, sizeof(g_launcherDir));
    }

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleDemoInit();
    pstrosSetVmConsoleEnabled(0);

    if (!fatInitDefault()) {
        iprintf("Pstro Laucher\n\nFAT init failed.\n\nCheck DLDI / SD card.");
        waitForever();
    }
    if (getcwd(g_scanCwd, sizeof(g_scanCwd)) == NULL) g_scanCwd[0] = 0;

    irqSet(IRQ_VBLANK, kvm_vblank_handler);
    lcdSetVBlankIrq(true);
    irqEnable(IRQ_VBLANK);

    for (;;) {
        int selected;
        char *rawManifest = NULL;
        char *manifest = NULL;
        int rawManifestLength = 0;
        char appName[128];
        char mainClass[192];
        char gameId[80];
        char midletArg[224];
        char appArg[160];
        char *kvmArgv[5];
        int kvmArgc = 0;

        scanGames();
        selected = launcherChooseGame();
        if (selected < 0 || selected >= g_gameCount) waitForever();

        iprintf("\x1b[2J\x1b[0;0H");
        setConsoleRow(0, "Loading J2ME game...");
        setConsoleRow(2, g_games[selected].label);

        if (!loadJarManifest(g_games[selected].path, &rawManifest, &rawManifestLength)) {
            setConsoleRow(4, "Manifest read failed");
            setConsoleRow(6, "Unsupported/corrupt JAR");
            waitForever();
        }
        manifest = unfoldManifest(rawManifest, rawManifestLength);
        free(rawManifest);
        rawManifest = NULL;
        if (manifest == NULL || !parseMidletInfo(manifest,
                                                 appName, sizeof(appName),
                                                 mainClass, sizeof(mainClass))) {
            setConsoleRow(4, "MIDlet-1 not found");
            free(manifest);
            waitForever();
        }

        makeGameId(g_games[selected].path, gameId, sizeof(gameId));
        pstrosUiSetGameId(gameId, appName);
        pstrosUiSetGamePath(g_games[selected].path);
        if (jarUsesNokiaFullCanvas(g_games[selected].path)) {
            pstrosSetNokiaFullCanvasMode(1);
            setConsoleRow(7, "Key5: Nokia FIRE");
        } else {
            pstrosSetNokiaFullCanvasMode(0);
        }
        if (!pstrosConfigureSaveStorageForGame(gameId)) {
            setConsoleRow(5, "Save: read-only");
        } else {
            setConsoleRow(5, "Save: enabled");
        }

        initJadBuffer();
        if (!setJadBufferText(manifest)) {
            free(manifest);
            setConsoleRow(7, "Manifest memory failed");
            waitForever();
        }
        free(manifest);

        RequestedHeapSize = DEFAULTHEAPSIZE;
        UserClassPath = g_games[selected].path;

        kvmArgv[kvmArgc++] = "nds.pstros.MainApp";
        snprintf(midletArg, sizeof(midletArg), "-C%s", mainClass);
        snprintf(appArg, sizeof(appArg), "-A%s", appName);
        kvmArgv[kvmArgc++] = midletArg;
        kvmArgv[kvmArgc++] = appArg;
        kvmArgv[kvmArgc++] = "-mute";
        if (pstrosGetSavePath() == NULL || pstrosGetSavePath()[0] == 0) {
            kvmArgv[kvmArgc++] = "-ro";
        }

        pstrosUiActivate();
        result = StartJVM(kvmArgc, kvmArgv);
        pstrosAudioDiagKvmExit(result);
        freeJadBuffer();

        if (result == 0) {
            /* Normal MIDlet exit: go back to the launcher list. */
            pstrosSetVmConsoleEnabled(0);
            iprintf("\x1b[2J\x1b[0;0H");
            continue;
        }

        /* Abnormal exit: keep diagnostics visible. */
        waitForever();
    }

    return result;
}
