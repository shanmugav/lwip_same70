/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include "app_commands.h"
#include "lwip_port/ethernetif.h"
#include "lwip_port/debug_log.h"
#include "lwip/dhcp.h"
#include <string.h>
#include <errno.h>

/* PHY MDIO read function from ethernetif.c */
extern uint16_t mdio_read(uint8_t phy_addr, uint8_t reg_addr);

/* GMAC debug counters from ethernetif.c */
extern volatile uint32_t dbg_isr_count;
extern volatile uint32_t dbg_isr_rcomp;
extern volatile uint32_t dbg_isr_other;
extern volatile uint32_t dbg_tx_count;
extern volatile uint32_t dbg_tx_bytes;
extern volatile uint32_t dbg_rx_count;
extern volatile uint32_t dbg_rx_bytes;
extern volatile uint32_t dbg_rx_none;

static uint32_t diag_tick = 0;

int32_t _APP_ParseUrl(char *uri, char **host, char **path, uint16_t * port);

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
 */

APP_DATA appData ={
    .state = APP_WAITING_FOR_INITIALIZATION
};
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback functions.
 */


// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary local functions.
 */


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void) {
    /* Place the App state machine in its initial state. */
    appData.state = APP_WAITING_FOR_INITIALIZATION;

    /* TODO: Initialize your application's state machine and other
     * parameters.
     */
#if 1 /* APP command enabled */
    APP_Commands_Init();
#endif
}

