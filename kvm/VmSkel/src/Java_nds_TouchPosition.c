#include <nds.h>
#include <nds/arm9/input.h>
#include <kni.h>

static touchPosition pos;

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_TouchPosition_update(void)
{
    /* libnds 2.x/Calico API: touchRead fills TouchData and returns whether
       the touchscreen is currently pressed. */
    const bool touched = touchRead(&pos);
    const jint rawX = touched ? (jint)pos.rawx : 0;
    const jint rawY = touched ? (jint)pos.rawy : 0;
    const jint pixelX = touched ? (jint)pos.px : 0;
    const jint pixelY = touched ? (jint)pos.py : 0;

    /* Calico no longer exposes the old Z1/Z2 ADC pressure samples.
       Preserve the Java ABI with a simple non-zero touch indicator. */
    const jint touchState = touched ? 1 : 0;

    KNI_StartHandles(2);
    KNI_DeclareHandle(objectHandle);
    KNI_DeclareHandle(classHandle);

    KNI_GetThisPointer(objectHandle);
    KNI_GetObjectClass(objectHandle, classHandle);

    jfieldID fid;

    fid = KNI_GetFieldID(classHandle, "x", "I");
    KNI_SetIntField(objectHandle, fid, rawX);

    fid = KNI_GetFieldID(classHandle, "y", "I");
    KNI_SetIntField(objectHandle, fid, rawY);

    fid = KNI_GetFieldID(classHandle, "px", "I");
    KNI_SetIntField(objectHandle, fid, pixelX);

    fid = KNI_GetFieldID(classHandle, "py", "I");
    KNI_SetIntField(objectHandle, fid, pixelY);

    fid = KNI_GetFieldID(classHandle, "z1", "I");
    KNI_SetIntField(objectHandle, fid, touchState);

    fid = KNI_GetFieldID(classHandle, "z2", "I");
    KNI_SetIntField(objectHandle, fid, touchState);

    KNI_EndHandles();
    KNI_ReturnVoid();
}
