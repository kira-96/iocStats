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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <epicsTime.h>
#include <devIocStats.h>

static unsigned int pid;
static epicsTimeStamp oldTime;
static double oldUsage;
static double scale;

static unsigned int getPid(const char *exe) {
  char cmd[80];
  sprintf(cmd, "pidof %s", exe);
  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    return 0;
  }
  char buf[12];
  if (fgets(buf, sizeof(buf), fp) == NULL) {
    return 0;
  }
  pclose(fp);
  return atoi(buf);
}

static double usageFromProc(unsigned int pid) {
  if (pid == 0) {
    return 0.;
  }

  static char statfile[32];
  sprintf(statfile, "/proc/%d/stat", pid);
  char sd[80];
  char cd;
  int id;
  unsigned int ud;
  long ld;
  long sticks = 0;
  long uticks = 0;
  FILE *fp;

  fp = fopen(statfile, "r");
  if (fp) {
    fscanf(fp, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu", &id, sd,
           &cd, &id, &id, &id, &id, &id, &ud, &ld, &ld, &ld, &ld, &uticks,
           &sticks);
    fclose(fp);
  }
  return (uticks + sticks) / (double)TICKS_PER_SEC;
}

int devScannerStatsInitCpuUtilization() {
  pid = getPid("scanner");
  epicsTimeGetCurrent(&oldTime);
  oldUsage = usageFromProc(pid);
  scale = 100.0f / NO_OF_CPUS;
  return 0;
}

int devScannerStatsGetCpuUtilization(double *pval) {
  epicsTimeStamp curTime;
  double curUsage;
  double elapsed;
  double cpuFract;

  epicsTimeGetCurrent(&curTime);
  curUsage = usageFromProc(pid);
  elapsed = epicsTimeDiffInSeconds(&curTime, &oldTime);

  cpuFract = (elapsed > 0) ? (curUsage - oldUsage) * scale / elapsed : 0.0;

  oldTime = curTime;
  oldUsage = curUsage;

  *pval = cpuFract;
  return 0;
}
