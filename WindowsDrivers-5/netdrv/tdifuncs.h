/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Example TDI Interface
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/

#ifndef __TDIFUNCS_H__
#define __TDIFUNCS_H__

#define NETWORK_BYTE_ORDER_LONG(x)  ((x<<24) | (x>>24) | ((x<<8)&0xFF0000) | ((x>>8)&0xFF00))
#define NETWORK_BYTE_ORDER_SHORT(x) ((x<<8)&0xFF00) | ((x>>8)&0xFF)

typedef struct _TDI_HANDLE 
{
    HANDLE hTransport;
    HANDLE hConnection;
    PFILE_OBJECT pfoTransport;
    PFILE_OBJECT pfoConnection;

} TDI_HANDLE, *PTDI_HANDLE;

/*
 * Quick Function Wrappers
 */
void TdiFuncs_FreeHandles(PTDI_HANDLE pTdiHandle);
NTSTATUS TdiFuncs_InitializeTransportHandles(PTDI_HANDLE pTdiHandle);

/*
 * Raw Functionality Functions
 */
NTSTATUS TdiFuncs_OpenTransportAddress(PHANDLE pTdiHandle, PFILE_OBJECT *pFileObject);
NTSTATUS TdiFuncs_OpenConnection(PHANDLE pTdiHandle, PFILE_OBJECT *pFileObject);
NTSTATUS TdiFuncs_AssociateTransportAndConnection(HANDLE hTransportAddress, PFILE_OBJECT pfoConnection);
NTSTATUS TdiFuncs_CloseTdiOpenHandle(HANDLE hTdiHandle, PFILE_OBJECT pfoTdiFileObject);
NTSTATUS TdiFuncs_DisAssociateTransportAndConnection(PFILE_OBJECT pfoConnection);
NTSTATUS TdiFuncs_Connect(PFILE_OBJECT pfoConnection, UINT uiAddress, USHORT uiPort);
NTSTATUS TdiFuncs_Send(PFILE_OBJECT pfoConnection, PVOID pData, UINT uiSendLength, UINT *pDataSent);
NTSTATUS TdiFuncs_Disconnect(PFILE_OBJECT pfoConnection);
NTSTATUS TdiFuncs_CompleteIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
NTSTATUS TdiFuncs_SetEventHandler(PFILE_OBJECT pfoTdiFileObject, LONG InEventType, PVOID InEventHandler, PVOID InEventContext);


#endif



