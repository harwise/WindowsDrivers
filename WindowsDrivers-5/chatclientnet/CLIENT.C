/**********************************************************************
 * 
 *  Toby Opferman
 *
 * Chat Client using Lightbulbs
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/

/* Header Files */
#include <windows.h>
#include <winsock.h>
#include "../inc/lightbulb.h"
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#pragma comment(lib, "WS2_32.LIB")

/**********************************************************************
 * Internal Data Structures
 **********************************************************************/
 /*
  *  OVERLAPPED READ WITH CONTEXT
  */
 typedef struct _network_async_read
 {
     OVERLAPPED ov;
     char *pszBuffer;
     UINT uiSize;
     HANDLE hClientLightBulb;
 } NETWORK_ASYNC_READ, *PNETWORK_ASYNC_READ;

 /*
  *  OVERLAPPED WRITE WITH CONTEXT
  */
 typedef struct _network_async_write
 {
     OVERLAPPED ov;
     BOOL bWriteComplete;

 } NETWORK_ASYNC_WRITE, *PNETWORK_ASYNC_WRITE;

 
 typedef struct _client_connect_info
 {
     char szNick[12];
     char szTempBuffer[100];
     ULONG ulAddress;
     USHORT usPort;
     NETWORK_ASYNC_READ FirstAsyncRead;

 } CLIENT_CONNECT_INFO, *PCLIENT_CONNECT_INFO;

 typedef enum
 {
   
   CLIENT_LIGHTBULB_INIT_ERROR = 1,
   CLIENT_LIGHTBULB_BULB_ERROR,
   CLIENT_LIGHTBULB_CONNECT_ERROR,
   CLIENT_LIGHTBULB_SEND_ERROR,
   CLIENT_LIGHTBULB_SERVER_CLOSED_ERROR

 } CLIENT_ERROR;


/**********************************************************************
 * Internal Prototypes
 **********************************************************************/
 BOOL Client_InitializeLightBulbs(void);
 void Client_FreeLightBulbs(void);
 BOOL Client_GetHostInfoFromUser(PCLIENT_CONNECT_INFO pClientConnectInfo);
 HANDLE Client_ConnectToHost(PCLIENT_CONNECT_INFO pClientConnectInfo);
 void Client_RunClient(HANDLE hClientLightBulb, char *pszNick);
 void Client_DisplayErrorMsg(CLIENT_ERROR uiMsg);
 VOID CALLBACK Client_FirstReadCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
 VOID CALLBACK Client_ReadCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
 VOID CALLBACK Client_WriteCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

 
/**********************************************************************
 * 
 *  main
 *
 *    Console App Entry Function
 *
 **********************************************************************/
 int __cdecl main(void)
 {
     CLIENT_CONNECT_INFO ClientConnectInfo = {0};

     if(Client_InitializeLightBulbs())
     {
         HANDLE hConnectSocket;

         if(Client_GetHostInfoFromUser(&ClientConnectInfo))
         {
             hConnectSocket = Client_ConnectToHost(&ClientConnectInfo);

             if(hConnectSocket != INVALID_SOCKET)
             {
                Client_RunClient(hConnectSocket, ClientConnectInfo.szNick);
             }
         }

         Client_FreeLightBulbs();
     }

     return 0;
 }

/**********************************************************************
 * 
 *  Client_InitializeLightBulbs
 *
 *    Initialize Light Bulbs
 *
 **********************************************************************/
 BOOL Client_InitializeLightBulbs(void)
 {
     BOOL bInitialized = TRUE;

     return bInitialized;
 }


/**********************************************************************
 * 
 *  Client_FreeLightBulbs
 *
 *    Free Winsock
 *
 **********************************************************************/
 void Client_FreeLightBulbs(void)
 {
     /* No Implementation */
 }

/**********************************************************************
 * 
 *  Client_DisplayErrorMsg
 *
 *    Displays Error Messages
 *
 **********************************************************************/ 
 void Client_DisplayErrorMsg(CLIENT_ERROR uiMsg)
 {
     switch(uiMsg)
     {
         case CLIENT_LIGHTBULB_INIT_ERROR:
              printf("Chat Client - Cannot Initialize Light Bulbs!\n");
              break;

         case CLIENT_LIGHTBULB_BULB_ERROR:
              printf("Chat Client - Cannot Create Light Bulb!\n");
              break;

         case CLIENT_LIGHTBULB_CONNECT_ERROR:
              printf("Chat Server - Cannot Connect to Remote Server!\n");
              break;

         case CLIENT_LIGHTBULB_SEND_ERROR:
              printf("Chat Server - Light Bulb Error Data Not Sent!\n");
              break;

         case CLIENT_LIGHTBULB_SERVER_CLOSED_ERROR:
              printf("Chat Server - Server Closed!\n");
              break;

         default:
              printf("Chat Client - Runtime Error!\n");

     }
 }


