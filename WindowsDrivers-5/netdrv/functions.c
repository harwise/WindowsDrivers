/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Example Driver
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/


// #define _X86_ 

#include <wdm.h>
#include <tdi.h>
#include <tdikrnl.h>
#include "../inc/NETDRV.h"
#include "kmem.h"
#include "tdiexample.h"
#include "handleirp.h"
#include "tdifuncs.h"


typedef enum
{
    CS_NOT_CONNECTED,
    CS_CONNECTING,
    CS_CONNECTED,    
    CS_DISCONNECTING
                                           
} CONNECTION_STATUS;

typedef struct _TDI_EXAMPLE_CONTEXT
{
    TDI_HANDLE TdiHandle;
    CONNECTION_STATUS csConnectionState;
    PIRPLISTHEAD pReadIrpListHead;
    PIRPLISTHEAD pWriteIrpListHead;
    KMUTEX kConnectionLock;
    KEVENT kWriteIrpReady;
    KEVENT kWakeWriteIrpThread;
    KEVENT kInitEvent;
    NTSTATUS NtThreadStatus;
    BOOLEAN bWriteThreadAlive;       
    PFILE_OBJECT pWriteThread;

} TDI_EXAMPLE_CONTEXT, *PTDI_EXAMPLE_CONTEXT;



/**********************************************************************
 * Internal Functions
 **********************************************************************/
 
 VOID TdiExample_CancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP pIrp); 
 VOID TdiExample_IrpCleanUp(PIRP pIrp, PVOID pContext);
 NTSTATUS TdiExample_Connect(PTDI_EXAMPLE_CONTEXT pTdiExampleContext, PVOID pAddressContext, UINT uiLength);
 NTSTATUS TdiExample_Disconnect(PTDI_EXAMPLE_CONTEXT pTdiExampleContext);
 VOID TdiExample_WorkItem(PDEVICE_OBJECT  DeviceObject, PVOID  Context);
 VOID TdiExample_NetworkWriteThread(PVOID StartContext);
 NTSTATUS TdiExample_ClientEventReceive(PVOID TdiEventContext, CONNECTION_CONTEXT ConnectionContext, ULONG ReceiveFlags, ULONG  BytesIndicated, ULONG  BytesAvailable, ULONG  *BytesTaken, PVOID  Tsdu, PIRP  *IoRequestPacket);

#pragma alloc_text(PAGE, TdiExample_NetworkWriteThread)
#pragma alloc_text(PAGE, TdiExample_WorkItem)
#pragma alloc_text(PAGE, TdiExample_Disconnect)
#pragma alloc_text(PAGE, TdiExample_IrpCleanUp)
/* #pragma alloc_text(PAGE, TdiExample_CancelRoutine) */
#pragma alloc_text(PAGE, TdiExample_IoControlInternal)
#pragma alloc_text(PAGE, TdiExample_Create) 
#pragma alloc_text(PAGE, TdiExample_Close) 
#pragma alloc_text(PAGE, TdiExample_IoControl) 
#pragma alloc_text(PAGE, TdiExample_Read)
#pragma alloc_text(PAGE, TdiExample_Write)
#pragma alloc_text(PAGE, TdiExample_UnSupportedFunction)
#pragma alloc_text(PAGE, TdiExample_Connect)
/* #pragma alloc_text(PAGE, TdiExample_ClientEventReceive) */
                                
#define STATUS_CONTINUE_COMPLETION  STATUS_SUCCESS
#define TDIEXAMPLE_POOL_TAG         ((ULONG)'EidT')
#define READ_IRPLIST_POOL_TAG       ((ULONG)'RprI')
#define WRITE_IRPLIST_POOL_TAG      ((ULONG)'WprI')


/**********************************************************************
 * 
 *  TdiExample_Create
 *
 *    This is called when an instance of this driver is created (CreateFile)
 *
 **********************************************************************/
