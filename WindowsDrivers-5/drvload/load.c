/**********************************************************************
 * 
 *  Toby Opferman
 *
 *  Example Dynamic Loading a Driver
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/


#include <windows.h>
#include <stdio.h>

/*********************************************************
 *   Main Function Entry
 *
 *********************************************************/
int _cdecl main(void)
{
    HANDLE hSCManager;
    HANDLE hService;
    SERVICE_STATUS ss;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    
    printf("Starting TDI Client Driver Example\n");

    if(hSCManager)
    {
        printf("Initializing\n");

        hService = CreateServiceA(hSCManager, "NETDRV", "TDI Client Driver Example", SERVICE_START | DELETE | SERVICE_STOP, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, "C:\\netdrv.sys", NULL, NULL, NULL, NULL, NULL);

        if(!hService)
        {
            hService = OpenServiceA(hSCManager, "NETDRV", SERVICE_START | DELETE | SERVICE_STOP);
        }

        if(hService)
        {
            printf("Starting Driver\n");

            StartService(hService, 0, NULL);
            printf("Press Enter to stop the driver\r\n");
            getchar();
            ControlService(hService, SERVICE_CONTROL_STOP, &ss);

            CloseServiceHandle(hService);

            DeleteService(hService);
        }

        CloseServiceHandle(hSCManager);
    }
    
    return 0;
}


