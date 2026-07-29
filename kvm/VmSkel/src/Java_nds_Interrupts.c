#include <nds.h>
#include <nds/arm9/video.h>
#include <kni.h> 
#include <stdio.h> 
KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Interrupts_irqInit() {
    /* Compatibility entry point for the old Java API.
     * Calico has already initialized IRQ handling before main(). */
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Interrupts_irqSet() { 
    irqSet(IRQ_VBLANK, 0);
    KNI_ReturnVoid(); 
} 

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Interrupts_irqClear() { 
    irqClear(IRQ_VBLANK);
    KNI_ReturnVoid(); 
} 

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Interrupts_irqEnable() {
    lcdSetVBlankIrq(true);
    irqEnable(IRQ_VBLANK);
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_nds_Interrupts_irqDisable() {
    irqDisable(IRQ_VBLANK);
    lcdSetVBlankIrq(false);
    KNI_ReturnVoid();
}

 
