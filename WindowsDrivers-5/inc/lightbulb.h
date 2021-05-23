/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Light Bulb Library
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/

#ifndef __LIGHTBULB_H__
#define __LIGHTBULB_H__

HANDLE Network_CreateLightBulb(void);                                    
BOOL Network_Connect(HANDLE hLightBulb, ULONG ulAddress, USHORT usPort);
void Network_Disconnect(HANDLE hLightBulb);
UINT Network_SendAsync(HANDLE hLightBulb, PVOID pData, UINT uiSize, LPOVERLAPPED lpOverLapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCallback);
UINT Network_RecieveAsync(HANDLE hLightBulb, PVOID pData, UINT uiSize, LPOVERLAPPED lpOverLapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCallback);
void Network_CloseLightBulb(HANDLE hLightBulb);


#endif





