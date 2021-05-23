/**********************************************************************
 * 
 *  Toby Opferman
 *
 * Chat Server using Winsock
 *
 *  This example is for educational purposes only.  I license this source
 *  out for use in learning how to write a device driver.
 *
 *  Copyright (c) 2005, All Rights Reserved  
 **********************************************************************/



#include <windows.h>
#include <winsock.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#pragma comment(lib, "WS2_32.LIB")


/**********************************************************************
 * Internal Data Structures
 **********************************************************************/
 typedef struct _client_info
  {
     SOCKET hSocket;
     SOCKADDR_IN SockAddr;
     char Nick[12];
     BOOL bSetNick;
     struct _client_info *Next;

 } CLIENT_INFO, *PCLIENT_INFO;

 typedef struct _server_info
 {
    SOCKET hServerSocket;
    PCLIENT_INFO pClientInfoList;
    fd_set stReadFDS, stXcptFDS;
    int DataSockets;
    BOOL bServerIsRunning;

 } SERVER_INFO, *PSERVER_INFO;
 
 typedef enum 
 {
     SERV_WINSOCK_INIT_ERROR = 1,
     SERV_WINSOCK_SOCKET_ERROR,
     SERV_WINSOCK_BIND_ERROR,
     SERV_WINSOCK_LISTEN_ERROR,
     SERV_WINSOCK_LOCALIP_ERROR,
     SERV_MEMORY_ERROR

 } SERV_ERROR;

/**********************************************************************
 * Internal Prototypes
 **********************************************************************/
 BOOL Serv_InitializeWinsock(void);
 SOCKET Serv_CreateServer(void);
 void Serv_RunServer(SOCKET hServerSocket);
 void Serv_DisplayErrorMsg(SERV_ERROR uiMsg);
 void Serv_FreeWinsock(void);
 BOOL Serv_DisplayLocalIp(SOCKADDR_IN *SockAddr);
 void Serv_ProcessIncommingSockets(PSERVER_INFO pServerInfo);
 void Serv_ProcessChatBroadcast(PSERVER_INFO pServerInfo);
 BOOL Serv_ProcessSocketWait(PSERVER_INFO pServerInfo);
 void Serv_SendNewClientBroadCast(SOCKET hClientSocket, PSERVER_INFO pServerInfo);
 PCLIENT_INFO Serv_CreateNewClient(SOCKET hNewClientSocket, SOCKADDR_IN *pNewClientSockAddr);
 void Serv_SendClientBroadCast(PCLIENT_INFO pClientInfo, PSERVER_INFO pServerInfo, char *pszBuffer);
 void Serv_FreeClientList(PSERVER_INFO pServerInfo);

/**********************************************************************
 * 
 *  main
 *
 *    Console App Entry Function
 *
 **********************************************************************/
 int __cdecl main(void)
 {
     if(Serv_InitializeWinsock())
     {
         SOCKET hServSocket;

         hServSocket = Serv_CreateServer();

         if(hServSocket != INVALID_SOCKET)
         {
             Serv_RunServer(hServSocket);
         }

         Serv_FreeWinsock();
     }

     return 0;
 }

/**********************************************************************
 * 
 *  Serv_InitializeWinsock
 *
 *    Initialize Winsock
 *
 **********************************************************************/
 BOOL Serv_InitializeWinsock(void)
 {
     BOOL bInitialized = FALSE;
     WSADATA WsaData;

     if(WSAStartup(MAKEWORD(1, 1), &WsaData) != 0)
     {
        Serv_DisplayErrorMsg(SERV_WINSOCK_INIT_ERROR);
     }
     else
     {
         bInitialized = TRUE;
     }

     return bInitialized;
 }


/**********************************************************************
 * 
 *  Serv_FreeWinsock
 *
 *    Free Winsock
 *
 **********************************************************************/
 void Serv_FreeWinsock(void)
 {
     WSACleanup();
 }


/**********************************************************************
 * 
 *  Serv_DisplayErrorMsg
 *
 *    Displays Error Messages
 *
 **********************************************************************/ 
 void Serv_DisplayErrorMsg(SERV_ERROR uiMsg)
 {
     switch(uiMsg)
     {
         case SERV_WINSOCK_INIT_ERROR:
              printf("Chat Server - Cannot Initialize Winsock!\n");
              break;

         case SERV_WINSOCK_SOCKET_ERROR:
              printf("Chat Server - Cannot Create Socket!\n");
              break;

         case SERV_WINSOCK_BIND_ERROR:
              printf("Chat Server - Cannot Bind Socket to Port!\n");
              break;

         case SERV_WINSOCK_LISTEN_ERROR:
              printf("Chat Server - Cannot Listen on Port!\n");
              break;
     
         case SERV_WINSOCK_LOCALIP_ERROR:
              printf("Chat Server - Cannot Find Local IP Address!\n");
              break;

         case SERV_MEMORY_ERROR:
              printf("Chat Server - Out of Memory!\n");
              break;
              
         default:
              printf("Chat Server - Runtime Error!\n");

     }
 }