NTSTATUS TdiExample_Create(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS NtStatus = STATUS_INSUFFICIENT_RESOURCES;
    PIO_STACK_LOCATION pIoStackIrp = NULL;
    PTDI_EXAMPLE_CONTEXT pTdiExampleContext = NULL;

    DbgPrint("TdiExample_Create Called \r\n");


    pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);

    /* 
     * We need to allocate our instance context for this handle.  This
     * data structure will be associated with this user mode handle.
     *
     * We are allocating it in Non-Paged Pool so we can use it while holding
     * Spin Locks in our IRP Queue.
     *
     */

    pTdiExampleContext = (PTDI_EXAMPLE_CONTEXT)KMem_AllocateNonPagedMemory(sizeof(TDI_EXAMPLE_CONTEXT), TDIEXAMPLE_POOL_TAG);

    if(pTdiExampleContext)
    {
         DbgPrint("TdiExample_Create Allocate = 0x%0x \r\n", pTdiExampleContext);
        /*
         * We have created a library called "HandleIrp" which helps to queue our Pending IRP's.
         * The first thing we do is create the context for this list.
         */
        pTdiExampleContext->pReadIrpListHead = HandleIrp_CreateIrpList(READ_IRPLIST_POOL_TAG);
        pTdiExampleContext->pWriteIrpListHead = HandleIrp_CreateIrpList(WRITE_IRPLIST_POOL_TAG);

        if(pTdiExampleContext->pWriteIrpListHead && pTdiExampleContext->pReadIrpListHead)
        {
            /*
             * We need to maintain a state for this handle which will help us to know
             * if a Read or Write request should be ignored or performed for example.
             *
             * We obviously start with not connected.
             */
            pTdiExampleContext->csConnectionState = CS_NOT_CONNECTED;
    
            /*
             * We have our own TDI Client Driver library which allows us to simply
             * call functions to initate connections, etc.  The first thing we need to do
             * is create a TdiHandle context which can then be used in other TDI Library Calls.
             */
            NtStatus = TdiFuncs_InitializeTransportHandles(&pTdiExampleContext->TdiHandle);
    
            if(NT_SUCCESS(NtStatus))
            {
                NtStatus = TdiFuncs_SetEventHandler(pTdiExampleContext->TdiHandle.pfoTransport, TDI_EVENT_RECEIVE, (void*)&TdiExample_ClientEventReceive, (PVOID)pTdiExampleContext);
            }

            if(NT_SUCCESS(NtStatus))
            {
                PIO_WORKITEM pIoWorkItem;

                /*
                 * We need to initialize our locks and events for this handle.  
                 *
                 * The Connection Lock is used to Synchronize access to connecting and disconnecting
                 *
                 * The Write IRP Ready event is used to signal the Writer thread
                 * that an IRP has been queued and it's waiting to be sent.
                 *
                 * The Wake Up Write Irp Thread is used to signal the thread to wake up.  It is generally
                 * used to signal the thread to exit.
                 *
                 * kInitEvent is used to synchronize initialization and notify when the work item has
                 * signaled that the thread has been created.
                 *
                 * The bWriteThreadAlive variable is used to tell the thread it should exit. 
                 */
                KeInitializeMutex(&pTdiExampleContext->kConnectionLock, 0);
                
                KeInitializeEvent(&pTdiExampleContext->kWriteIrpReady, SynchronizationEvent, FALSE);
                KeInitializeEvent(&pTdiExampleContext->kWakeWriteIrpThread, SynchronizationEvent, FALSE); 
                KeInitializeEvent(&pTdiExampleContext->kInitEvent, NotificationEvent, FALSE); 
                    
                pTdiExampleContext->bWriteThreadAlive = TRUE;   
                
                /* 
                 * We want to create a SYSTEM thread so we can handle our Writes Asynchronously.
                 * The problem is that we can't call PsCreateSystemThread() outside of the SYSTEM
                 * process on Windows 2000.  On Windows XP/2003 we can by using the object attributes,
                 * however we want this driver to run on Windows 2000 as well.  So we need to create
                 * a work item which will process some work on our behalf in the system context.
                 *
                 * We can use this to be in the context of SYSTEM to create our thread.
                 *
                 * IO_WORKITEM is an opaque data structure used by the system.
                 *
                 */

                pIoWorkItem = IoAllocateWorkItem(DeviceObject);

                if(pIoWorkItem)
                {

                    IoQueueWorkItem(pIoWorkItem, TdiExample_WorkItem, DelayedWorkQueue, (PVOID)pTdiExampleContext);
                    
                    /* 
                     * Wait for the work item to complete creating the thread.
                     */
                    KeWaitForSingleObject(&pTdiExampleContext->kInitEvent, Executive, KernelMode, FALSE, NULL);
                    
                    NtStatus = pTdiExampleContext->NtThreadStatus;

                    /*
                     * It is safe to free the work item when the Work Item function is called.  The work
                     * item is dequeued before the function is called.  It is not safe to free a work item
                     * that is currently queued.
                     */
                    IoFreeWorkItem(pIoWorkItem);

                    if(NT_SUCCESS(NtStatus))
                    {
                        /*
                         * Set the context of our FILE_OBJECT
                         */
                        pIoStackIrp->FileObject->FsContext = (PVOID)pTdiExampleContext;
                    }
                    else
                    {
                        HandleIrp_FreeIrpList(pTdiExampleContext->pReadIrpListHead);
                        HandleIrp_FreeIrpList(pTdiExampleContext->pWriteIrpListHead);
                        DbgPrint("TdiExample_Create Free = 0x%0x \r\n", pTdiExampleContext);

                        KMem_FreeNonPagedMemory(pTdiExampleContext);
                    }
                }
                else
                {
                    HandleIrp_FreeIrpList(pTdiExampleContext->pReadIrpListHead);
                    HandleIrp_FreeIrpList(pTdiExampleContext->pWriteIrpListHead);
                    DbgPrint("TdiExample_Create Free = 0x%0x \r\n", pTdiExampleContext);

                    KMem_FreeNonPagedMemory(pTdiExampleContext);
                }

            }
            else
            {
                HandleIrp_FreeIrpList(pTdiExampleContext->pReadIrpListHead);
                HandleIrp_FreeIrpList(pTdiExampleContext->pWriteIrpListHead);
                DbgPrint("TdiExample_Create Free = 0x%0x \r\n", pTdiExampleContext);

                KMem_FreeNonPagedMemory(pTdiExampleContext);
            }
        }
        else
        {
            if(pTdiExampleContext->pWriteIrpListHead)
            {
                HandleIrp_FreeIrpList(pTdiExampleContext->pWriteIrpListHead);
            }

            if(pTdiExampleContext->pReadIrpListHead)
            {
                HandleIrp_FreeIrpList(pTdiExampleContext->pReadIrpListHead);
            }
            DbgPrint("TdiExample_Create Free = 0x%0x \r\n", pTdiExampleContext);

            KMem_FreeNonPagedMemory(pTdiExampleContext);

        }
    }

    Irp->IoStatus.Status      = NtStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DbgPrint("TdiExample_Create Exit 0x%0x \r\n", NtStatus);

    return NtStatus;
}