/**********************************************************************
 * 
 *  Client_GetHostInfoFromUser
 *
 *    Get User Information From Host
 *
 **********************************************************************/ 
 BOOL Client_GetHostInfoFromUser(PCLIENT_CONNECT_INFO pClientConnectInfo)
 {
     BOOL bGotUserInfo = TRUE;
     char szHostName[101];

     printf("Enter Your Name (Or A Nick Name, Max 11 Characters)\n");
     fgets(pClientConnectInfo->szNick, 12, stdin);

     if(strlen(pClientConnectInfo->szNick) == 0 || (strlen(pClientConnectInfo->szNick) == 1 && pClientConnectInfo->szNick[0] == '\n'))
     {
         strcpy_s(pClientConnectInfo->szNick, 12, "Default");
     }


     if(pClientConnectInfo->szNick[strlen(pClientConnectInfo->szNick)-1] == '\n')
     {
        pClientConnectInfo->szNick[strlen(pClientConnectInfo->szNick)-1] = 0;
     }
     
     pClientConnectInfo->usPort   = htons(4000);

     printf("Enter Host IP Address like: 127.0.0.1\n");
     fgets(szHostName, 100, stdin);
    
     pClientConnectInfo->ulAddress = inet_addr(szHostName);

     return bGotUserInfo;
 }