/**********************************************************************
 * 
 *  Serv_CreateServer
 *
 *    Creates the Listener
 *
 **********************************************************************/
 SOCKET Serv_CreateServer(void)
 {
    SOCKET hSocket = INVALID_SOCKET;
    SOCKADDR_IN SockAddr = {0};
    UINT uiErrorStatus;

    /*
     * Create a socket
     */
    hSocket = socket(PF_INET, SOCK_STREAM, 0);

    if(hSocket != INVALID_SOCKET)
    {

    
        SockAddr.sin_family = PF_INET;
        SockAddr.sin_port = htons(4000);

        /*
         * BIND the Socket to a Port
         */
        uiErrorStatus = bind(hSocket, (struct sockaddr *)&SockAddr, sizeof(SOCKADDR_IN));
         
        if(uiErrorStatus != INVALID_SOCKET)
        {
            /*
             * Start Listening
             */

            if(listen(hSocket, 5) == 0)
            {
                /*
                 * Display Local IP Address
                 */
                if(Serv_DisplayLocalIp(&SockAddr) == FALSE)
                {
                      Serv_DisplayErrorMsg(SERV_WINSOCK_LOCALIP_ERROR);
                      closesocket(hSocket);
                      hSocket = INVALID_SOCKET;
                }

            }
            else
            {
                Serv_DisplayErrorMsg(SERV_WINSOCK_LISTEN_ERROR);
                closesocket(hSocket);
                hSocket = INVALID_SOCKET;
            }
        }
        else
        {
            Serv_DisplayErrorMsg(SERV_WINSOCK_BIND_ERROR);
            closesocket(hSocket);
            hSocket = INVALID_SOCKET;
        }
    }
    else
    {
        Serv_DisplayErrorMsg(SERV_WINSOCK_SOCKET_ERROR);
    }

    return hSocket;
 }


/**********************************************************************
 * 
 *  Serv_DisplayLocalIp
 *
 *    Displays a Local IP Address using a socket structure
 *
 **********************************************************************/
 BOOL Serv_DisplayLocalIp(SOCKADDR_IN *SockAddr)
 {
    BOOL bDisplayedLocalIp = FALSE;
    char HostName[101];
    LPHOSTENT Host;

    /*
     * If the Local IP Address is not in the SOCKADDR data structure, then
     * attempt to find it.
     */
    if(!SockAddr->sin_addr.s_addr)
    {
          if(gethostname(HostName, 100) != SOCKET_ERROR)
          {
              Host = gethostbyname((LPSTR)HostName);
                                 
              if(Host)
              {
                  SockAddr->sin_addr.s_addr = *((long *)Host->h_addr);
              }
                                           
              if(SockAddr->sin_addr.s_addr)
              {
                    printf("Server IP Address = %i.%i.%i.%i\n", SockAddr->sin_addr.s_addr&0xFF,
                                                                SockAddr->sin_addr.s_addr>>8 & 0xFF,
                                                                SockAddr->sin_addr.s_addr>>16 & 0xFF,
                                                                SockAddr->sin_addr.s_addr>>24 & 0xFF);

                    printf("Server Port Number = %i\n", htons(SockAddr->sin_port));

                    bDisplayedLocalIp = TRUE;
              }
 
          }
    }
    else
    {
        bDisplayedLocalIp = TRUE;
        printf("Server IP Address = %i.%i.%i.%i\n", SockAddr->sin_addr.s_addr&0xFF,
                                           SockAddr->sin_addr.s_addr>>8 & 0xFF,
                                           SockAddr->sin_addr.s_addr>>16 & 0xFF,
                                           SockAddr->sin_addr.s_addr>>24 & 0xFF);
        printf("Server Port Number = %i\n", htons(SockAddr->sin_port));                
    }

    return bDisplayedLocalIp;
 }



 /**********************************************************************
 * 
 *  Serv_RunServer
 *
 *    Runs the server
 *
 **********************************************************************/
 void Serv_RunServer(SOCKET hServerSocket)
 {
     SERVER_INFO ServerInfo = {0};

     printf("Toby Opferman's Simple Chat Broadcast Server Example is Running\n");
     printf("Type 'Q' to Quit\n");

     ServerInfo.hServerSocket    = hServerSocket;
     ServerInfo.bServerIsRunning = TRUE;

     while(ServerInfo.bServerIsRunning)
     {
          if(Serv_ProcessSocketWait(&ServerInfo))
          {
              Serv_ProcessIncommingSockets(&ServerInfo);
              Serv_ProcessChatBroadcast(&ServerInfo);
          }
     }

     Serv_FreeClientList(&ServerInfo);
     closesocket(hServerSocket);

 }