/**********************************************************************
 * 
 *  TdiExample_WorkItem
 *
 *    This is executed in the context of the SYSTEM process.
 *
 **********************************************************************/
VOID TdiExample_WorkItem(PDEVICE_OBJECT  DeviceObject, PVOID  Context)
{
   PTDI_EXAMPLE_CONTEXT pTdiExampleContext = (PVOID)Context;
   HANDLE WriteThreadHandle;

   DbgPrint("TdiExample_WorkItem Enter \r\n");
    /*
     * We want to allow asynchronous Reads and Writes.  To handle our Writes we will have our
     * own thread in the SYSTEM process space.  This thread will simply be woken up when we have
     * Write's on the Queue and perform the write.
     *
     * If a write is issued Asynchronous the I/O Manager will return to user mode and allow this
     * thread to continue.  If it is not, even though we return from our write and post to this
     * thread the I/O Manager will wait for it to finish, just as we are doing when we issue TDI
     * IRPs.
     */
    pTdiExampleContext->NtThreadStatus = PsCreateSystemThread(&WriteThreadHandle, 0, NULL, NULL, NULL, TdiExample_NetworkWriteThread, (PVOID)pTdiExampleContext);
    
    if(NT_SUCCESS(pTdiExampleContext->NtThreadStatus))
    {
        /*
         * We would rather use the PFILE_OBJECT since it is a pointer to the object rather than a handle.  Handles
         * are only good from within the context of a particular process.
         */
        ObReferenceObjectByHandle(WriteThreadHandle, GENERIC_READ | GENERIC_WRITE, NULL, KernelMode, (PVOID *)&pTdiExampleContext->pWriteThread, NULL);
        ZwClose(WriteThreadHandle);
    }

    KeSetEvent(&pTdiExampleContext->kInitEvent, IO_NO_INCREMENT, FALSE);
}


