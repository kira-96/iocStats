/*************************************************************************\
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* osdScannerCpuUsage.c - CPU utilization info: linux implementation = read
 * /proc/pid/stat */

/*
 *  Author: Yang Zhenghan
 *
 *  Modification History
 *
 */

#include <devIocStats.h>

int devScannerStatsInitCpuUtilization() {
  return 0;
}

int devScannerStatsGetCpuUtilization(double *pval) {
  return -1;
}
