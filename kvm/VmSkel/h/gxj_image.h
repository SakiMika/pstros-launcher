#ifndef GXJ_IMAGE_H
#define GXJ_IMAGE_H

/*
 * Minimal PNG decoder used by Pstros NDS Image.createImage().
 *
 * The previous merge had only a stub decode_png_image(), so every PNG decode
 * returned 0 and Java threw: RuntimeException: unknown image format.
 *
 * This decoder is intentionally small for NDS/KVM:
 * - PNG signature / IHDR / PLTE / tRNS / IDAT / IEND parsing
 * - zlib stream carried by IDAT is inflated with KVM's existing inflater
 * - non-interlaced PNG only
 * - color types 0, 2, 3, 4, 6
 * - bit depths 1/2/4/8 for grayscale and palette, 8/16 for truecolor types
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inflate.h>

typedef struct memoryFileStruct {
    char *data;
    int pos;
    int size;
} memoryFile;

typedef struct imageSrcDataStruct {
    memoryFile *file;
} imageSrcData;

typedef struct imageDataStruct {
    unsigned short *palette;
    unsigned short *pixelBuf;
    char *alphaBuf;
    int imgType;
} imageData;

typedef struct imageDstDataStruct {
    imageData *image;
} imageDstData;

static inline void initImageSrcData(imageSrcData *dst, memoryFile *src) {
    dst->file = src;
}

static inline void initImageDstData(imageDstData *dst, imageData *src) {
    dst->image = src;
}

static unsigned int gxj_png_read32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

static unsigned short gxj_rgb15(int r, int g, int b) {
    return (unsigned short)(0x8000 |
        ((r >> 3) & 0x1F) |
        (((g >> 3) & 0x1F) << 5) |
        (((b >> 3) & 0x1F) << 10));
}

static int gxj_png_channels(int colorType) {
    switch (colorType) {
        case 0: return 1; /* grayscale */
        case 2: return 3; /* RGB */
        case 3: return 1; /* indexed */
        case 4: return 2; /* gray + alpha */
        case 6: return 4; /* RGBA */
    }
    return 0;
}

static int gxj_png_valid_depth(int colorType, int bitDepth) {
    switch (colorType) {
        case 0: return bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8 || bitDepth == 16;
        case 2: return bitDepth == 8 || bitDepth == 16;
        case 3: return bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8;
        case 4: return bitDepth == 8 || bitDepth == 16;
        case 6: return bitDepth == 8 || bitDepth == 16;
    }
    return 0;
}

typedef struct gxj_png_byte_sourceStruct {
    unsigned char *data;
    int pos;
    int len;
} gxj_png_byte_source;

static int gxj_png_get_byte(unsigned char *buff, int length, void *p) {
    gxj_png_byte_source *src = (gxj_png_byte_source*)p;
    int remaining;
    int count;

    if (src == NULL || buff == NULL || length <= 0 || src->pos >= src->len) {
        return 0;
    }

    remaining = src->len - src->pos;
    count = (remaining < length) ? remaining : length;
    memcpy(buff, src->data + src->pos, count);
    src->pos += count;
    return count;
}

static int gxj_png_paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

static int gxj_png_scale_sample(int v, int bitDepth) {
    switch (bitDepth) {
        case 1: return v ? 255 : 0;
        case 2: return (v * 255) / 3;
        case 4: return (v * 255) / 15;
        case 8: return v;
        case 16: return v >> 8;
    }
    return v;
}

static int gxj_png_get_packed_sample(const unsigned char *row, int x, int bitDepth) {
    int bit = x * bitDepth;
    int byteIndex = bit >> 3;
    int shift = 8 - bitDepth - (bit & 7);
    int mask = (1 << bitDepth) - 1;
    return (row[byteIndex] >> shift) & mask;
}