/**********************************************************************
 * 
 *  TdiExample_NetworkWriteThread
 *
 *    This is executed in the context of the SYSTEM process.
 *    This is where we perform our Network Writes
 *
 **********************************************************************/
VOID TdiExample_NetworkWriteThread(PVOID StartContext)
{
    PTDI_EXAMPLE_CONTEXT pTdiExampleContext = (PVOID)StartContext;
    PVOID pEvents[2];
    NTSTATUS NtStatus;

    DbgPrint("TdiExample_NetworkWriteThread Called \r\n");

    /*
     * This thread will continue until the instance handle is closed.
     *
     */
    while(pTdiExampleContext->bWriteThreadAlive)
    {
         pEvents[0] = (PVOID)&pTdiExampleContext->kWriteIrpReady;
         pEvents[1] = (PVOID)&pTdiExampleContext->kWakeWriteIrpThread;

         NtStatus = KeWaitForMultipleObjects(2, (PVOID)pEvents, WaitAny, Executive, KernelMode, FALSE, NULL, NULL);

         /*
          * The first wait object will return "STATUS_WAIT_0".  Other objects will
          * increment from there.  We only send data on a wait of 0.  The other
          * return values from this function are simply used to wake up not send data.
          */
         if(NtStatus == STATUS_WAIT_0)
         {
             BOOLEAN bMoreIrps = FALSE;
             PIRP Irp;
             /*
              * We will need to remove an IRP from the Queue and then send the data.  We will
              * continue to do this until there are no more IRP's to send or the thread
              * has been terminated.
              */
             do {
                 bMoreIrps = FALSE;

                 Irp = HandleIrp_RemoveNextIrp(pTdiExampleContext->pWriteIrpListHead);

                 if(Irp)
                 {  
                    NTSTATUS NtStatus;
                    UINT uiDataSent;
                    PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);

                    Irp->Tail.Overlay.DriverContext[0] = NULL;

                    bMoreIrps = TRUE;
                    
                    NtStatus = TdiFuncs_Send(pTdiExampleContext->TdiHandle.pfoConnection, Irp->AssociatedIrp.SystemBuffer, pIoStackLocation->Parameters.Write.Length, &uiDataSent);

                    Irp->IoStatus.Status      = NtStatus;
                    Irp->IoStatus.Information = uiDataSent;

                    DbgPrint("TdiExample_NetworkWriteThread Complete IRP = 0x%0x \r\n", Irp);
                    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

                 }


             } while(pTdiExampleContext->bWriteThreadAlive && bMoreIrps);
         }
    }
    DbgPrint("TdiExample_NetworkWriteThread Exit \r\n");
}



/**********************************************************************
 * 
 *  TdiExample_CleanUp
 *
 *    This is called when an instance of this driver is closed (CloseHandle)
 *
 *    This is actually IRP_MJ_CLEANUP so we can release the IRP's.
 *
 **********************************************************************/
