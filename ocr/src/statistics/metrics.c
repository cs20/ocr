/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "statistics/metrics.h"
#include "debug.h"

#include "ocr-worker.h"
#ifdef ENABLE_WORKER_HC
#include "worker/hc/hc-worker.h"
#endif

#if !(defined(TG_XE_TARGET) || defined(TG_CE_TARGET) || defined(ENABLE_BUILDER_ONLY))
#include <math.h>
#endif

#define DEBUG_TYPE UTIL

#ifndef WORKER_EDT_METRIC_STORE_DEFAULT_COUNT
#define WORKER_EDT_METRIC_STORE_DEFAULT_COUNT 10
#endif

//Begin copy-paste to metrics.c from generateMacroMetricDefs.sh

// Metric name definitions

static char * EDT_MetricsRATIO_Names[EDT_METRIC_RATIO_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_RATIO_RT_USER
    EDT_METRIC_RATIO_RT_USER_NAME,
#endif
};

static char * EDT_MetricsCOUNTER_Names[EDT_METRIC_COUNTER_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE
    EDT_METRIC_COUNT_EDT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY
    EDT_METRIC_COUNT_EVT_SATISFY_NAME,
#endif
};

static char * EDT_MetricsDIFF_Names[EDT_METRIC_DIFF_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED
    EDT_METRIC_TIME_DEPV_ACQUIRED_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE
    EDT_METRIC_TIME_PROLOGUE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC
    EDT_METRIC_TIME_FUNC_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE
    EDT_METRIC_TIME_EPILOGUE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE
    EDT_METRIC_TIME_ACTION_EVT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY
    EDT_METRIC_TIME_ACTION_EVT_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY
    EDT_METRIC_TIME_ACTION_EVT_SATISFY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE
    EDT_METRIC_TIME_ACTION_TPL_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY
    EDT_METRIC_TIME_ACTION_TPL_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE
    EDT_METRIC_TIME_ACTION_EDT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY
    EDT_METRIC_TIME_ACTION_EDT_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP
    EDT_METRIC_TIME_ACTION_ADDDEP_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_USER
    EDT_METRIC_TIME_USER_NAME,
#endif
};

static char * EDT_MetricsDETAILED_Names[EDT_METRIC_DETAILED_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_DETAILED
    EDT_METRIC_TIME_DEPV_ACQUIRED_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE_DETAILED
    EDT_METRIC_TIME_PROLOGUE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC_DETAILED
    EDT_METRIC_TIME_FUNC_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE_DETAILED
    EDT_METRIC_TIME_EPILOGUE_NAME,
#endif
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE_DETAILED
    EDT_METRIC_COUNT_EDT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_DETAILED
    EDT_METRIC_COUNT_EVT_SATISFY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_SATISFY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_TPL_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_TPL_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_EDT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_EDT_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_DETAILED
    EDT_METRIC_TIME_ACTION_ADDDEP_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_USER_DETAILED
    EDT_METRIC_TIME_USER_NAME,
#endif
#if ENABLE_EDT_METRIC_RATIO_RT_USER_DETAILED
    EDT_METRIC_RATIO_RT_USER_NAME,
#endif
};

static char * EDT_MetricsAGG_EDT_WORKER_Names[EDT_METRIC_AGG_EDT_WORKER_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_AGG_EDT_WORKER
    EDT_METRIC_TIME_DEPV_ACQUIRED_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE_AGG_EDT_WORKER
    EDT_METRIC_TIME_PROLOGUE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC_AGG_EDT_WORKER
    EDT_METRIC_TIME_FUNC_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE_AGG_EDT_WORKER
    EDT_METRIC_TIME_EPILOGUE_NAME,
#endif
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_COUNT_EDT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_AGG_EDT_WORKER
    EDT_METRIC_COUNT_EVT_SATISFY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_SATISFY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_TPL_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_TPL_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EDT_CREATE_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EDT_DESTROY_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_ADDDEP_NAME,
#endif
#if ENABLE_EDT_METRIC_TIME_USER_AGG_EDT_WORKER
    EDT_METRIC_TIME_USER_NAME,
#endif
#if ENABLE_EDT_METRIC_RATIO_RT_USER_AGG_EDT_WORKER
    EDT_METRIC_RATIO_RT_USER_NAME,
#endif
};

