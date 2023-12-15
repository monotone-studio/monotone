#pragma once

//
// noire
//
// time-series storage
//

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <setjmp.h>
#include <assert.h>

#include <pthread.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