/**********************************************************************
 * 
 *  Client_ConnectToHost
 *
 *    Connect to a remote host
 *
 **********************************************************************/
 HANDLE Client_ConnectToHost(PCLIENT_CONNECT_INFO pClientConnectInfo)
 {
     HANDLE hClientLightBulb = INVALID_SOCKET;
     BOOL bConnected;
     NETWORK_ASYNC_WRITE NetAsyncWrite = {0};


     
     hClientLightBulb = Network_CreateLightBulb();

     if(hClientLightBulb != INVALID_HANDLE_VALUE)
     {
        pClientConnectInfo->FirstAsyncRead.pszBuffer = pClientConnectInfo->szTempBuffer;

        Network_RecieveAsync(hClientLightBulb, pClientConnectInfo->FirstAsyncRead.pszBuffer, sizeof(pClientConnectInfo->szTempBuffer) - 1, (LPOVERLAPPED)&pClientConnectInfo->FirstAsyncRead, Client_FirstReadCompletionRoutine);

        bConnected = Network_Connect(hClientLightBulb, pClientConnectInfo->ulAddress, pClientConnectInfo->usPort);

        if(bConnected == FALSE)
        {
            Client_DisplayErrorMsg(CLIENT_LIGHTBULB_CONNECT_ERROR);
            Network_CloseLightBulb(hClientLightBulb);
            hClientLightBulb = INVALID_HANDLE_VALUE;
        }
        else
        {
            Network_SendAsync(hClientLightBulb, pClientConnectInfo->szNick, strlen(pClientConnectInfo->szNick), (LPOVERLAPPED)&NetAsyncWrite, Client_WriteCompletionRoutine);

            do {
                SleepEx(25, TRUE);    
            } while(NetAsyncWrite.bWriteComplete == FALSE);
        }

     }
     else
     {
         Client_DisplayErrorMsg(CLIENT_LIGHTBULB_BULB_ERROR);
     }

     return hClientLightBulb;
 }

 /**********************************************************************
 * 
 *  Client_RunClient
 *
 *    Start running the client loop
 *
 **********************************************************************/
 void Client_RunClient(HANDLE hClientLightBulb, char *pszNick)
 {
    BOOL bDoClientLoop = TRUE;
    int iRetVal;
    char szBuffer[1001], szRecieveBuffer[5][300] = {0};
    NETWORK_ASYNC_WRITE NetAsyncWrite = {0};
    NETWORK_ASYNC_READ NetAsyncRead[5] = {0};
    DWORD dwBytesTransfered = 0;
    
    printf("Toby Opferman's Example Chat Client\n");
    printf("Enter a string to send or press ESC to exit\n");

    /*
     * Remember, we do no buffering in our driver.  However, we have enabled
     * asynchronous file operations which means we can make as many requests
     * as we want to the driver!  The actual joy of this is that we can now
     * make a ton of requests and hopefully not miss any data since we have
     * enough buffers in the waiting queue.  This helps to demonstrate how
     * to do asynchronous I/O.
     *
     */
    NetAsyncRead[0].pszBuffer = szRecieveBuffer[0];
    NetAsyncRead[0].hClientLightBulb = hClientLightBulb;
    NetAsyncRead[0].uiSize = sizeof(szRecieveBuffer[0]);

    NetAsyncRead[1].pszBuffer = szRecieveBuffer[1];
    NetAsyncRead[1].hClientLightBulb = hClientLightBulb;
    NetAsyncRead[1].uiSize = sizeof(szRecieveBuffer[1]);

    NetAsyncRead[2].pszBuffer = szRecieveBuffer[2];
    NetAsyncRead[2].hClientLightBulb = hClientLightBulb;
    NetAsyncRead[2].uiSize = sizeof(szRecieveBuffer[2]);

    NetAsyncRead[3].pszBuffer = szRecieveBuffer[3]; 
    NetAsyncRead[3].hClientLightBulb = hClientLightBulb;
    NetAsyncRead[3].uiSize = sizeof(szRecieveBuffer[3]);

    NetAsyncRead[4].pszBuffer = szRecieveBuffer[4];
    NetAsyncRead[4].hClientLightBulb = hClientLightBulb;
    NetAsyncRead[4].uiSize = sizeof(szRecieveBuffer[4]);

    Network_RecieveAsync(hClientLightBulb, szRecieveBuffer[0], sizeof(szRecieveBuffer[0]) - 1, (LPOVERLAPPED)&NetAsyncRead[0], Client_ReadCompletionRoutine);
    Network_RecieveAsync(hClientLightBulb, szRecieveBuffer[1], sizeof(szRecieveBuffer[1]) - 1, (LPOVERLAPPED)&NetAsyncRead[1], Client_ReadCompletionRoutine);
    Network_RecieveAsync(hClientLightBulb, szRecieveBuffer[2], sizeof(szRecieveBuffer[2]) - 1, (LPOVERLAPPED)&NetAsyncRead[2], Client_ReadCompletionRoutine);
    Network_RecieveAsync(hClientLightBulb, szRecieveBuffer[3], sizeof(szRecieveBuffer[3]) - 1, (LPOVERLAPPED)&NetAsyncRead[3], Client_ReadCompletionRoutine);
    Network_RecieveAsync(hClientLightBulb, szRecieveBuffer[4], sizeof(szRecieveBuffer[4]) - 1, (LPOVERLAPPED)&NetAsyncRead[4], Client_ReadCompletionRoutine);

    NetAsyncWrite.bWriteComplete = TRUE;

    while(bDoClientLoop)
    {
       
       SleepEx(25, TRUE);

       if(NetAsyncWrite.bWriteComplete)
       {
          if (_kbhit()) {
             int cChar = _getch();

             switch (cChar)
             {
             case 27:
                bDoClientLoop = FALSE;
                break;


             default:

                printf("Type Text (Max 256 Characters):\n");
                printf("%c", cChar);

                sprintf_s(szBuffer, 1001, "%s: %c", pszNick, cChar);
                fgets(szBuffer + strlen(szBuffer), 256, stdin);

                memset(&NetAsyncWrite, 0, sizeof(NetAsyncWrite));

                iRetVal = Network_SendAsync(hClientLightBulb, szBuffer, strlen(szBuffer), (LPOVERLAPPED)&NetAsyncWrite, Client_WriteCompletionRoutine);

                break;
             }
          }
       }

    }

    Network_CloseLightBulb(hClientLightBulb);

 }

 /**********************************************************************
 * 
 *  Client_FirstReadCompletionRoutine
 *
 *    First Async Read Routine Completed, Catch the "People Connected" data
 *    without having to buffer the data in the driver, keep the implementation
 *    simple.
 *
 **********************************************************************/
 VOID CALLBACK Client_FirstReadCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
 {
    PNETWORK_ASYNC_READ pNetworkAsyncRead = (PNETWORK_ASYNC_READ)lpOverlapped;

    printf(pNetworkAsyncRead->pszBuffer);
    printf("\n");
 }


 /**********************************************************************
 * 
 *  Client_ReadCompletionRoutine
 *
 *    Async Read Routine Completed
 *
 **********************************************************************/
 VOID CALLBACK Client_ReadCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
 {
    PNETWORK_ASYNC_READ pNetworkAsyncRead = (PNETWORK_ASYNC_READ)lpOverlapped;

    printf(pNetworkAsyncRead->pszBuffer);
    printf("\n");

    memset(lpOverlapped, 0, sizeof(OVERLAPPED));
    memset(pNetworkAsyncRead->pszBuffer, 0, pNetworkAsyncRead->uiSize);

    Network_RecieveAsync(pNetworkAsyncRead->hClientLightBulb, pNetworkAsyncRead->pszBuffer, pNetworkAsyncRead->uiSize - 1, (LPOVERLAPPED)pNetworkAsyncRead, Client_ReadCompletionRoutine);
 }

  /**********************************************************************
 * 
 *  Client_WriteCompletionRoutine
 *
 *    Async Write Routine Completed
 *
 **********************************************************************/
 VOID CALLBACK Client_WriteCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
 {
     PNETWORK_ASYNC_WRITE pNetworkAsyncWrite = (PNETWORK_ASYNC_WRITE)lpOverlapped;

     pNetworkAsyncWrite->bWriteComplete = TRUE;
 }

