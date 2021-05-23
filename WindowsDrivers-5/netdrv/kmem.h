/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Allocate Memory Thunk Library
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/

#ifndef __KMEM_H__
#define __KMEM_H__

PVOID KMem_AllocateNonPagedMemory(ULONG uiSize, ULONG ulPoolTag);
void KMem_FreeNonPagedMemory(PVOID pAllocatedMemory);


#endif