static char * WORKER_MetricsDIFF_Names[WORKER_METRIC_DIFF_MAX] __attribute__((unused)) = {
#if ENABLE_WORKER_METRIC_TIME_GETWORK_NAK
    WORKER_METRIC_TIME_GETWORK_NAK_NAME,
#endif
#if ENABLE_WORKER_METRIC_TIME_GETWORK_ACK
    WORKER_METRIC_TIME_GETWORK_ACK_NAME,
#endif
#if ENABLE_WORKER_METRIC_TIME_EXEC_EDT
    WORKER_METRIC_TIME_EXEC_EDT_NAME,
#endif
};

//TODO: this must be auto-generated from the script. Quick fix for the XAS report.
static char * WORKER_MetricsDETAILED_Names[WORKER_METRIC_DETAILED_MAX] __attribute__((unused)) = {
#if ENABLE_WORKER_METRIC_TIME_GETWORK_NAK_DETAILED
    WORKER_METRIC_TIME_GETWORK_NAK_NAME,
#endif
#if ENABLE_WORKER_METRIC_TIME_GETWORK_ACK_DETAILED
    WORKER_METRIC_TIME_GETWORK_ACK_NAME,
#endif
};

static u8 EDT_MetricsRATIO_Types[EDT_METRIC_RATIO_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_RATIO_RT_USER
    EDT_METRIC_RATIO_RT_USER_TYPE,
#endif
};

static u8 EDT_MetricsCOUNTER_Types[EDT_METRIC_COUNTER_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE
    EDT_METRIC_COUNT_EDT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY
    EDT_METRIC_COUNT_EVT_SATISFY_TYPE,
#endif
};

static u8 EDT_MetricsDIFF_Types[EDT_METRIC_DIFF_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED
    EDT_METRIC_TIME_DEPV_ACQUIRED_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE
    EDT_METRIC_TIME_PROLOGUE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC
    EDT_METRIC_TIME_FUNC_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE
    EDT_METRIC_TIME_EPILOGUE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE
    EDT_METRIC_TIME_ACTION_EVT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY
    EDT_METRIC_TIME_ACTION_EVT_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY
    EDT_METRIC_TIME_ACTION_EVT_SATISFY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE
    EDT_METRIC_TIME_ACTION_TPL_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY
    EDT_METRIC_TIME_ACTION_TPL_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE
    EDT_METRIC_TIME_ACTION_EDT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY
    EDT_METRIC_TIME_ACTION_EDT_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP
    EDT_METRIC_TIME_ACTION_ADDDEP_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_USER
    EDT_METRIC_TIME_USER_TYPE,
#endif
};

static u8 EDT_MetricsDETAILED_Types[EDT_METRIC_DETAILED_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_DETAILED
    EDT_METRIC_TIME_DEPV_ACQUIRED_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE_DETAILED
    EDT_METRIC_TIME_PROLOGUE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC_DETAILED
    EDT_METRIC_TIME_FUNC_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE_DETAILED
    EDT_METRIC_TIME_EPILOGUE_TYPE,
#endif
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE_DETAILED
    EDT_METRIC_COUNT_EDT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_DETAILED
    EDT_METRIC_COUNT_EVT_SATISFY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_SATISFY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_TPL_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_TPL_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_EDT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_EDT_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_DETAILED
    EDT_METRIC_TIME_ACTION_ADDDEP_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_USER_DETAILED
    EDT_METRIC_TIME_USER_TYPE,
#endif
#if ENABLE_EDT_METRIC_RATIO_RT_USER_DETAILED
    EDT_METRIC_RATIO_RT_USER_TYPE,
#endif
};

