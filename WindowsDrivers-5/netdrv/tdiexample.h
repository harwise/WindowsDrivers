/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Driver Example
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *     Driver Shared Header File
 **********************************************************************/




#ifndef __TDIEXAMPLE_H__
#define __TDIEXAMPLE_H__

typedef unsigned int UINT;
typedef char * PCHAR;

/* #define __USE_DIRECT__ */
#define __USE_BUFFERED__

NTSTATUS TdiExample_Create(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS TdiExample_CleanUp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS TdiExample_Close(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS TdiExample_IoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS TdiExample_Write(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS TdiExample_Read(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS TdiExample_UnSupportedFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS TdiExample_IoControlInternal(PDEVICE_OBJECT DeviceObject, PIRP Irp);



#endif






