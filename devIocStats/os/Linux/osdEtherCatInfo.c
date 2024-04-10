/*************************************************************************\
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* osdEtherCatInfo.c - EtherCAT info: linux implementation = read */
/* /proc/devices */
/* ethercat version */

/*
 *  Author: Yang Zhenghan
 *
 *  Modification History
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <devIocStats.h>

static char *notavail = "<not available>";
static char *yes = "YES";
static char *no = "NO";
static char ethercatver[128];

int devIocStatsInitEtherCatInfo() {
  return 0;
}

int devIocStatsGetEtherCatLoaded(char **pval) {
  static char *devicesfile = "/proc/devices";
  char device[256];
  
  *pval = no;  // default NO

  FILE *fp = fopen(devicesfile, "r");
  if (fp == NULL) {
    return -1;
  }

  while (fgets(device, sizeof(device), fp)) {
    if (strstr(device, "EtherCAT")) {
      *pval = yes;
      break;
    }
  }

  fclose(fp);

  return 0;
}

int devIocStatsGetEtherCatVersion(char **pval) {
  static char *cmd = "ethercat version";
  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    *pval = notavail;
    return -1;
  }

  if (fgets(ethercatver, sizeof(ethercatver), fp) && 
      strstr(ethercatver, "IgH EtherCAT master")) {
    /* remove '\n' at the end of string. */
    int index = strlen(ethercatver) - 1;
    while (index >= 0 && ethercatver[index] == '\n') {
      ethercatver[index--] = '\0';
    }
    *pval = ethercatver;
  } else {
    *pval = notavail;
  }

  pclose(fp);

  return 0;
}