NTSTATUS TdiExample_CleanUp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS NtStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIoStackIrp = NULL;
    PTDI_EXAMPLE_CONTEXT pTdiExampleContext = NULL;

    DbgPrint("TdiExample_CleanUp Called \r\n");

    pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);

    pTdiExampleContext = (PTDI_EXAMPLE_CONTEXT)pIoStackIrp->FileObject->FsContext;

    if(pTdiExampleContext)
    {
        /*
         * The first thing we want to do is close the write thread.  We will do this
         * by signaling the thread and then waiting for it to be destroyed.
         *
         * 1. Set Boolean to False
         * 2. Signal Thread to Wake Up
         * 3. Wait for Thread Object to become signaled
         * 4. Remove Reference to Thread Object
         *
         */
        pTdiExampleContext->bWriteThreadAlive = FALSE;

        KeSetEvent(&pTdiExampleContext->kWakeWriteIrpThread, IO_NO_INCREMENT, FALSE);

        NtStatus = KeWaitForSingleObject(pTdiExampleContext->pWriteThread, Executive, KernelMode, FALSE, NULL);

        DbgPrint("Write Thread Close - 0x%0x\n", NtStatus);

        NtStatus = STATUS_SUCCESS;

        ObDereferenceObject(pTdiExampleContext->pWriteThread);

        pTdiExampleContext->pWriteThread = NULL;
                
        /*
         * If the handle is connected disconnect.  We no longer
         * require the connection lock since we are currently closing this handle.
         */
        if(pTdiExampleContext->csConnectionState == CS_CONNECTED)
        {
           TdiFuncs_Disconnect(pTdiExampleContext->TdiHandle.pfoConnection);
           pTdiExampleContext->csConnectionState = CS_NOT_CONNECTED;
        }

        /*
         * Free Handles, IRPs, etc.
         */
        TdiFuncs_FreeHandles(&pTdiExampleContext->TdiHandle);
        
        HandleIrp_FreeIrpListWithCleanUp(pTdiExampleContext->pReadIrpListHead);
        HandleIrp_FreeIrpListWithCleanUp(pTdiExampleContext->pWriteIrpListHead);

        DbgPrint("TdiExample_CleanUp Free = 0x%0x \r\n", pTdiExampleContext);
        KMem_FreeNonPagedMemory(pTdiExampleContext);

        pIoStackIrp->FileObject->FsContext = NULL;
    }

    Irp->IoStatus.Status      = NtStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DbgPrint("TdiExample_CleanUp Exit 0x%0x \r\n", NtStatus);

    
    return NtStatus;
}


/**********************************************************************
 * 
 *  TdiExample_IoControlInternal
 *
 *    These are IOCTL's which can only be sent by other drivers.
 *
 **********************************************************************/
NTSTATUS TdiExample_IoControlInternal(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS NtStatus = STATUS_NOT_SUPPORTED;
    PIO_STACK_LOCATION pIoStackIrp = NULL;

    DbgPrint("TdiExample_IoControlInternal Called \r\n");

    Irp->IoStatus.Status      = NtStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DbgPrint("TdiExample_IoControlInternal Exit 0x%0x \r\n", NtStatus);

    return NtStatus;
}



/**********************************************************************
 * 
 *  TdiExample_IoControl
 *
 *    This is called when an IOCTL is issued on the device handle (DeviceIoControl)
 *
 **********************************************************************/
NTSTATUS TdiExample_IoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS NtStatus = STATUS_NOT_SUPPORTED;
    PIO_STACK_LOCATION pIoStackIrp = NULL;
    PTDI_EXAMPLE_CONTEXT pTdiExampleContext = NULL;
    
    DbgPrint("TdiExample_IoControl Called \r\n");

    /*
     * Each time the IRP is passed down the driver stack a new stack location is added
     * specifying certain parameters for the IRP to the driver.
     */
    pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);    

    if(pIoStackIrp) /* Should Never Be NULL! */
    {
        DbgPrint("Example_IoControl Called IOCTL = 0x%0x\r\n", pIoStackIrp->Parameters.DeviceIoControl.IoControlCode);
        
        pTdiExampleContext = (PTDI_EXAMPLE_CONTEXT)pIoStackIrp->FileObject->FsContext;

        switch(pIoStackIrp->Parameters.DeviceIoControl.IoControlCode)
        {
            case IOCTL_TDIEXAMPLE_CONNECT:
                 NtStatus = TdiExample_Connect(pTdiExampleContext, Irp->AssociatedIrp.SystemBuffer, pIoStackIrp->Parameters.DeviceIoControl.InputBufferLength);
                 break;
            
            case IOCTL_TDIEXAMPLE_DISCONNECT:
                 NtStatus = TdiExample_Disconnect(pTdiExampleContext);
                 break;

        }
    }


    Irp->IoStatus.Status      = NtStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DbgPrint("TdiExample_IoControl Exit 0x%0x \r\n", NtStatus);

    return NtStatus;
}






/**********************************************************************
 * 
 *  TdiExample_Write
 *
 *    This is called when a write is issued on the device handle (WriteFile/WriteFileEx)
 *
 *
 **********************************************************************/