static u8 EDT_MetricsAGG_EDT_WORKER_Types[EDT_METRIC_AGG_EDT_WORKER_MAX] __attribute__((unused)) = {
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_AGG_EDT_WORKER
    EDT_METRIC_TIME_DEPV_ACQUIRED_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE_AGG_EDT_WORKER
    EDT_METRIC_TIME_PROLOGUE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC_AGG_EDT_WORKER
    EDT_METRIC_TIME_FUNC_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE_AGG_EDT_WORKER
    EDT_METRIC_TIME_EPILOGUE_TYPE,
#endif
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_COUNT_EDT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_AGG_EDT_WORKER
    EDT_METRIC_COUNT_EVT_SATISFY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_SATISFY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_TPL_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_TPL_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EDT_CREATE_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EDT_DESTROY_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_ADDDEP_TYPE,
#endif
#if ENABLE_EDT_METRIC_TIME_USER_AGG_EDT_WORKER
    EDT_METRIC_TIME_USER_TYPE,
#endif
#if ENABLE_EDT_METRIC_RATIO_RT_USER_AGG_EDT_WORKER
    EDT_METRIC_RATIO_RT_USER_TYPE,
#endif
};

static u8 WORKER_MetricsDIFF_Types[WORKER_METRIC_DIFF_MAX] __attribute__((unused)) = {
#if ENABLE_WORKER_METRIC_TIME_GETWORK_NAK
    WORKER_METRIC_TIME_GETWORK_NAK_TYPE,
#endif
#if ENABLE_WORKER_METRIC_TIME_GETWORK_ACK
    WORKER_METRIC_TIME_GETWORK_ACK_TYPE,
#endif
#if ENABLE_WORKER_METRIC_TIME_EXEC_EDT
    WORKER_METRIC_TIME_EXEC_EDT_TYPE,
#endif
};

static u8 WORKER_MetricsDETAILED_Types[WORKER_METRIC_DETAILED_MAX] __attribute__((unused)) = {
#if ENABLE_WORKER_METRIC_TIME_GETWORK_NAK_DETAILED
    WORKER_METRIC_TIME_GETWORK_NAK_TYPE,
#endif
#if ENABLE_WORKER_METRIC_TIME_GETWORK_ACK_DETAILED
    WORKER_METRIC_TIME_GETWORK_ACK_TYPE,
#endif
};

//End copy-paste to metrics.c from generateMacroMetricDefs.sh

double metricsSqrt(double i) {
#if (defined(TG_XE_TARGET) || defined(TG_CE_TARGET) || defined(ENABLE_BUILDER_ONLY))
    return ((double)0.0);
#else
    return sqrt(i);
#endif
}

static void computeAvg(detailedMetric_t * metric) {
    // compute avg and stddev
    u32 j = 0;
    u64 avg = 0;
    u64 modd = metric->count % METRIC_MAX_ENTRY_COUNT;
    u64 ub = !modd /*full*/ ? METRIC_MAX_ENTRY_COUNT : modd;
    while (j < ub) {
        avg += metric->vals[j++];
    }
    avg = (avg / ub);
    j = 0;
    u64 dev = 0;
    while (j < ub) {
        u64 diff = (avg > metric->vals[j]) ?
                   (avg - metric->vals[j]) : (metric->vals[j] - avg);
        dev += (diff * diff);
        j++;
    }
    metric->dev = metricsSqrt(dev/ub);
    u64 oldAvg = metric->avg;
    metric->avg += avg;
    if (oldAvg != 0) {
        metric->avg /= 2;
    }
}

