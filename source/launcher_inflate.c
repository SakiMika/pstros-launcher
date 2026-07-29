/* Small pre-JVM raw DEFLATE wrapper used only to read JAR manifests on FAT.
 * It compiles a second copy of the KVM inflater against normal malloc/free,
 * so it is safe before StartJVM creates the Java heap. */
#define COMPILING_FOR_KVM 0
#define inflateData launcherInflateDataKvm
#define inflateBuffer launcherInflateBuffer
#define inflateBufferIndex launcherInflateBufferIndex
#define inflateBufferCount launcherInflateBufferCount
#include "../kvm/VmExtra/src/inflate.c"

typedef struct {
    const unsigned char *cursor;
    int remaining;
} LauncherInflateInput;

static int launcherInflateRead(unsigned char *out, int length, void *opaque) {
    LauncherInflateInput *input = (LauncherInflateInput *)opaque;
    int count;
    if (out == NULL || input == NULL || length <= 0 || input->remaining <= 0) return 0;
    count = length;
    if (count > input->remaining) count = input->remaining;
    memcpy(out, input->cursor, count);
    input->cursor += count;
    input->remaining -= count;
    return count;
}

int pstrosLauncherInflateRaw(const unsigned char *compressed,
                             int compressedLength,
                             unsigned char *output,
                             int outputLength) {
    LauncherInflateInput input;
    unsigned char *outputPtr = output;
    if (compressed == NULL || output == NULL || compressedLength < 0 || outputLength < 0) return 0;
    input.cursor = compressed;
    input.remaining = compressedLength;
    return launcherInflateDataKvm(&input,
                                  (JarGetByteFunctionType)launcherInflateRead,
                                  compressedLength,
                                  &outputPtr,
                                  outputLength) ? 1 : 0;
}