static int gxj_png_inflate_idat(unsigned char *idat, int idatLen, unsigned char *out, int outLen) {
    gxj_png_byte_source byteSrc;
    unsigned char *outPtr;

    /* PNG stores a zlib stream: 2-byte zlib header + raw deflate + 4-byte Adler32. */
    if (idatLen < 6) {
        return 0;
    }

    byteSrc.data = idat + 2;
    byteSrc.pos = 0;
    byteSrc.len = idatLen - 2; /* includes Adler32 as safe extra bytes for KVM inflater */
    outPtr = out;

    return inflateData(&byteSrc, (JarGetByteFunctionType)gxj_png_get_byte,
                       idatLen - 6, &outPtr, outLen) ? 1 : 0;
}

static int gxj_png_decode_pixels(
        unsigned char *raw, int width, int height, int rowBytes,
        int bitDepth, int colorType,
        unsigned char *plte, int plteEntries,
        unsigned char *trns, int trnsLen,
        unsigned short *pixelBuf, char *alphaBuf) {
    int channels = gxj_png_channels(colorType);
    int bytesPerPixel = (channels * bitDepth + 7) >> 3;
    int y, x, i;
    int pos = 0;
    int outIndex = 0;
    unsigned char *prev = NULL;
    unsigned char *row;

    if (bytesPerPixel < 1) bytesPerPixel = 1;

    for (y = 0; y < height; y++) {
        int filter = raw[pos++];
        row = raw + pos;

        for (i = 0; i < rowBytes; i++) {
            int left = (i >= bytesPerPixel) ? row[i - bytesPerPixel] : 0;
            int up = prev ? prev[i] : 0;
            int upLeft = (prev && i >= bytesPerPixel) ? prev[i - bytesPerPixel] : 0;
            int val = row[i];

            switch (filter) {
                case 0: break;
                case 1: val += left; break;
                case 2: val += up; break;
                case 3: val += ((left + up) >> 1); break;
                case 4: val += gxj_png_paeth(left, up, upLeft); break;
                default: return 0;
            }
            row[i] = (unsigned char)(val & 0xFF);
        }

        if (colorType == 0) {
            int transparentGray = -1;
            if (trnsLen >= 2) {
                transparentGray = ((int)trns[0] << 8) | trns[1];
                if (bitDepth < 16) transparentGray &= ((1 << bitDepth) - 1);
            }
            if (bitDepth < 8) {
                for (x = 0; x < width; x++) {
                    int s = gxj_png_get_packed_sample(row, x, bitDepth);
                    int g = gxj_png_scale_sample(s, bitDepth);
                    pixelBuf[outIndex] = gxj_rgb15(g, g, g);
                    alphaBuf[outIndex++] = (char)g;
                }
            } else if (bitDepth == 8) {
                for (x = 0; x < width; x++) {
                    int g = row[x];
                    pixelBuf[outIndex] = gxj_rgb15(g, g, g);
                    alphaBuf[outIndex++] = (char)g;
                }
            } else {
                for (x = 0; x < width; x++) {
                    int s16 = ((int)row[x * 2] << 8) | row[x * 2 + 1];
                    int g = s16 >> 8;
                    pixelBuf[outIndex] = gxj_rgb15(g, g, g);
                    alphaBuf[outIndex++] = (char)g;
                }
            }
        } else if (colorType == 2) {
            int tr = -1, tg = -1, tb = -1;
            if (trnsLen >= 6) {
                tr = ((int)trns[0] << 8) | trns[1];
                tg = ((int)trns[2] << 8) | trns[3];
                tb = ((int)trns[4] << 8) | trns[5];
            }
            if (bitDepth == 8) {
                for (x = 0; x < width; x++) {
                    int p = x * 3;
                    int r = row[p], g = row[p + 1], b = row[p + 2];
                    int a = ((r == tr) && (g == tg) && (b == tb)) ? 0 : 255;
                    pixelBuf[outIndex] = gxj_rgb15(r, g, b);
                    alphaBuf[outIndex++] = (char)a;
                }
            } else {
                for (x = 0; x < width; x++) {
                    int p = x * 6;
                    int r16 = ((int)row[p] << 8) | row[p + 1];
                    int g16 = ((int)row[p + 2] << 8) | row[p + 3];
                    int b16 = ((int)row[p + 4] << 8) | row[p + 5];
                    int a = ((r16 == tr) && (g16 == tg) && (b16 == tb)) ? 0 : 255;
                    pixelBuf[outIndex] = gxj_rgb15(r16 >> 8, g16 >> 8, b16 >> 8);
                    alphaBuf[outIndex++] = (char)a;
                }
            }
        } else if (colorType == 3) {
            for (x = 0; x < width; x++) {
                int idx = (bitDepth == 8) ? row[x] : gxj_png_get_packed_sample(row, x, bitDepth);
                int r = 0, g = 0, b = 0, a = 255;
                if (idx < plteEntries) {
                    r = plte[idx * 3];
                    g = plte[idx * 3 + 1];
                    b = plte[idx * 3 + 2];
                }
                if (idx < trnsLen) a = trns[idx];
                pixelBuf[outIndex] = gxj_rgb15(r, g, b);
                alphaBuf[outIndex++] = (char)a;
            }
        } else if (colorType == 4) {
            if (bitDepth == 8) {
                for (x = 0; x < width; x++) {
                    int p = x * 2;
                    int g = row[p];
                    int a = row[p + 1];
                    pixelBuf[outIndex] = gxj_rgb15(g, g, g);
                    alphaBuf[outIndex++] = (char)a;
                }
            } else {
                for (x = 0; x < width; x++) {
                    int p = x * 4;
                    int g = row[p];
                    int a = row[p + 2];
                    pixelBuf[outIndex] = gxj_rgb15(g, g, g);
                    alphaBuf[outIndex++] = (char)a;
                }
            }
        } else if (colorType == 6) {
            if (bitDepth == 8) {
                for (x = 0; x < width; x++) {
                    int p = x * 4;
                    int r = row[p], g = row[p + 1], b = row[p + 2], a = row[p + 3];
                    pixelBuf[outIndex] = gxj_rgb15(r, g, b);
                    alphaBuf[outIndex++] = (char)a;
                }
            } else {
                for (x = 0; x < width; x++) {
                    int p = x * 8;
                    int r = row[p], g = row[p + 2], b = row[p + 4], a = row[p + 6];
                    pixelBuf[outIndex] = gxj_rgb15(r, g, b);
                    alphaBuf[outIndex++] = (char)a;
                }
            }
        } else {
            return 0;
        }

        prev = row;
        pos += rowBytes;
    }

    return 1;
}

