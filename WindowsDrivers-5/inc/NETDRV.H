/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Network Interface Driver
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/

#ifndef __NETDRV_H__
#define __NETDRV_H__

/*
 * This IOCTL tells the driver to make a connection
 *
 *  INPUT: NETWORK_ADDRESS
 *
 *  OUTPUT: None
 *
 */
#define IOCTL_TDIEXAMPLE_CONNECT     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

typedef struct _NETWORK_ADDRESS
{
    ULONG ulAddress;
    USHORT usPort;

} NETWORK_ADDRESS, *PNETWORK_ADDRESS;


/*
 * This IOCTL tells the driver to disconnect
 *
 *  INPUT: None
 *
 *  OUTPUT: None
 *
 */
#define IOCTL_TDIEXAMPLE_DISCONNECT     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)


#endif




