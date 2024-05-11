/*************************************************************************\
* Copyright (c) 2024 Shanghai Institute of Applied Physics
*     Chinese Academy of Sciences.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* devIocStatsAsub.c - Array Subroutine Routines for IOC statistics */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <epicsThread.h>
#include <epicsExport.h>
#include <epicsPrint.h>

#include <dbScan.h>
#include <aSubRecord.h>
#include <registryFunction.h>

#include "devIocStats.h"

#define DEV_TYPE 1
#define BUF_LEN 32
#define UDP_PORT 37427

#define REVISION 0x0100

static const uint32_t LO = 0x0100007F;
static const char * SEARCH = "://SINAP/EPICS/DEVICE-SEARCH";
static const char * HEADER = "://SINAP/EPICS/DEVICE/";

struct device_info {
    char header[23];
    unsigned char dev_type;
    unsigned short rev;
    unsigned short port;
    struct in_addr addr;
    char ioc[32];
};

static struct device_info info;  /* 设备信息 */
static epicsMutexId mutex; /* protect usage against concurrent access */

#ifndef _WIN32
/*************************************************************************\
* 通过掩码计算的方式，获取最匹配的本机IP地址。
\*************************************************************************/
static uint32_t get_match_ip(int sockfd, uint32_t addr) {
    if (sockfd <= 0) return LO;

    int n, i;
    struct ifreq req[INET_ADDRSTRLEN];
    struct ifconf ifc;

    ifc.ifc_len = sizeof(req);
    ifc.ifc_buf = (caddr_t)req;
    if (0 == ioctl(sockfd, SIOCGIFCONF, (char*)&ifc)) {
        n = ifc.ifc_len / sizeof(struct ifreq);
        i = n;
        while (i-- > 0) {
            if (0 == ioctl(sockfd, SIOCGIFADDR, (char*)&req[i])) {
                /* printf("%u \n", ntohl(((struct sockaddr_in*)&req[i].ifr_addr)->sin_addr.s_addr)); */
            }
        }

        /* 找出相近IP */
        int j;
        uint32_t mask = 0xFFFFFFFF;
        uint32_t temp = LO;

        for (j = 3; j >= 0; j--) {
            mask &= (~(0xFF << (j * 8)));
            for (i = 0; i < n; i++) {
                if ((((struct sockaddr_in*)&req[i].ifr_addr)->sin_addr.s_addr & mask) == (addr & mask)) {
                    temp = ((struct sockaddr_in*)&req[i].ifr_addr)->sin_addr.s_addr;
                    break;
                }
            }
            if (temp != LO) break;
        }

        return temp;
    }

    return LO;
}
#endif

/*************************************************************************\
* 监听设备搜寻命令
* 需要使用独立线程运行，否则会阻塞整个IOC运行。
\*************************************************************************/
static void searchMonTask(void *parm) {
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData))
    return;
#endif

#ifdef _WIN32
  SOCKET sockfd;
#else
  int sockfd;
#endif

  sockfd = socket(PF_INET, SOCK_DGRAM, 0);

  if (sockfd < 0) return;

  char bConditionalAccept = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &bConditionalAccept, sizeof(char));

  struct sockaddr_in localAddr;
  memset(&localAddr, 0, sizeof(localAddr));
  localAddr.sin_family = AF_INET;  /* IPv4 */
  localAddr.sin_port = htons(UDP_PORT);
  localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sockfd, (const struct sockaddr*)&localAddr, sizeof(localAddr)))
    return;
  else {
    printf("\nListening for broadcast at %s:%d ... \n", inet_ntoa(localAddr.sin_addr), ntohs(localAddr.sin_port));
  }

  int size;
  char buffer[BUF_LEN];  /* 接收缓存 */
  struct sockaddr_in from;  /* 消息发送地址 */
#ifdef _WIN32
  int sockaddr_size = sizeof(from);
#else
  socklen_t sockaddr_size = sizeof(from);
#endif

  while (TRUE) {
    memset(&from, 0, sizeof(from));
    memset(buffer, 0, sizeof(char) * BUF_LEN);
    /* 阻塞方式接受广播消息 */
    size = recvfrom(sockfd, buffer, BUF_LEN, 0, (struct sockaddr*)&from, &sockaddr_size);

    if (0 == strcmp(buffer, SEARCH)) {
#ifdef _WIN32
      /* 暂不支持获取Windows系统IP地址 */
      uint32_t ip = LO;
#else
      uint32_t ip = get_match_ip(sockfd, from.sin_addr.s_addr);  /* 获取最相近的IP */
#endif
      epicsMutexLock(mutex);
      info.addr.s_addr = ip;
      epicsMutexUnlock(mutex);

      size = sendto(sockfd, (const char*)&info, sizeof(info), 0, (struct sockaddr*)&from, sizeof(from));
      if (size > 0) {
        printf("\nSent device info to %s:%d: %d bytes. \n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), size);
      }
    }
  }

#ifdef _WIN32
  closesocket(sockfd);
  WSACleanup();
#else
  close(sockfd);
#endif
}

/*************************************************************************\
* Record 初始化函数
* 初始化设备信息
* 启动设备搜寻监听服务
\*************************************************************************/
static long searchMonInit(struct aSubRecord *psub) {
  psub->val = 1;
  
  mutex = epicsMutexMustCreate();
  
  memset(&info, 0, sizeof(info));
  strcpy(info.header, HEADER);
  info.dev_type = DEV_TYPE;  /* 设备类型 */
  info.rev = REVISION;    /* 修订版本 */

  /* 从 record name 获取IOC名称 */
  char name[61];
  strcpy(name, psub->name);
  char *p = name;
  
  while (TRUE) {
    char *temp = strchr(p + 1, ':');
    if (temp != NULL) {
      p = temp;
    } else {
      *p = '\0';
      break;
    }
  }

  strcpy(info.ioc, name);

  epicsThreadMustCreate("searchMonTask", epicsThreadPriorityMin,
                        epicsThreadGetStackSize(epicsThreadStackMedium),
                        searchMonTask, NULL);

  return 0;
}

/*************************************************************************\
* Record 更新函数
* 用于更新设备信息
* 目前只更新CA端口信息
\*************************************************************************/
static long searchMon(struct aSubRecord *psub) {
  epicsOldString *a = (epicsOldString*)(psub->a);
  epicsMutexLock(mutex);
  info.port = atoi(a[0]);
  epicsMutexUnlock(mutex);

  return 0;
}

epicsRegisterFunction(searchMonInit);
epicsRegisterFunction(searchMon);