NTSTATUS TdiExample_Write(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION pIoStackIrp = NULL;
    PTDI_EXAMPLE_CONTEXT pTdiExampleContext = NULL;


    DbgPrint("TdiExample_Write Called 0x%0x\r\n", Irp);
    
    /*
     * Each time the IRP is passed down the driver stack a new stack location is added
     * specifying certain parameters for the IRP to the driver.
     */
    pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);    
    
    pTdiExampleContext = (PTDI_EXAMPLE_CONTEXT)pIoStackIrp->FileObject->FsContext;

    if(pTdiExampleContext->csConnectionState == CS_CONNECTED)
    {
        /*
         * We can use the "Overlay.DriverContext" data on the IRP to use driver specific contexts
         * onto an IRP but only WHILE WE OWN THE IRP.  Since we will be owning this IRP
         * we can use this as the context we can find in our cancel routine.  The problem
         * is that we cannot use teh IO_STACK_LOCATION in the cancel routine since we 
         * can't be guarentteed what the current stack location is when the cancel
         * routine is called.
         *
         * We then add our IRP to the list.
         */
        Irp->Tail.Overlay.DriverContext[0] = (PVOID)pTdiExampleContext->pWriteIrpListHead;
        NtStatus = HandleIrp_AddIrp(pTdiExampleContext->pWriteIrpListHead, Irp, TdiExample_CancelRoutine, TdiExample_IrpCleanUp, NULL);

        if(NT_SUCCESS(NtStatus))
        {
            KeSetEvent(&pTdiExampleContext->kWriteIrpReady, IO_NO_INCREMENT, FALSE);
            NtStatus = STATUS_PENDING;
        }

    }
    else
    {
    
        Irp->IoStatus.Status      = NtStatus;
        Irp->IoStatus.Information = 0;
    
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    DbgPrint("TdiExample_Write Exit 0x%0x \r\n", NtStatus);

    
    return NtStatus;
}


/**********************************************************************
 * 
 *  TdiExample_Read
 *
 *    This is called when a read is issued on the device handle (ReadFile/ReadFileEx)
 *
 *
 **********************************************************************/
NTSTATUS TdiExample_Read(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS NtStatus = STATUS_BUFFER_TOO_SMALL;
    PIO_STACK_LOCATION pIoStackIrp = NULL;
    PTDI_EXAMPLE_CONTEXT pTdiExampleContext = NULL;

    DbgPrint("TdiExample_Read Called 0x%0x \r\n", Irp);

    /*
     * Each time the IRP is passed down the driver stack a new stack location is added
     * specifying certain parameters for the IRP to the driver.
     */
    pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);    
    
    pTdiExampleContext = (PTDI_EXAMPLE_CONTEXT)pIoStackIrp->FileObject->FsContext;

/*    
     To keep the example simple we will allow read requests to be queued before
     the user is connected so that we can get any initial data that is sent to us.
     
     if(pTdiExampleContext->csConnectionState == CS_CONNECTED)
    {*/
        /*
         * We can use the "Overlay.DriverContext" data on the IRP to use driver specific contexts
         * onto an IRP but only WHILE WE OWN THE IRP.  Since we will be owning this IRP
         * we can use this as the context we can find in our cancel routine.  The problem
         * is that we cannot use teh IO_STACK_LOCATION in the cancel routine since we 
         * can't be guarentteed what the current stack location is when the cancel
         * routine is called.
         *
         * We then add our IRP to the list.
         */
        Irp->Tail.Overlay.DriverContext[0] = (PVOID)pTdiExampleContext->pReadIrpListHead;
        NtStatus = HandleIrp_AddIrp(pTdiExampleContext->pReadIrpListHead, Irp, TdiExample_CancelRoutine, TdiExample_IrpCleanUp, NULL);

        if(NT_SUCCESS(NtStatus))
        {
            NtStatus = STATUS_PENDING;
        }

  /*  }
    else
    {
    
        Irp->IoStatus.Status      = NtStatus;
        Irp->IoStatus.Information = 0;
    
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    } */

    DbgPrint("TdiExample_Read Exit 0x%0x \r\n", NtStatus);

    return NtStatus;
}


/**********************************************************************
 * 
 *  TdiExample_Close
 *
 *    This function is called to close the handle
 *
 **********************************************************************/
