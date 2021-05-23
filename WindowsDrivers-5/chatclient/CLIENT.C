/**********************************************************************
 * 
 *  Toby Opferman
 *
 * Chat Client using Winsock
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/

/* Header Files */
#include <windows.h>
#include <winsock.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#pragma comment(lib, "WS2_32.LIB")

/**********************************************************************
 * Internal Data Structures
 **********************************************************************/
 typedef struct _client_connect_info
 {
     char szNick[12];
     SOCKADDR_IN SockHostAddress;

 } CLIENT_CONNECT_INFO, *PCLIENT_CONNECT_INFO;

 typedef enum
 {
   
   CLIENT_WINSOCK_INIT_ERROR = 1,
   CLIENT_WINSOCK_SOCKET_ERROR,
   CLIENT_WINSOCK_CONNECT_ERROR,
   CLIENT_WINSOCK_SEND_ERROR,
   CLIENT_WINSOCK_SERVER_CLOSED_ERROR

 } CLIENT_ERROR;

/**********************************************************************
 * Internal Prototypes
 **********************************************************************/
 BOOL Client_InitializeWinsock(void);
 void Client_FreeWinsock(void);
 BOOL Client_GetHostInfoFromUser(PCLIENT_CONNECT_INFO pClientConnectInfo);
 SOCKET Client_ConnectToHost(PCLIENT_CONNECT_INFO pClientConnectInfo);
 void Client_RunClient(SOCKET hClientSocket, char *pszNick);
 void Client_DisplayErrorMsg(CLIENT_ERROR uiMsg);

 
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

     if(Client_InitializeWinsock())
     {
         SOCKET hConnectSocket;

         if(Client_GetHostInfoFromUser(&ClientConnectInfo))
         {
             hConnectSocket = Client_ConnectToHost(&ClientConnectInfo);

             if(hConnectSocket != INVALID_SOCKET)
             {
                Client_RunClient(hConnectSocket, ClientConnectInfo.szNick);
             }
         }

         Client_FreeWinsock();
     }

     return 0;
 }

/**********************************************************************
 * 
 *  Client_InitializeWinsock
 *
 *    Initialize Winsock
 *
 **********************************************************************/
 BOOL Client_InitializeWinsock(void)
 {
     BOOL bInitialized = FALSE;
     WSADATA WsaData;

     if(WSAStartup(MAKEWORD(1, 1), &WsaData) != 0)
     {
        Client_DisplayErrorMsg(CLIENT_WINSOCK_INIT_ERROR);
     }
     else
     {
         bInitialized = TRUE;
     }

     return bInitialized;
 }


/**********************************************************************
 * 
 *  Client_FreeWinsock
 *
 *    Free Winsock
 *
 **********************************************************************/
 void Client_FreeWinsock(void)
 {
     WSACleanup();
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
         case CLIENT_WINSOCK_INIT_ERROR:
              printf("Chat Client - Cannot Initialize Winsock!\n");
              break;

         case CLIENT_WINSOCK_SOCKET_ERROR:
              printf("Chat Client - Cannot Create Socket!\n");
              break;

         case CLIENT_WINSOCK_CONNECT_ERROR:
              printf("Chat Server - Cannot Connect to Remote Server!\n");
              break;

         case CLIENT_WINSOCK_SEND_ERROR:
              printf("Chat Server - Socket Error Data Not Sent!\n");
              break;

         case CLIENT_WINSOCK_SERVER_CLOSED_ERROR:
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

     if(pClientConnectInfo->szNick[strlen(pClientConnectInfo->szNick)-1] == '\n')
     {
        pClientConnectInfo->szNick[strlen(pClientConnectInfo->szNick)-1] = 0;
     }
     
     pClientConnectInfo->SockHostAddress.sin_family = PF_INET;
     pClientConnectInfo->SockHostAddress.sin_port   = htons(4000);

     printf("Enter Host IP Address like: 127.0.0.1\n");
     fgets(szHostName, 100, stdin);
    
     pClientConnectInfo->SockHostAddress.sin_addr.s_addr = inet_addr(szHostName);

     return bGotUserInfo;
 }


/**********************************************************************
 * 
 *  Client_ConnectToHost
 *
 *    Connect to a remote host
 *
 **********************************************************************/
 SOCKET Client_ConnectToHost(PCLIENT_CONNECT_INFO pClientConnectInfo)
 {
     SOCKET hClientSocket = INVALID_SOCKET;
     int iRetVal;

     hClientSocket = socket(PF_INET, SOCK_STREAM, 0);

     if(hClientSocket != INVALID_SOCKET)
     {
        iRetVal = connect(hClientSocket, (LPSOCKADDR)&pClientConnectInfo->SockHostAddress, sizeof(SOCKADDR_IN));

        if(iRetVal == INVALID_SOCKET)
        {
            Client_DisplayErrorMsg(CLIENT_WINSOCK_CONNECT_ERROR);
            closesocket(hClientSocket);
            hClientSocket = INVALID_SOCKET;
        }
        else
        {
            send(hClientSocket, pClientConnectInfo->szNick, strlen(pClientConnectInfo->szNick), 0);
        }
     }
     else
     {
         Client_DisplayErrorMsg(CLIENT_WINSOCK_SOCKET_ERROR);
     }

     return hClientSocket;
 }

 /**********************************************************************
 * 
 *  Client_RunClient
 *
 *    Start running the client loop
 *
 **********************************************************************/
 void Client_RunClient(SOCKET hClientSocket, char *pszNick)
 {
    BOOL bDoClientLoop = TRUE;
    fd_set stReadFDS;
    struct timeval stTimeout;
    int iRetVal;
    char szBuffer[1001];

    printf("Toby Opferman's Example Chat Client\n");
    printf("Enter a string to send or press ESC to exit\n");

    while(bDoClientLoop)
    {
       FD_ZERO(&stReadFDS);
       FD_SET(hClientSocket, &stReadFDS);

       stTimeout.tv_sec = 0;
       stTimeout.tv_usec = 1;
       
       iRetVal = select(0, &stReadFDS, NULL, NULL, &stTimeout);

       if(iRetVal == 0 || iRetVal == SOCKET_ERROR)
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

                iRetVal = send(hClientSocket, szBuffer, strlen(szBuffer), 0);

                if (iRetVal == SOCKET_ERROR)
                {
                   Client_DisplayErrorMsg(CLIENT_WINSOCK_SEND_ERROR);
                }
                break;
             }
          }
       }
       else
       {

           if(FD_ISSET(hClientSocket, &stReadFDS))
           {
              iRetVal = recv(hClientSocket, szBuffer, 1000, 0);

              if(iRetVal == 0 || iRetVal == SOCKET_ERROR)
              {
                 Client_DisplayErrorMsg(CLIENT_WINSOCK_SERVER_CLOSED_ERROR);
                 bDoClientLoop = FALSE;
              }
              else
              {
                 szBuffer[iRetVal] = 0;
                 printf("%s\n", szBuffer);
              }
           }
       }

    }

    closesocket(hClientSocket);
 }




       