/**********************************************************************
 * 
 *  Serv_ProcessSocketWait
 *
 *    Process wait for socket communications
 *
 **********************************************************************/
 BOOL Serv_ProcessSocketWait(PSERVER_INFO pServerInfo)
 {
     BOOL bProcessSocketInformation = FALSE;
     BOOL bNoDataToProcess = TRUE;
     struct timeval stTimeout;
     PCLIENT_INFO pClientInfo;
     int RetVal;

     /*
      * Loop Until Client Sends Data, Server is Terminated or Another Client Connects
      */
     while(bNoDataToProcess)
     {
           FD_ZERO(&pServerInfo->stReadFDS);
           FD_ZERO(&pServerInfo->stXcptFDS);
           FD_SET(pServerInfo->hServerSocket, &pServerInfo->stReadFDS);
           
           pServerInfo->DataSockets = 0;

           pClientInfo = pServerInfo->pClientInfoList;
    
           while(pClientInfo)
           {
              FD_SET(pClientInfo->hSocket, &pServerInfo->stReadFDS);
              pClientInfo = pClientInfo->Next;
           }
           
           stTimeout.tv_sec = 1;
           stTimeout.tv_usec = 0;
           
           RetVal = select(0, &pServerInfo->stReadFDS, NULL, &pServerInfo->stXcptFDS, &stTimeout);
    
           if(RetVal == 0 || RetVal == SOCKET_ERROR)
           {
               if(_kbhit())
               {
                   switch(getch())
                   {
                       case 'q' :
                       case 'Q' :
                                 bNoDataToProcess              = FALSE;
                                 pServerInfo->bServerIsRunning = FALSE;
                                 break;
                   }
               }
           }
           else
           {
               bNoDataToProcess = FALSE;
               bProcessSocketInformation = TRUE;
               pServerInfo->DataSockets = RetVal;
           }
     }

     return bProcessSocketInformation;
 }


/**********************************************************************
 * 
 *  Serv_ProcessIncommingSockets
 *
 *    Accept new client connections
 *
 **********************************************************************/
 void Serv_ProcessIncommingSockets(PSERVER_INFO pServerInfo)
 {
   SOCKADDR_IN NewClientSockAddr = {0};
   SOCKET hNewClient;
   UINT uiLength;
   PCLIENT_INFO pClientInfo, pClientInfoWalker;

   /*
    * Check if the Server Socket has any new connecting clients
    */

   if(FD_ISSET(pServerInfo->hServerSocket, &pServerInfo->stReadFDS))
   {
        pServerInfo->DataSockets--;
        uiLength = sizeof(SOCKADDR_IN);
        
        if((hNewClient = accept(pServerInfo->hServerSocket, (struct sockaddr *)&NewClientSockAddr, &uiLength)) != INVALID_SOCKET)
        {
            /*
             * Send All Client Information to the new Client
             */
            Serv_SendNewClientBroadCast(hNewClient, pServerInfo);

            pClientInfo = Serv_CreateNewClient(hNewClient, &NewClientSockAddr);

            if(pClientInfo)
            {
                if(pServerInfo->pClientInfoList)
                {
                   pClientInfoWalker = pServerInfo->pClientInfoList;

                   while(pClientInfoWalker->Next)
                   {
                       pClientInfoWalker = pClientInfoWalker->Next;
                   }

                   pClientInfoWalker->Next = pClientInfo;
                }
                else
                {
                   pServerInfo->pClientInfoList = pClientInfo;
                }
            }
            else
            {
                Serv_DisplayErrorMsg(SERV_MEMORY_ERROR);
                closesocket(hNewClient);
            }
        }
   }
 }


/**********************************************************************
 * 
 *  Serv_CreateNewClient
 *
 *    Create a new client data structure
 *
 **********************************************************************/
 PCLIENT_INFO Serv_CreateNewClient(SOCKET hNewClientSocket, SOCKADDR_IN *pNewClientSockAddr)
 {
    PCLIENT_INFO pClientInfo;

    pClientInfo = (PCLIENT_INFO)LocalAlloc(LMEM_ZEROINIT, sizeof(CLIENT_INFO));

    if(pClientInfo)
    {
        pClientInfo->hSocket = hNewClientSocket;
        pClientInfo->SockAddr = *pNewClientSockAddr;

        printf("Connected IP = %i.%i.%i.%i\n", pClientInfo->SockAddr.sin_addr.s_addr&0xFF,
                                               pClientInfo->SockAddr.sin_addr.s_addr>>8 & 0xFF,
                                               pClientInfo->SockAddr.sin_addr.s_addr>>16 & 0xFF,
                                               pClientInfo->SockAddr.sin_addr.s_addr>>24 & 0xFF);
    }

    return pClientInfo;
 }


