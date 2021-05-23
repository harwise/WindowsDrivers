/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Kernel Memory Thunk Library
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/


// #define _X86_ 

#include <wdm.h>
#include "kmem.h"


#define KMEM_ADJUST_POINTER(p, s) (((UCHAR *)p) + s)
#define KMEM_ADJUST_POINTER_BACK(p, s) (((UCHAR *)p) - s)

/*
 * This is used for debugging purposes.  Insert any data which may
 * help track down memory issues.
 */
typedef struct _internal_memory_header
{
  ULONG uiDataSize;

} INTERNAL_MEMORY_HEADER, *PINTERNAL_MEMORY_HEADER;


/**********************************************************************
 * 
 *  KMem_AllocateNonPagedMemory
 *
 *    This function allocates non-paged memory.
 *
 **********************************************************************/
PVOID KMem_AllocateNonPagedMemory(ULONG uiSize, ULONG ulPoolTag)
{
    PINTERNAL_MEMORY_HEADER pDataBlob = NULL;
    PVOID pRetData = NULL;
    ULONG uiAllocatedSize;

    uiAllocatedSize = uiSize + sizeof(INTERNAL_MEMORY_HEADER);

    pDataBlob = (PINTERNAL_MEMORY_HEADER)ExAllocatePoolWithTag(NonPagedPool, uiAllocatedSize, ulPoolTag);

    if(pDataBlob)
    {
       DbgPrint("KMem_AllocateNonPagedMemory = 0x%0x\n", pDataBlob); 
       pDataBlob->uiDataSize = uiSize;
       pRetData = (PVOID)KMEM_ADJUST_POINTER(pDataBlob, sizeof(INTERNAL_MEMORY_HEADER));
    }  

    return pRetData;
}


/**********************************************************************
 * 
 *  KMem_FreeNonPagedMemory
 *
 *    This function Frees non-paged memory.
 *
 **********************************************************************/
void KMem_FreeNonPagedMemory(PVOID pAllocatedMemory)
{
    PINTERNAL_MEMORY_HEADER pDataBlob = (PINTERNAL_MEMORY_HEADER)KMEM_ADJUST_POINTER_BACK(pAllocatedMemory, sizeof(INTERNAL_MEMORY_HEADER));

    /*
     * If we have over written any memory areas, we have filled this area with a special "DA" value so
     * later we can back track and look if there are any series of "DA" around a corrupted memory region.
     */
    RtlFillMemory(pDataBlob, pDataBlob->uiDataSize + sizeof(INTERNAL_MEMORY_HEADER), 0xDA); 

    DbgPrint("KMem_FreeNonPagedMemory = 0x%0x\n", pDataBlob); 
    ExFreePool(pDataBlob);
}


