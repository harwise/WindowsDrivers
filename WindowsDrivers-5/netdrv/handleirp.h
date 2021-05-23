/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Handling IRP Library
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/

#ifndef __HANDLEIRP_H__
#define __HANDLEIRP_H__

typedef void (*PFNCLEANUPIRP)(PIRP pIrp, PVOID pContext);


typedef struct _IRPLISTHEAD IRPLISTHEAD;
typedef struct _IRPLISTHEAD *PIRPLISTHEAD;

PIRPLISTHEAD HandleIrp_CreateIrpList(ULONG ulPoolTag);
NTSTATUS HandleIrp_FreeIrpList(PIRPLISTHEAD pIrpListHead);

NTSTATUS HandleIrp_AddIrp(PIRPLISTHEAD pIrpListHead, PIRP pIrp, PDRIVER_CANCEL pDriverCancelRoutine, PFNCLEANUPIRP pfnCleanUpIrp, PVOID pContext);
PIRP HandleIrp_RemoveNextIrp(PIRPLISTHEAD pIrpListHead);
NTSTATUS HandleIrp_PerformCancel(PIRPLISTHEAD pIrpListHead, PIRP pIrp);
void HandleIrp_FreeIrpListWithCleanUp(PIRPLISTHEAD pIrpListHead);


#endif