/**********************************************************************
 * 
 *  Serv_ProcessChatBroadcast
 *
 *    Broadcast Client Chat Messages
 *
 **********************************************************************/
 void Serv_ProcessChatBroadcast(PSERVER_INFO pServerInfo)
 {
    PCLIENT_INFO pClientInfo, pClientPrev = NULL;
    int RetVal;
    char szBuffer[1000];

    pClientInfo = pServerInfo->pClientInfoList;

    while(pServerInfo->DataSockets && pClientInfo)
    {
        if(FD_ISSET(pClientInfo->hSocket, &pServerInfo->stReadFDS))
        {
           pServerInfo->DataSockets--;
           
           /*
            * Get Incomming Data
            */
           RetVal = recv(pClientInfo->hSocket, szBuffer, sizeof(szBuffer), 0);

           /*
            * Client Error?
            */
           if(RetVal == 0 || RetVal == SOCKET_ERROR)
           {

               /*
                * If Client has a NickName, Broadcast he left the channel.
                */
               if(pClientInfo->bSetNick)
               {
                  sprintf_s(szBuffer, 1000, "***** %s Has Left the chat line\n", pClientInfo->Nick);
                  Serv_SendClientBroadCast(pClientInfo, pServerInfo, szBuffer);
               } 

               /*
                * Close Socket, remove client from linked list and free the memory.
                */
               closesocket(pClientInfo->hSocket);

               if(pClientPrev)
               {
                   pClientPrev->Next = pClientInfo->Next;
                   LocalFree(pClientInfo);
                   pClientInfo = pClientPrev->Next;
               }
               else
               {
                   pServerInfo->pClientInfoList = pClientInfo->Next;
                   LocalFree(pClientInfo);
                   pClientInfo = pServerInfo->pClientInfoList;
               }
               
           }
           else
           {
                
                szBuffer[RetVal] = 0;

                /*
                 * The First Packet from a client is it's nick name.
                 */
                if(!pClientInfo->bSetNick)
                {
                    

                    strcpy_s(pClientInfo->Nick, 12, szBuffer);
                    sprintf_s(szBuffer, 1000, "*** %s has joined the chat line\n", pClientInfo->Nick);
                    
                    pClientInfo->bSetNick = TRUE;

                }

                Serv_SendClientBroadCast(pClientInfo, pServerInfo, szBuffer);

                pClientPrev = pClientInfo;
                pClientInfo = pClientInfo->Next;
           }
        }
        else
        {
            pClientPrev = pClientInfo;
            pClientInfo = pClientInfo->Next;
        }
    }
 }

/**********************************************************************
 * 
 *  Serv_FreeClientList
 *
 *    Free All Clients
 *
 **********************************************************************/
 void Serv_FreeClientList(PSERVER_INFO pServerInfo)
 {
     PCLIENT_INFO pClientInfo = pServerInfo->pClientInfoList, pNextClient;

     while(pClientInfo)
     {
         pNextClient = pClientInfo->Next;
         
         closesocket(pClientInfo->hSocket);
         LocalFree(pClientInfo);

         pClientInfo = pNextClient;
     }

     pServerInfo->pClientInfoList = NULL;
 }

/**********************************************************************
 * 
 *  Serv_SendClientBroadCast
 *
 *    Send All Connected Client Information to All but one Client
 *
 **********************************************************************/ 
 void Serv_SendClientBroadCast(PCLIENT_INFO pClientInfo, PSERVER_INFO pServerInfo, char *pszBuffer)
 {
     PCLIENT_INFO pClientRunner;
     
     pClientRunner = pServerInfo->pClientInfoList;
     
     while(pClientRunner)
     {

         if(pClientRunner != pClientInfo)
         {
             send(pClientRunner->hSocket, pszBuffer, strlen(pszBuffer) + 1, 0);
         }

         pClientRunner = pClientRunner->Next;
     }

 }

/**********************************************************************
 * 
 *  Serv_SendNewClientBroadCast
 *
 *    Send All Connected Client Information to New Client
 *
 **********************************************************************/ 
 void Serv_SendNewClientBroadCast(SOCKET hClientSocket, PSERVER_INFO pServerInfo)
 {
     char szBuffer[5000] = {0};
     PCLIENT_INFO pClientInfo;

     sprintf_s(szBuffer, 5000, "People Connected: ");
     pClientInfo = pServerInfo->pClientInfoList;

     while(pClientInfo)
     {
         sprintf_s(szBuffer, 5000, "%s %s", szBuffer, pClientInfo->Nick);
         pClientInfo = pClientInfo->Next;
     }

     sprintf_s(szBuffer, 5000, "%s\n", szBuffer);
     send(hClientSocket, szBuffer, strlen(szBuffer), 0);
 }



       