// want to keep the backing storage but
//  - apply a different algorithm to it
//  - change the datatype
void recordDetailed(detailedMetric_t * metric, u64 value) {
    if (metric->vals == NULL) {
        ocrPolicyDomain_t *pd = NULL;
        getCurrentEnv(&pd, NULL, NULL, NULL);
        metric->vals = pd->fcts.pdMalloc(pd, sizeof(u64)*METRIC_MAX_ENTRY_COUNT);
    }
    metric->vals[metric->count % METRIC_MAX_ENTRY_COUNT] = value;
    metric->count++;
    if (value < metric->min) {
        metric->min = value;
    }
    if (value > metric->max) {
        metric->max = value;
    }
    if ((metric->count % METRIC_MAX_ENTRY_COUNT) == 0) {
        if (METRIC_MAX_ENTRY_COUNT_ERROR) {
            ocrAssert(false && "Maximum of metric allowed reached");
        } else {
            computeAvg(metric);
        }
    }
}


//
//
// Metrics Analysis
//
//


#if ENABLE_EDT_METRICS && ENABLE_WORKER_METRICS
// TODO: shall we put these in a separate metrics-analysis.h ?

// Given a metric store, compute a new metric.
// The analysis code should only be active if all the required input metrics are available

#if (ENABLE_EDT_METRICS_ANALYSIS_RT_USER_RATIO && ENABLE_EDT_METRIC_TIME_FUNC && ENABLE_EDT_METRIC_TIME_USER)

// Input is an EDT metric store. Output is the same EDT metric store, with the analysis metric updated or not.
void userRtMetricAnalysis(EDT_MetricStore_t * edtMetricStore) {
    u64 inEdtRtDuration, userDuration, prologueDuration, epilogueDuration, outEdtRtDuration, totalRtDuration;
    // Time spent in the EDT user code
    READ_DIFF(edtMetricStore, EDT, EDT_METRIC_TIME_FUNC, inEdtRtDuration)
    // Time spent in the EDT user code, excluding calls in the OCR runtime
    READ_DIFF(edtMetricStore, EDT, EDT_METRIC_TIME_USER, userDuration)
    // Actual time spent in the OCR runtime while executing the EDT user code
    inEdtRtDuration -= userDuration;
    READ_DIFF(edtMetricStore, EDT, EDT_METRIC_TIME_PROLOGUE, prologueDuration)
    READ_DIFF(edtMetricStore, EDT, EDT_METRIC_TIME_EPILOGUE, epilogueDuration)
    outEdtRtDuration = prologueDuration + epilogueDuration;
    totalRtDuration = inEdtRtDuration + outEdtRtDuration;
    double rtUserRatio = ((double)totalRtDuration / (totalRtDuration+userDuration));
    u64 rtUserRatioAsInt = (rtUserRatio * 100000); //because we store as u64 and don't want to loose the precision too much
    // DPRINTF(DEBUG_LVL_WARN, "MK totalRtDuration=%"PRIu64" userDuration=%"PRIu64" rtUserRatio=%0.3f rtUserRatioAsInt=%"PRIu64"\n", totalRtDuration, userDuration, rtUserRatio, rtUserRatioAsPercent);
    RECORD_RATIO(edtMetricStore, EDT, EDT_METRIC_RATIO_RT_USER, rtUserRatioAsInt)
}

#endif

#endif


//
//
// End Metrics Analysis
//
//


#if ENABLE_EDT_METRICS && ENABLE_WORKER_METRICS