NTSTATUS TdiExample_Close(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS NtStatus = STATUS_SUCCESS;
    DbgPrint("TdiExample_Close Called \r\n");


    Irp->IoStatus.Status      = NtStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DbgPrint("TdiExample_Close Exit 0x%0x \r\n", NtStatus);    
    
    return NtStatus;
}

/**********************************************************************
 * 
 *  TdiExample_UnSupportedFunction
 *
 *    This is called when a major function is issued that isn't supported.
 *
 **********************************************************************/
NTSTATUS TdiExample_UnSupportedFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS NtStatus = STATUS_NOT_SUPPORTED;
    DbgPrint("TdiExample_UnSupportedFunction Called \r\n");


    Irp->IoStatus.Status      = NtStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DbgPrint("TdiExample_UnSupportedFunction Exit 0x%0x \r\n", NtStatus);    
    
    return NtStatus;
}


/**********************************************************************
 * 
 *  TdiExample_CancelRoutine
 *
 *    This function is called if the IRP is ever canceled
 *
 *    CancelIo() from user mode, IoCancelIrp() from the Kernel
 *
 **********************************************************************/
VOID TdiExample_CancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP pIrp)
{
    PIRPLISTHEAD pIrpListHead = NULL;

    /*
     * We must release the cancel spin lock
     */

    IoReleaseCancelSpinLock(pIrp->CancelIrql);
    
    DbgPrint("TdiExample_CancelRoutine Called IRP = 0x%0x \r\n", pIrp);

    /*
     * We stored the IRPLISTHEAD context in our DriverContext on the IRP
     * before adding it to the queue so it should not be NULL here.
     */

    pIrpListHead = (PIRPLISTHEAD)pIrp->Tail.Overlay.DriverContext[0];
    pIrp->Tail.Overlay.DriverContext[0] = NULL;
    
    /*
     * We can then just throw the IRP to the PerformCancel
     * routine since it will find it in the queue, remove it and
     * then call our clean up routine.  Our clean up routine
     * will then complete the IRP.  If this does not occur then
     * our completion of the IRP will occur in another context
     * since it is not in the list.
     */
    HandleIrp_PerformCancel(pIrpListHead, pIrp);

}



/**********************************************************************
 * 
 *  TdiExample_Connect
 *
 *    This function connects to a server.
 *
 **********************************************************************/
NTSTATUS TdiExample_Connect(PTDI_EXAMPLE_CONTEXT pTdiExampleContext, PVOID pAddressContext, UINT uiLength)
{
    NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;
    PNETWORK_ADDRESS pNetworkAddress = (PNETWORK_ADDRESS)pAddressContext;

    /*
     * Enter Critical Section
     *
     */
    KeWaitForMutexObject(&pTdiExampleContext->kConnectionLock, Executive, KernelMode, FALSE, NULL);

    if(uiLength >= sizeof(NETWORK_ADDRESS) && pTdiExampleContext->csConnectionState == CS_NOT_CONNECTED)
    {
        /*
         * We are not connected, we need to connect so we set an intermediate state
         * so that while the mutex is released any other threads checking the state will know
         * we are in a connecting state.
         *
         */

        pTdiExampleContext->csConnectionState = CS_CONNECTING;

        KeReleaseMutex(&pTdiExampleContext->kConnectionLock, FALSE);

        NtStatus = TdiFuncs_Connect(pTdiExampleContext->TdiHandle.pfoConnection, pNetworkAddress->ulAddress, pNetworkAddress->usPort);

        KeWaitForMutexObject(&pTdiExampleContext->kConnectionLock, Executive, KernelMode, FALSE, NULL);

        if(NT_SUCCESS(NtStatus))
        {
            pTdiExampleContext->csConnectionState = CS_CONNECTED;
        }
        else
        {
            pTdiExampleContext->csConnectionState = CS_NOT_CONNECTED;
        }
    }

    KeReleaseMutex(&pTdiExampleContext->kConnectionLock, FALSE);

    return NtStatus;
}

/**********************************************************************
 * 
 *  TdiExample_Disconnect
 *
 *    This function disconnects from a server.
 *
 **********************************************************************/