/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks(void) {
    struct hostent * hostInfo;
    int i;
    /* Check the application's current state. */
    switch (appData.state) {
        case APP_WAITING_FOR_INITIALIZATION:
        {
              //SYS_CONSOLE_MESSAGE("Network interface link is up\r\n");
             //break;
             #if 1
            /* Wait for lwIP netif link to come up */
            if (netif_is_link_up(&g_netif)) {
                SYS_CONSOLE_MESSAGE("Network interface link is up\r\n");
                appData.state = APP_TCPIP_WAIT_FOR_IP;
            }
            else
               SYS_CONSOLE_MESSAGE("Network interface link is Down\r\n");
                #endif
            break;
        }
        case APP_TCPIP_WAIT_FOR_IP:
        {
            /* Check if we got a real IP (DHCP or static) */
            if (g_netif.ip_addr.addr != 0) {
                uint8_t *ip = (uint8_t *)&g_netif.ip_addr.addr;
                SYS_CONSOLE_PRINT("IP Address: %d.%d.%d.%d\r\n",
                                  ip[0], ip[1], ip[2], ip[3]);
                appData.state = APP_TCPIP_WAITING_FOR_COMMAND_READY;
            }

            /* Yield to other tasks (tcpip_thread, GMAC RX task) */
            vTaskDelay(20 / portTICK_PERIOD_MS);
            break;
        }
        case APP_TCPIP_WAITING_FOR_COMMAND_READY:
        {
            appData.state = APP_TCPIP_WAITING_FOR_COMMAND_INPUT;
        }
            break;
        case APP_TCPIP_WAITING_FOR_COMMAND_INPUT:
        {
#if 1 /* APP command enabled */
            if (APP_URL_Buffer[0] != '\0') {
                if (_APP_ParseUrl(APP_URL_Buffer, &appData.host, &appData.path, &appData.port)) {
                    SYS_CONSOLE_PRINT("Could not parse URL '%s'\r\n", APP_URL_Buffer);
                    APP_URL_Buffer[0] = '\0';
                    break;
                }
                appData.state = APP_DNS_START_RESOLUTION;
            }
#endif
            /* Yield to other tasks */
            vTaskDelay(20 / portTICK_PERIOD_MS);

        }
            break;

        case APP_DNS_START_RESOLUTION:
        {
            dbg_puts("[APP] DNS: resolving host...\n");
            hostInfo = gethostbyname(appData.host);
            if (hostInfo != NULL) {
                appData.addr.sin_addr.s_addr = *(uint32_t *)(hostInfo->h_addr);
                uint8_t *rip = (uint8_t *)&appData.addr.sin_addr.s_addr;
                DBG_LOG_VAL("[APP] DNS resolved to:", appData.addr.sin_addr.s_addr);
                appData.state = APP_BSD_START;
            } else {
                dbg_puts("[APP] DNS resolution FAILED\n");
                appData.state = APP_TCPIP_WAITING_FOR_COMMAND_INPUT;
                APP_URL_Buffer[0] = '\0';
                break;
            }
        }

        case APP_BSD_START:
        {
            int tcpSkt;
            dbg_puts("[APP] Creating TCP socket...\n");
            if ((tcpSkt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                dbg_puts("[APP] socket() FAILED\n");
                return;
            }
            appData.socket = tcpSkt;
            DBG_LOG_VAL("[APP] Socket created, fd:", tcpSkt);
            appData.state = APP_BSD_CONNECT;
        }
            break;

        case APP_BSD_CONNECT:
        {
            int addrlen;
            int ret;
            appData.addr.sin_family = AF_INET;
            appData.addr.sin_port = htons(appData.port);
            addrlen = sizeof (struct sockaddr);
            DBG_LOG_VAL("[APP] Connecting to port:", appData.port);
            ret = connect(appData.socket, (struct sockaddr*) &appData.addr, addrlen);
            DBG_LOG_VAL("[APP] connect() returned:", ret);
            if (ret < 0) {
                DBG_LOG_VAL("[APP] connect errno:", errno);
                return;
            }
            dbg_puts("[APP] Connected OK!\n");
            appData.state = APP_BSD_SEND;
        }
            break;
        case APP_BSD_SEND:
        {
            char buffer[MAX_URL_SIZE];
            int sendLen, sent;
            sprintf(buffer, "GET /%s HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "Connection: close\r\n\r\n", appData.path, appData.host);
            sendLen = strlen((char*) buffer);
            dbg_puts("[APP] Sending HTTP GET...\n");
            DBG_LOG_VAL("[APP] Send length:", sendLen);
            sent = send(appData.socket, (const char*) buffer, sendLen, 0);
            DBG_LOG_VAL("[APP] send() returned:", sent);
            if (sent < 0) {
                DBG_LOG_VAL("[APP] send errno:", errno);
            }
            dbg_puts("[APP] Waiting for response (recv)...\n");
            appData.state = APP_BSD_OPERATION;
        }
            break;

        case APP_BSD_OPERATION:
        {
            char recvBuffer[80];
            dbg_puts("[APP] Calling recv()...\n");
            i = recv(appData.socket, recvBuffer, sizeof (recvBuffer) - 1, 0);
            DBG_LOG_VAL("[APP] recv() returned:", i);

            if (i <= 0)
            {
                if (i == 0) {
                    dbg_puts("[APP] recv: connection closed by server\n");
                    appData.state = APP_BSD_CLOSE;
                } else {
                    DBG_LOG_VAL("[APP] recv error, errno:", errno);
                    if (errno != EWOULDBLOCK) {
                        appData.state = APP_BSD_CLOSE;
                    }
                }
                break;
            }

            recvBuffer[i] = '\0';
            dbg_puts("[APP] --- Received data ---\n");
            dbg_puts(recvBuffer);
            dbg_puts("\n[APP] --- End data ---\n");

            if (appData.state == APP_BSD_OPERATION)
                break;
        }
            break;

        case APP_BSD_CLOSE:
            dbg_puts("[APP] Closing socket...\n");
            closesocket(appData.socket);
            dbg_puts("[APP] Connection Closed\n");
            appData.state = APP_TCPIP_WAITING_FOR_COMMAND_INPUT;

#if 1 /* APP command enabled */
            APP_URL_Buffer[0] = '\0';
#endif
            // No break needed
        default:
            return;
    }
}

int32_t _APP_ParseUrl(char *uri, char **host, char **path, uint16_t * port) {
    char * pos;
    pos = strstr(uri, "//"); //Check to see if its a proper URL
    *port = 80;


    if (!pos) {
        return -1;
    }
    *host = pos + 2; // This is where the host should start

    pos = strchr(* host, ':');

    if (!pos) {
        pos = strchr(*host, '/');
        if (!pos) {
            *path = NULL;
        } else {
            *pos = '\0';
            *path = pos + 1;
        }
    } else {
        *pos = '\0';
        char * portc = pos + 1;

        pos = strchr(portc, '/');
        if (!pos) {
            *path = NULL;
        } else {
            *pos = '\0';
            *path = pos + 1;
        }
        *port = atoi(portc);
    }
    return 0;
}

/* _APP_PumpDNS removed - lwIP gethostbyname() is synchronous */
/*******************************************************************************
 End of File
 */

