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


// #define _X86_ 

#include <wdm.h>
#include "kmem.h"
#include "handleirp.h"


typedef struct _IRPLIST *PIRPLIST;
 
typedef struct _IRPLIST
{
    PIRP pIrp;
    PFNCLEANUPIRP pfnCleanUpIrp;
    PDRIVER_CANCEL pfnCancelRoutine;
    PVOID pContext;
    PIRPLIST pNextIrp;

} IRPLIST, *PIRPLIST;

typedef struct _IRPLISTHEAD
{
    KSPIN_LOCK kspIrpListLock;
    ULONG ulPoolTag;
    PIRPLIST pListFront;
    PIRPLIST pListBack;
    
} IRPLISTHEAD, *PIRPLISTHEAD;



/* #pragma alloc_text(PAGE, HandleIrp_FreeIrpListWithCleanUp) */
/* #pragma alloc_text(PAGE, HandleIrp_AddIrp)  */
/* #pragma alloc_text(PAGE, HandleIrp_RemoveNextIrp) */
#pragma alloc_text(PAGE, HandleIrp_CreateIrpList)
#pragma alloc_text(PAGE, HandleIrp_FreeIrpList) 
/* #pragma alloc_text(PAGE, HandleIrp_PerformCancel) */



/**********************************************************************
 * 
 *  HandleIrp_CreateIrpList
 *
 *    This function creates an IRP List Head context.
 *
 **********************************************************************/
PIRPLISTHEAD HandleIrp_CreateIrpList(ULONG ulPoolTag)
{
   PIRPLISTHEAD pIrpListHead = NULL;

   pIrpListHead = (PIRPLISTHEAD)KMem_AllocateNonPagedMemory(sizeof(IRPLISTHEAD), ulPoolTag);

   if(pIrpListHead)
   {
       DbgPrint("HandleIrp_CreateIrpList Allocate = 0x%0x \r\n", pIrpListHead);

       pIrpListHead->ulPoolTag = ulPoolTag;

       KeInitializeSpinLock(&pIrpListHead->kspIrpListLock);
       pIrpListHead->pListBack = pIrpListHead->pListFront = NULL;
   }

   return pIrpListHead;
}

/**********************************************************************
 * 
 *  HandleIrp_FreeIrpList
 *
 *    This function frees an IRP List Head.
 *
 *    This function will fail if there are currently IRPs in the list.
 *
 **********************************************************************/
NTSTATUS HandleIrp_FreeIrpList(PIRPLISTHEAD pIrpListHead)
{
    NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;
    
    if(pIrpListHead->pListBack == NULL && pIrpListHead->pListFront == NULL)
    {
        NtStatus = STATUS_SUCCESS;
        DbgPrint("HandleIrp_FreeIrpList Complete Free Memory = 0x%0x \r\n", pIrpListHead);

        KMem_FreeNonPagedMemory(pIrpListHead);
    }

    return NtStatus;
}

/**********************************************************************
 * 
 *  HandleIrp_FreeIrpListWithCleanUp
 *
 *    This function cancels the IRP List Head.
 *
 *    This function will fail if there are currently IRPs in the list.
 *
 **********************************************************************/
void HandleIrp_FreeIrpListWithCleanUp(PIRPLISTHEAD pIrpListHead)
{
    PIRPLIST pIrpListCurrent;
    KIRQL kOldIrql;
    PDRIVER_CANCEL pCancelRoutine;

    KeAcquireSpinLock(&pIrpListHead->kspIrpListLock, &kOldIrql);
    
    pIrpListCurrent  = pIrpListHead->pListFront;

    while(pIrpListCurrent)
    {
        /*
         * To remove an IRP from the Queue we first want to
         * reset the cancel routine.
         */
        pCancelRoutine = IoSetCancelRoutine(pIrpListCurrent->pIrp, NULL);

        pIrpListHead->pListFront = pIrpListCurrent->pNextIrp;

        if(pIrpListHead->pListFront == NULL)
        {
            pIrpListHead->pListBack = NULL;
        }

        KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);
            
        pIrpListCurrent->pfnCleanUpIrp(pIrpListCurrent->pIrp, pIrpListCurrent->pContext);   

        DbgPrint("HandleIrp_FreeIrpListWithCleanUp Complete Free Memory = 0x%0x \r\n", pIrpListCurrent);

        KMem_FreeNonPagedMemory(pIrpListCurrent);
        pIrpListCurrent = NULL;

        KeAcquireSpinLock(&pIrpListHead->kspIrpListLock, &kOldIrql);
        
        pIrpListCurrent  = pIrpListHead->pListFront;
    }


    KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);

    HandleIrp_FreeIrpList(pIrpListHead);
}

/**********************************************************************
 * 
 *  HandleIrp_AddIrp
 *
 *    This function adds an IRP to the IRP List.
 *
 **********************************************************************/