NTSTATUS TdiExample_Disconnect(PTDI_EXAMPLE_CONTEXT pTdiExampleContext)
{
    NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;

    /*
     * Enter Critical Section
     *
     */
    KeWaitForMutexObject(&pTdiExampleContext->kConnectionLock, Executive, KernelMode, FALSE, NULL);

    if(pTdiExampleContext->csConnectionState == CS_CONNECTED)
    {

        /*
         * We are connected, we need to disconnect so we set an intermediate state
         * so that while the mutex is released any other threads checking the state will know
         * we are in a disconnecting state.
         *
         */

        pTdiExampleContext->csConnectionState = CS_DISCONNECTING;

        KeReleaseMutex(&pTdiExampleContext->kConnectionLock, FALSE);
        
        NtStatus = TdiFuncs_Disconnect(pTdiExampleContext->TdiHandle.pfoConnection);
        KeWaitForMutexObject(&pTdiExampleContext->kConnectionLock, Executive, KernelMode, FALSE, NULL);

        pTdiExampleContext->csConnectionState = CS_NOT_CONNECTED;
    }

    KeReleaseMutex(&pTdiExampleContext->kConnectionLock, FALSE);



    return NtStatus;
}

/**********************************************************************
 * 
 *  TdiExample_IrpCleanUp
 *
 *    This function is called to clean up the IRP if it is ever
 *    canceled after we have given it to the queueing routines.
 *
 **********************************************************************/
VOID TdiExample_IrpCleanUp(PIRP pIrp, PVOID pContext)
{
    pIrp->IoStatus.Status      = STATUS_CANCELLED;
    pIrp->IoStatus.Information = 0;
    pIrp->Tail.Overlay.DriverContext[0] = NULL;
    DbgPrint("TdiExample_IrpCleanUp Called IRP = 0x%0x \r\n", pIrp);

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
}



/**********************************************************************
 * 
 *  TdiExample_ClientEventReceive
 *
 *    This function is called when data is recieved.
 *
 **********************************************************************/
NTSTATUS TdiExample_ClientEventReceive(PVOID TdiEventContext, CONNECTION_CONTEXT ConnectionContext, ULONG ReceiveFlags, ULONG  BytesIndicated, ULONG  BytesAvailable, ULONG  *BytesTaken, PVOID  Tsdu, PIRP  *IoRequestPacket)
{
    NTSTATUS NtStatus = STATUS_SUCCESS;
    UINT uiDataRead = 0;
    PTDI_EXAMPLE_CONTEXT pTdiExampleContext = (PTDI_EXAMPLE_CONTEXT)TdiEventContext;
    PIRP pIrp;

    DbgPrint("TdiExample_ClientEventReceive 0x%0x, %i, %i\n", ReceiveFlags, BytesIndicated, BytesAvailable);


    *BytesTaken = BytesAvailable;
    
    /*
     * This implementation is extremely simple.  We do not queue data if we do not have
     * an IRP to put it there.  We also assume we always get the full data packet
     * sent every recieve.  These are Bells and Whistles that can easily be added to
     * any implementation but would help to make the implementation more complex and
     * harder to follow the underlying idea.  Since those essentially are common-sense
     * add ons they are ignored and the general implementation of how to Queue IRP's and 
     * recieve data are implemented.
     *
     */

    pIrp = HandleIrp_RemoveNextIrp(pTdiExampleContext->pReadIrpListHead);

    if(pIrp)
    {
        PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(pIrp);

        uiDataRead = BytesAvailable > pIoStackLocation->Parameters.Read.Length ? pIoStackLocation->Parameters.Read.Length : BytesAvailable;

        pIrp->Tail.Overlay.DriverContext[0] = NULL;

        RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer, Tsdu, uiDataRead);

        pIrp->IoStatus.Status      = NtStatus;
        pIrp->IoStatus.Information = uiDataRead;
        DbgPrint("TdiExample_ClientEventReceive Called IRP = 0x%0x \r\n", pIrp);

        IoCompleteRequest(pIrp, IO_NETWORK_INCREMENT);
    }

    /*
     * The I/O Request can be used to recieve the rest of the data.  We are not using
     * it in this example however and will actually be assuming that we always get
     * all the data.
     *
     */
    *IoRequestPacket = NULL;

    return NtStatus;
}




