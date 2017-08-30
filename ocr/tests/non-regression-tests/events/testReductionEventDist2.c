/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Collective event - all add then all satisfy
 */

// Nb of max generations alive simultaneously
#define TEST_MAXGEN 1

// Nb of reductions per PD
#define TEST_NBCONTRIBSPD 2

#define TEST_IT_MAX 10

#include "testReductionEventBasicAddSat.ctpl"