NTSTATUS HandleIrp_AddIrp(PIRPLISTHEAD pIrpListHead, PIRP pIrp, PDRIVER_CANCEL pDriverCancelRoutine, PFNCLEANUPIRP pfnCleanUpIrp, PVOID pContext)
{
    NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;
    KIRQL kOldIrql;
    PDRIVER_CANCEL pCancelRoutine;
    PIRPLIST pIrpList;

    pIrpList = (PIRPLIST)KMem_AllocateNonPagedMemory(sizeof(IRPLIST), pIrpListHead->ulPoolTag);

    if(pIrpList)
    {
        DbgPrint("HandleIrp_AddIrp Allocate Memory = 0x%0x \r\n", pIrpList);

        pIrpList->pContext      = pContext;
        pIrpList->pfnCleanUpIrp = pfnCleanUpIrp;
        pIrpList->pIrp          = pIrp;
        pIrpList->pfnCancelRoutine = pDriverCancelRoutine;

        /*
         *  The first thing we need to to is acquire our spin lock.
         *
         *  The reason for this is a few things.
         *
         *     1. All access to this list is synchronized, the obvious reason
         *     2. This will synchronize adding this IRP to the list with the cancel routine.
         *
         */
    
        KeAcquireSpinLock(&pIrpListHead->kspIrpListLock, &kOldIrql);
    
        /*
         * We will now attempt to set the cancel routine which will be called
         * when (if) the IRP is ever canceled.  This allows us to remove an IRP
         * from the queue that is no longer valid.
         *
         * A potential misconception is that if the IRP is canceled it is no longer
         * valid.  This is not true the IRP does not self-destruct.  The IRP is valid
         * as long as it has not been completed.  Once it has been completed this is
         * when it is no longer valid (while we own it).  So, while we own the IRP we need
         * to complete it at some point.  The reason for setting a cancel routine is to realize
         * that the IRP has been canceled and complete it immediately and get rid of it.  
         * We don't want to do processing for an IRP that has been canceled as the result
         * will just be thrown away.
         *
         * So, if we remove an IRP from this list for processing and it's canceled the only
         * problem is that we did processing on it.  We complete it at the end and there's no problem.
         *
         * There is a problem however if your code is written in a way that allows your cancel routine
         * to complete the IRP unconditionally.  This is fine as long as you have some type of synchronization
         * since you DO NOT WANT TO COMPLETE AN IRP TWICE!!!!!!
         */
    
        IoSetCancelRoutine(pIrp, pIrpList->pfnCancelRoutine);
    
        /*
         * We have set our cancel routine.  Now, check if the IRP has already been canceled.
         *
         * We must set the cancel routine before checking this to ensure that once we queue
         * the IRP it will definately be called if the IRP is ever canceled.
         */
    
        if(pIrp->Cancel)
        {
            /*
             * If the IRP has been canceled we can then check if our cancel routine has been
             * called.
             */
            pCancelRoutine = IoSetCancelRoutine(pIrp, NULL);
    
            /*
             * if pCancelRoutine == NULL then our cancel routine has been called.
             * if pCancelRoutine != NULL then our cancel routine has not been called.
             *
             * The I/O Manager will set the cancel routine to NULL before calling the cancel routine.
             *
             * We have a decision to make here, we need to write the code in a way that we only
             * complete and clean up the IRP once.  We either allow the cancel routine to do it
             * or we do it here.  Now, we will already have to clean up the IRP here if the
             * pCancelRoutine != NULL.
             *
             * The solution we are going with here is that we will only clean up IRP's in the cancel
             * routine if the are in the list.  So, we will not add any IRP to the list if it has
             * already been canceled once we get to this location.
             *
             */
            KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);
    
            /*
             * We are going to allow the clean up function to complete the IRP.
             */
            pfnCleanUpIrp(pIrp, pContext);
            DbgPrint("HandleIrp_AddIrp Complete Free Memory = 0x%0x \r\n", pIrpList);
            KMem_FreeNonPagedMemory(pIrpList);
        }
        else
        {
            /*
             * The IRP has not been canceled, so we can simply queue it!
             */


            pIrpList->pNextIrp      = NULL;
            
            IoMarkIrpPending(pIrp);

            if(pIrpListHead->pListBack)
            {
               pIrpListHead->pListBack->pNextIrp = pIrpList;
               pIrpListHead->pListBack           = pIrpList;
            }
            else
            {
               pIrpListHead->pListFront = pIrpListHead->pListBack = pIrpList;
            }

            KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);

            NtStatus = STATUS_SUCCESS;
        }
    }
    else
    {
        /*
         * We are going to allow the clean up function to complete the IRP.
         */
        pfnCleanUpIrp(pIrp, pContext);
    }
    

    return NtStatus;      
}

/**********************************************************************
 * 
 *  HandleIrp_RemoveNextIrp
 *
 *    This function removes the next valid IRP.
 *
 **********************************************************************/
