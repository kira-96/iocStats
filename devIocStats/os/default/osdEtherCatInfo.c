/*************************************************************************\
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* osdEtherCatInfo.c - EtherCAT info: default implementation = do nothing */

/*
 *  Author: Yang Zhenghan
 *
 *  Modification History
 *
 */

#include <devIocStats.h>

static char *notavail = "<not available>";

int devIocStatsInitEtherCatInfo() {
  return 0;
}

int devIocStatsGetEtherCatLoaded(char **pval) {
  *pval = notavail;
  return -1;
}

int devIocStatsGetEtherCatVersion(char **pval) {
  *pval = notavail;
  return -1;
}