// Aggregate simple metric's value field
// TODO: the actual average type should be read from a data-structure
#define AGGREGATE_METRIC_TYPE(TYPE, DST_ENTITY, SRC_ENTITY, dst, src, PRIMITIVE_TYPE) \
    {  u16 j; \
       for (j=0; j<(SRC_ENTITY##_METRIC_##TYPE##_MAX-1); j++) { \
        u64 value = (src)->TYPE##_Metrics[j].value; \
        ((SRC_ENTITY##_MetricStore_t *) (dst))->TYPE##_Metrics[j].value += value; \
        u64 aggId = resolveAgg_##TYPE##_##SRC_ENTITY##_##DST_ENTITY(j); \
        if (aggId != 0) { \
            detailedMetric_t * dmetric = &((dst)->DETAILED_Metrics[METRIC_ID_AS_SLOT(aggId)]); \
            RECORD_DETAILED(dmetric, PRIMITIVE_TYPE, value, METRIC_DETAILED_KIND_STD) \
        } \
       } \
    }

// Aggregate two stores of certain entity kind
#define AGGREGATE_METRIC_STORES(DST_ENTITY, SRC_ENTITY, dst, src) \
  { \
    AGGREGATE_METRIC_TYPE(COUNTER, DST_ENTITY, SRC_ENTITY, dst, src, u64) \
    AGGREGATE_METRIC_TYPE(DIFF, DST_ENTITY, SRC_ENTITY, dst, src, u64) \
    AGGREGATE_METRIC_TYPE(RATIO, DST_ENTITY, SRC_ENTITY, dst, src, double) \
  }

#define IS_DETAILED(METRIC_NAME, SRC, DST) \
    ((ENABLE_##METRIC_NAME) && (ENABLE_##METRIC_NAME##_AGG_##SRC##_##DST))

#define CONVERT_METRIC_NAME_TO_AGG(METRIC_NAME, SRC, DST, metricSlot) \
    if ((ENABLE_##METRIC_NAME) && (METRIC_ID_AS_SLOT(METRIC_NAME) == (metricSlot))) { \
        if (ENABLE_##METRIC_NAME##_AGG_##SRC##_##DST) { \
            return METRIC_NAME##_AGG_##SRC##_##DST; \
        } else { \
            return 0; \
        } \
    }

u64 resolveAgg_COUNTER_EDT_WORKER(u64 metricSlot) {
    return 0;
}

//This require manually adding new metrics and that sucks
u64 resolveAgg_DIFF_EDT_WORKER(u64 metricSlot) {
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_DEPV_ACQUIRED, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_PROLOGUE, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_FUNC, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_EPILOGUE, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_ACTION_EVT_CREATE, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_ACTION_EVT_DESTROY, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_ACTION_EVT_SATISFY, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_ACTION_TPL_CREATE, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_ACTION_TPL_DESTROY, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_ACTION_EDT_CREATE, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_ACTION_EDT_DESTROY, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_ACTION_ADDDEP, EDT, WORKER, (metricSlot))
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_TIME_USER, EDT, WORKER, (metricSlot))
    DPRINTF (DEBUG_LVL_WARN, "warning: unsupported metric in resolveAgg_DIFF_EDT_WORKER metricSlot=%"PRIu64"\n", metricSlot);
    return 0;
}

u64 resolveAgg_RATIO_EDT_WORKER(u64 metricSlot) {
    CONVERT_METRIC_NAME_TO_AGG(EDT_METRIC_RATIO_RT_USER, EDT, WORKER, (metricSlot))
    DPRINTF (DEBUG_LVL_WARN, "warning: unsupported metric in resolveAgg_RATIO_EDT_WORKER metricSlot=%"PRIu64"\n", metricSlot);
    return 0;
}

// Aggregate metrics from an EDT metric store into current worker's metric store
void aggregateEDTMetricStoreToWorker(EDT_MetricStore_t * metricStore) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    // Realloc if running out of space
    // Look up in the worker store for an EDT metric store representent given its key
    u64 * key = metricStore->key;
    u16 i, ub = worker->edtMetricStoresCount;
    for(i=0; i<ub; i++) {
        if (key == ((EDT_MetricStore_t *) &(worker->edtMetricStores[i]))->key) {
            break;
        }
    }
    if ((i == ub) && (i == worker->edtMetricStoresCountMax)) {
        // Didn't find the entry and it's full or not alloc
        u16 newUb = (ub) ? (ub*2) : WORKER_EDT_METRIC_STORE_DEFAULT_COUNT;
        ocrAssert(ub < newUb); // overflow
        EDT_WORKER_AGG_MetricStore_t * newEdtMetricStores = worker->pd->fcts.pdMalloc(worker->pd, sizeof(EDT_WORKER_AGG_MetricStore_t) * newUb);
        if (ub != 0) {
            hal_memCopy(newEdtMetricStores, worker->edtMetricStores, (sizeof(EDT_WORKER_AGG_MetricStore_t) * ub), false);
            worker->pd->fcts.pdFree(worker->pd, worker->edtMetricStores);
        }
        worker->edtMetricStoresCount = ub;
        worker->edtMetricStoresCountMax = newUb;
        worker->edtMetricStores = newEdtMetricStores;
    }
    if (i == ub) { // this is a new entry
        EDT_MetricStore_t * base = ((EDT_MetricStore_t *) &(worker->edtMetricStores[i]));
        EDT_WORKER_AGG_MetricStore_t * derived = ((EDT_WORKER_AGG_MetricStore_t *) base);
        INIT_EDT_METRIC_STORE(base, key)
        u16 j;
        for (j=0; j<(EDT_METRIC_AGG_EDT_WORKER_MAX-1); j++) {
            INIT_METRIC_DETAILED(&(derived->DETAILED_Metrics[j]))
        }
        worker->edtMetricStoresCount++;
    } else {
    }
    ocrAssert(key != NULL);
    ocrAssert(((EDT_MetricStore_t *) &(worker->edtMetricStores[i]))->key != NULL);
#if ENABLE_EDT_METRICS_ANALYSIS_RT_USER_RATIO
    userRtMetricAnalysis(metricStore);
#endif
    AGGREGATE_METRIC_STORES(WORKER, EDT, (&worker->edtMetricStores[i]), metricStore)
    ocrAssert(((EDT_MetricStore_t *) &(worker->edtMetricStores[i]))->key != NULL);
}

#endif /* enabled WORKER and EDT metric */


void dumpMetricValue(ocrLocation_t pdLoc, u64 wid, u64 * key, char * entity, char * name, u64 value, u8 primitiveType) {
    ocrPrintf("[PD:%"PRIu64" W:%"PRIu64" EDT:0x0][%s] | key=0x%"PRIx64" action=%s value=%"PRIu64"\n", pdLoc, wid, entity, (u64) key, name, value);
}

void dumpMetric_COUNTER(ocrLocation_t pdLoc, u64 wid, u64 * key, char * entity, char * name, counterMetric_t * metricPtr, u8 primitiveType) {
    dumpMetricValue(pdLoc, wid, key, entity, name, metricPtr->value, primitiveType);
}

void dumpMetric_DIFF(ocrLocation_t pdLoc, u64 wid, u64 * key, char * entity, char * name, diffMetric_t * metricPtr, u8 primitiveType) {
    dumpMetricValue(pdLoc, wid, key, entity, name, metricPtr->value, primitiveType);
}

void dumpMetric_RATIO(ocrLocation_t pdLoc, u64 wid, u64 * key, char * entity, char * name, ratioMetric_t * metricPtr, u8 primitiveType) {
    ocrPrintf("[PD:%"PRIu64" W:%"PRIu64" EDT:0x0][%s] | key=0x%"PRIx64" action=%s value=%.3f\n", pdLoc, wid, entity, (u64) key, name, metricPtr->value);
}

void dumpMetric_DETAILED(ocrLocation_t pdLoc, u64 wid, u64 * key, char * entity, char * name, detailedMetric_t * m, u8 primitiveType) {
    if (m->count) {
        // check if we need to compute average
        if (m->count % METRIC_MAX_ENTRY_COUNT) {
            if (primitiveType == PRIMITIVE_TYPE_DOUBLE) {
                COMPUTE_AVG(m, u64);
            } else {
                COMPUTE_AVG(m, u64);
            }
        }
        //TODO we need to distinguish between u64 and double metrics here
        //We can have an array that for each metric defines the type
        //and use a conditional here to branch to the right code
        if (primitiveType == PRIMITIVE_TYPE_DOUBLE) {
            ocrPrintf("[PD:%"PRIu64" W:%"PRIu64" EDT:0x0][%s] | key=0x%"PRIx64" action=%s avg=%.3f dev=%.3f min=%.3f max=%.3f ratioMinMax=%.5f count=%"PRIu64" DOUBLE\n",
                   pdLoc, wid, entity, (u64) key, name,(double)m->avg, (double)m->dev, (double)m->min, (double)m->max, (m->min != 0.0) ? (((double)m->max)/m->min) : m->max, m->count);
        } else {
            ocrPrintf("[PD:%"PRIu64" W:%"PRIu64" EDT:0x0][%s] | key=0x%"PRIx64" action=%s avg=%"PRIu64" dev=%"PRIu64" min=%"PRIu64" max=%"PRIu64" ratioMinMax=%.5f count=%"PRIu64"\n",
                   pdLoc, wid, entity, (u64) key, name, m->avg, m->dev, m->min, m->max, (m->min != 0) ? (((double)m->max)/m->min) : m->max, m->count);
        }
    }
}

#define DUMP_METRIC_AGG(pdLoc, wid, key, metricStorePtr, ENTITY, TYPE, AGG) { \
    u16 j; \
    for(j=0; j<(ENTITY##_METRIC_##AGG##_MAX-1); j++) {  \
        dumpMetric_##TYPE((pdLoc), (wid), (key), #ENTITY, ENTITY##_Metrics##AGG##_Names[j], &((metricStorePtr)->TYPE##_Metrics[j]), ENTITY##_Metrics##AGG##_Types[j]); \
    } \
}

#define DUMP_METRIC(pdLoc, wid, key, metricStorePtr, ENTITY, TYPE) DUMP_METRIC_AGG(pdLoc, wid, key, metricStorePtr, ENTITY, TYPE, TYPE)

#if ENABLE_WORKER_METRICS
void dumpWorkerMetric(ocrWorker_t * worker, WORKER_MetricStore_t * store) {
    ocrLocation_t pdLoc = worker->pd->myLocation;
    u64 wid = worker->id;
    DUMP_METRIC(pdLoc, wid, 0, store, WORKER, DIFF)
    DUMP_METRIC(pdLoc, wid, 0, store, WORKER, DETAILED)
#if ENABLE_EDT_METRICS
    dumpEdtMetric(worker, worker->edtMetricStores, worker->edtMetricStoresCount);
#endif
}
#endif

#if ENABLE_EDT_METRICS

void dumpEdtMetric(ocrWorker_t * worker, EDT_WORKER_AGG_MetricStore_t * storePtr, u16 count) {
    u16 i;
    ocrLocation_t pdLoc = worker->pd->myLocation;
    u64 wid = worker->id;
    for(i=0; i<count; i++) {
        EDT_WORKER_AGG_MetricStore_t * storePtrInstance = &storePtr[i];
        EDT_MetricStore_t * metricStorePtr = ((EDT_MetricStore_t *) storePtrInstance);
        u64 * key = metricStorePtr->key;
        ocrAssert(key != NULL);
        // For a given EDT metric store, print out accumulated metrics
        DUMP_METRIC(pdLoc, wid, key, metricStorePtr, EDT, COUNTER)
        DUMP_METRIC(pdLoc, wid, key, metricStorePtr, EDT, DIFF)
        DUMP_METRIC(pdLoc, wid, key, metricStorePtr, EDT, RATIO)
        DUMP_METRIC(pdLoc, wid, key, metricStorePtr, EDT, DETAILED)
#ifdef ENABLE_WORKER_METRICS
        // Print out detailed metrics information if available
        DUMP_METRIC_AGG(pdLoc, wid, key, (storePtrInstance), EDT, DETAILED, AGG_EDT_WORKER)
#endif
    }
}

#endif