PIRP HandleIrp_RemoveNextIrp(PIRPLISTHEAD pIrpListHead)
{
    PIRP pIrp = NULL;
    KIRQL kOldIrql;
    PDRIVER_CANCEL pCancelRoutine;
    PIRPLIST pIrpListCurrent;

    KeAcquireSpinLock(&pIrpListHead->kspIrpListLock, &kOldIrql);
    
    pIrpListCurrent  = pIrpListHead->pListFront;

    while(pIrpListCurrent && pIrp == NULL)
    {
        /*
         * To remove an IRP from the Queue we first want to
         * reset the cancel routine.
         */
        pCancelRoutine = IoSetCancelRoutine(pIrpListCurrent->pIrp, NULL);

        /*
         * The next phase is to determine if this IRP has been canceled
         */

        if(pIrpListCurrent->pIrp->Cancel)
        {
            /*
             * We have been canceled so we need to determine if our cancel routine
             * has already been called.  pCancelRoutine will be NULL if our cancel
             * routine has been called.  If will not be NULL if our cancel routine
             * has not been called.  However, we don't care in either case and we
             * will simply complete the IRP here since we have to implement at
             * least that case anyway.
             *
             * Remove the IRP from the list.
             */

            pIrpListHead->pListFront = pIrpListCurrent->pNextIrp;

            if(pIrpListHead->pListFront == NULL)
            {
                pIrpListHead->pListBack = NULL;
            }

            KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);
            
            pIrpListCurrent->pfnCleanUpIrp(pIrpListCurrent->pIrp, pIrpListCurrent->pContext);

            DbgPrint("HandleIrp_RemoveNextIrp Complete Free Memory = 0x%0x \r\n", pIrpListCurrent);
            KMem_FreeNonPagedMemory(pIrpListCurrent);
            pIrpListCurrent = NULL;

            KeAcquireSpinLock(&pIrpListHead->kspIrpListLock, &kOldIrql);
            
            pIrpListCurrent  = pIrpListHead->pListFront;

        }
        else
        {
            pIrpListHead->pListFront = pIrpListCurrent->pNextIrp;

            if(pIrpListHead->pListFront == NULL)
            {
                pIrpListHead->pListBack = NULL;
            }

            pIrp = pIrpListCurrent->pIrp;

            KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);

            DbgPrint("HandleIrp_RemoveNextIrp Complete Free Memory = 0x%0x \r\n", pIrpListCurrent);
            KMem_FreeNonPagedMemory(pIrpListCurrent);
            pIrpListCurrent = NULL;

            KeAcquireSpinLock(&pIrpListHead->kspIrpListLock, &kOldIrql);
            
        }
    }


    KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);

    return pIrp;
}

/**********************************************************************
 * 
 *  HandleIrp_PerformCancel
 *
 *    This function removes the specified IRP from the list.
 *
 **********************************************************************/
NTSTATUS HandleIrp_PerformCancel(PIRPLISTHEAD pIrpListHead, PIRP pIrp)
{
    NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;
    KIRQL kOldIrql;
    PIRPLIST pIrpListCurrent, pIrpListPrevious;

    KeAcquireSpinLock(&pIrpListHead->kspIrpListLock, &kOldIrql);
    
    pIrpListPrevious = NULL;
    pIrpListCurrent  = pIrpListHead->pListFront;

    while(pIrpListCurrent && NtStatus == STATUS_UNSUCCESSFUL)
    {
        if(pIrpListCurrent->pIrp == pIrp)
        {
            if(pIrpListPrevious)
            {
               pIrpListPrevious->pNextIrp = pIrpListCurrent->pNextIrp;
            }

            if(pIrpListHead->pListFront == pIrpListCurrent)
            {
               pIrpListHead->pListFront = pIrpListCurrent->pNextIrp;
            }

            if(pIrpListHead->pListBack == pIrpListCurrent)
            {
                pIrpListHead->pListBack = pIrpListPrevious;
            }

            KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);
            
            NtStatus = STATUS_SUCCESS;

            /*
             * We are going to allow the clean up function to complete the IRP.
             */
            pIrpListCurrent->pfnCleanUpIrp(pIrpListCurrent->pIrp, pIrpListCurrent->pContext);

            DbgPrint("HandleIrp_PerformCancel Complete Free Memory = 0x%0x \r\n", pIrpListCurrent);

            KMem_FreeNonPagedMemory(pIrpListCurrent);

            pIrpListCurrent = NULL;

            KeAcquireSpinLock(&pIrpListHead->kspIrpListLock, &kOldIrql);

        }
        else
        {
            pIrpListPrevious = pIrpListCurrent;
            pIrpListCurrent = pIrpListCurrent->pNextIrp;
        }
    }


    KeReleaseSpinLock(&pIrpListHead->kspIrpListLock, kOldIrql);

    return NtStatus;
}