static inline int decode_png_image(imageSrcData *src, imageDstData *dst) {
    unsigned char *png;
    int pngSize;
    int pos;
    int width = 0, height = 0, bitDepth = 0, colorType = 0, interlace = 0;
    int compression = 0, filterMethod = 0;
    int channels, bitsPerPixel, rowBytes, rawSize;
    unsigned char *idat = NULL;
    int idatLen = 0;
    unsigned char *plte = NULL;
    int plteLen = 0;
    unsigned char *trns = NULL;
    int trnsLen = 0;
    unsigned char *raw = NULL;
    int ok = 0;

    if (src == NULL || src->file == NULL || dst == NULL || dst->image == NULL ||
        dst->image->pixelBuf == NULL || dst->image->alphaBuf == NULL) {
        return 0;
    }

    png = (unsigned char*)src->file->data;
    pngSize = src->file->size;
    if (pngSize < 33 || png[0] != 0x89 || png[1] != 'P' || png[2] != 'N' || png[3] != 'G') {
        return 0;
    }

    pos = 8;
    while (pos + 8 <= pngSize) {
        unsigned int len = gxj_png_read32(png + pos);
        unsigned int type = gxj_png_read32(png + pos + 4);
        unsigned char *chunk = png + pos + 8;

        if (len > (unsigned int)(pngSize - pos - 12)) {
            goto done;
        }

        if (type == 0x49484452) { /* IHDR */
            if (len < 13) goto done;
            width = (int)gxj_png_read32(chunk);
            height = (int)gxj_png_read32(chunk + 4);
            bitDepth = chunk[8];
            colorType = chunk[9];
            compression = chunk[10];
            filterMethod = chunk[11];
            interlace = chunk[12];
        } else if (type == 0x504C5445) { /* PLTE */
            if (plte) free(plte);
            plteLen = (int)len;
            plte = (unsigned char*)malloc(plteLen);
            if (!plte) goto done;
            memcpy(plte, chunk, plteLen);
        } else if (type == 0x74524E53) { /* tRNS */
            if (trns) free(trns);
            trnsLen = (int)len;
            trns = (unsigned char*)malloc(trnsLen);
            if (!trns) goto done;
            memcpy(trns, chunk, trnsLen);
        } else if (type == 0x49444154) { /* IDAT */
            unsigned char *newIdat = (unsigned char*)malloc(idatLen + (int)len);
            if (!newIdat) goto done;
            if (idatLen > 0 && idat) memcpy(newIdat, idat, idatLen);
            memcpy(newIdat + idatLen, chunk, (int)len);
            if (idat) free(idat);
            idat = newIdat;
            idatLen += (int)len;
        } else if (type == 0x49454E44) { /* IEND */
            break;
        }

        pos += 12 + (int)len;
    }

    if (width <= 0 || height <= 0 || idatLen <= 0) goto done;
    printf("png decode: %dx%d ct=%d bd=%d idat=%d\n", width, height, colorType, bitDepth, idatLen);
    if (compression != 0 || filterMethod != 0 || interlace != 0) goto done;
    if (!gxj_png_valid_depth(colorType, bitDepth)) goto done;
    if (colorType == 3 && (!plte || plteLen < 3)) goto done;

    channels = gxj_png_channels(colorType);
    if (channels <= 0) goto done;
    bitsPerPixel = channels * bitDepth;
    rowBytes = (width * bitsPerPixel + 7) >> 3;
    rawSize = (rowBytes + 1) * height;
    if (rowBytes <= 0 || rawSize <= 0) goto done;

    raw = (unsigned char*)malloc(rawSize);
    if (!raw) goto done;
    if (!gxj_png_inflate_idat(idat, idatLen, raw, rawSize)) goto done;
    printf("png inflate ok: raw=%d\n", rawSize);

    if (!gxj_png_decode_pixels(raw, width, height, rowBytes, bitDepth, colorType,
                               plte, plteLen / 3, trns, trnsLen,
                               dst->image->pixelBuf, dst->image->alphaBuf)) {
        goto done;
    }

    /* Java adds 1 to imgType.
     * imgType 0 -> Java result 1: grayscale images keep pixelDataByte only.
     * imgType 3 -> Java result 4: indexed/palette transparency path.
     * Other truecolor/alpha images keep both pixel and alpha buffers.
     */
    if (colorType == 0) {
        dst->image->imgType = 0;
    } else if (colorType == 3) {
        dst->image->imgType = 3;
    } else {
        dst->image->imgType = 2;
    }
    ok = 1;
    printf("png decode ok: type=%d\n", dst->image->imgType);

done:
    /*
     * NDS/devkitARM diagnostic fix:
     * Earlier builds reached "png decode ok" and then froze before Java
     * returned from Video.decodePngImage().  The only code after that log was
     * freeing PNG temporary buffers.  On this old KVM/libnds heap, free() can
     * hang when the Java heap/native heap are under pressure, so keep the
     * small decode buffers alive instead of freeing them here.
     *
     * This intentionally leaks only the temporary PNG decode buffers.  It is a
     * boot-stability workaround for Pstros NDS; Diamond Rush mainly needs the
     * embedded system.dsf font and a very small number of PNGs.
     */
    printf("png decode done: ok=%d nofree\\n", ok);
    idat = NULL;
    plte = NULL;
    trns = NULL;
    raw = NULL;
    if (!ok && dst != NULL && dst->image != NULL) {
        dst->image->imgType = 0;
    }
    return ok;
}

#endif
