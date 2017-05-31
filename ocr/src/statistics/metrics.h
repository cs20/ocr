/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __METRICS_H__
#define __METRICS_H__

#include <limits.h>

#include "ocr-config.h"
#include "ocr-types.h"

struct _ocrPolicyMsg_t;
struct _ocrWorker_t;

#ifndef ENABLE_EDT_METRICS
#define ENABLE_EDT_METRICS 0
#endif

#ifndef ENABLE_WORKER_METRICS
#define ENABLE_WORKER_METRICS 0
#endif

// NOTE: These can be enabled from the build command line of
//       the OCR runtime or be selectively uncommented here.
// #define ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED 1
// #define ENABLE_EDT_METRIC_TIME_PROLOGUE 1
// #define ENABLE_EDT_METRIC_TIME_FUNC 1
// #define ENABLE_EDT_METRIC_TIME_EPILOGUE 1
// #define ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE 1
// #define ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY 1
// #define ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY 1
// #define ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE 1
// #define ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY 1
// #define ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE 1
// #define ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY 1
// #define ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP 1
// #define ENABLE_EDT_METRIC_TIME_USER 1
// #define ENABLE_EDT_METRIC_RATIO_RT_USER 1
// #define ENABLE_EDT_METRIC_RATIO_RT_USER_DETAILED 1
// #define ENABLE_EDT_METRICS_ANALYSIS_RT_USER_RATIO 1

// #define ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_AGG_EDT_WORKER 1
// #define ENABLE_EDT_METRIC_TIME_PROLOGUE_AGG_EDT_WORKER 1
// #define ENABLE_EDT_METRIC_TIME_FUNC_AGG_EDT_WORKER 1
// #define ENABLE_EDT_METRIC_TIME_EPILOGUE_AGG_EDT_WORKER 1
// #define ENABLE_EDT_METRIC_RATIO_RT_USER_AGG_EDT_WORKER 1

// #define ENABLE_WORKER_METRIC_TIME_GETWORK_NAK 1
// #define ENABLE_WORKER_METRIC_TIME_GETWORK_NAK 1
// #define ENABLE_WORKER_METRIC_TIME_GETWORK_ACK_DETAILED 1
// #define ENABLE_WORKER_METRIC_TIME_GETWORK_NAK_DETAILED 1
// #define ENABLE_WORKER_METRIC_TIME_EXEC_EDT 1


// Experimental detailed metrics tracking variants
#define METRIC_DETAILED_KIND_STD 0
// TODO: Not implemented yet
#define METRIC_DETAILED_KIND_SMA 1
#define METRIC_DETAILED_KIND_AVG 2

#define PRIMITIVE_TYPE_INTEGER ((u8)1)
#define PRIMITIVE_TYPE_DOUBLE  ((u8)2)

// If ENABLE_XYZ is not defined then:
// - define it to zero
// - define XYZ to zero
//
// It effectively kills all the compilation paths that uses the metric
// To turn on a particular metric XYZ define ENABLE_XYZ=1 but do not
// define XYZ itself, it's part of an enum definition activated by ENABLE_XYZ
// Additionally, for a given XYZ, one must always define the _DETAILED counter-part


//Begin copy-paste to metrics.h from generateMacroMetricDefs.sh


// Metric names

#define EDT_METRIC_TIME_DEPV_ACQUIRED_NAME "depvAcquired"
#define EDT_METRIC_TIME_PROLOGUE_NAME "prologue"
#define EDT_METRIC_TIME_FUNC_NAME "func"
#define EDT_METRIC_TIME_EPILOGUE_NAME "epilogue"
#define EDT_METRIC_COUNT_EDT_CREATE_NAME "edtCreate"
#define EDT_METRIC_COUNT_EVT_SATISFY_NAME "evtSatisfy"
#define EDT_METRIC_TIME_ACTION_EVT_CREATE_NAME "actionEvtCreate"
#define EDT_METRIC_TIME_ACTION_EVT_DESTROY_NAME "actionEvtDestroy"
#define EDT_METRIC_TIME_ACTION_EVT_SATISFY_NAME "actionEvtSatisfy"
#define EDT_METRIC_TIME_ACTION_TPL_CREATE_NAME "actionTplCreate"
#define EDT_METRIC_TIME_ACTION_TPL_DESTROY_NAME "actionTplDestroy"
#define EDT_METRIC_TIME_ACTION_EDT_CREATE_NAME "actionEdtCreate"
#define EDT_METRIC_TIME_ACTION_EDT_DESTROY_NAME "actionEdtDestroy"
#define EDT_METRIC_TIME_ACTION_ADDDEP_NAME "actionAdddep"
#define EDT_METRIC_TIME_USER_NAME "actionUser"
#define EDT_METRIC_RATIO_RT_USER_NAME "ratioRtUser"
#define WORKER_METRIC_TIME_GETWORK_NAK_NAME "getWorkNak"
#define WORKER_METRIC_TIME_GETWORK_ACK_NAME "getWorkAck"
#define WORKER_METRIC_TIME_EXEC_EDT_NAME "execEdt"

// Metric Primitive Types

#define EDT_METRIC_TIME_DEPV_ACQUIRED_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_PROLOGUE_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_FUNC_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_EPILOGUE_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_COUNT_EDT_CREATE_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_COUNT_EVT_SATISFY_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_ACTION_EVT_CREATE_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_ACTION_EVT_DESTROY_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_ACTION_EVT_SATISFY_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_ACTION_TPL_CREATE_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_ACTION_TPL_DESTROY_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_ACTION_EDT_CREATE_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_ACTION_EDT_DESTROY_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_ACTION_ADDDEP_TYPE PRIMITIVE_TYPE_INTEGER
#define EDT_METRIC_TIME_USER_TYPE PRIMITIVE_TYPE_INTEGER
// #define EDT_METRIC_RATIO_RT_USER_TYPE PRIMITIVE_TYPE_DOUBLE
#define EDT_METRIC_RATIO_RT_USER_TYPE PRIMITIVE_TYPE_INTEGER
#define WORKER_METRIC_TIME_GETWORK_NAK_TYPE PRIMITIVE_TYPE_INTEGER
#define WORKER_METRIC_TIME_GETWORK_ACK_TYPE PRIMITIVE_TYPE_INTEGER
#define WORKER_METRIC_TIME_EXEC_EDT_TYPE PRIMITIVE_TYPE_INTEGER

// Metric declarations

#ifndef ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED
#define ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED 0
#define EDT_METRIC_TIME_DEPV_ACQUIRED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_DETAILED
#define ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_DETAILED 0
#define EDT_METRIC_TIME_DEPV_ACQUIRED_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_DEPV_ACQUIRED_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_PROLOGUE
#define ENABLE_EDT_METRIC_TIME_PROLOGUE 0
#define EDT_METRIC_TIME_PROLOGUE 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_PROLOGUE_DETAILED
#define ENABLE_EDT_METRIC_TIME_PROLOGUE_DETAILED 0
#define EDT_METRIC_TIME_PROLOGUE_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_PROLOGUE_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_PROLOGUE_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_PROLOGUE_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_FUNC
#define ENABLE_EDT_METRIC_TIME_FUNC 0
#define EDT_METRIC_TIME_FUNC 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_FUNC_DETAILED
#define ENABLE_EDT_METRIC_TIME_FUNC_DETAILED 0
#define EDT_METRIC_TIME_FUNC_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_FUNC_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_FUNC_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_FUNC_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_EPILOGUE
#define ENABLE_EDT_METRIC_TIME_EPILOGUE 0
#define EDT_METRIC_TIME_EPILOGUE 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_EPILOGUE_DETAILED
#define ENABLE_EDT_METRIC_TIME_EPILOGUE_DETAILED 0
#define EDT_METRIC_TIME_EPILOGUE_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_EPILOGUE_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_EPILOGUE_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_EPILOGUE_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_COUNT_EDT_CREATE
#define ENABLE_EDT_METRIC_COUNT_EDT_CREATE 0
#define EDT_METRIC_COUNT_EDT_CREATE 0
#endif
#ifndef ENABLE_EDT_METRIC_COUNT_EDT_CREATE_DETAILED
#define ENABLE_EDT_METRIC_COUNT_EDT_CREATE_DETAILED 0
#define EDT_METRIC_COUNT_EDT_CREATE_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_COUNT_EDT_CREATE_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_COUNT_EDT_CREATE_AGG_EDT_WORKER 0
#define EDT_METRIC_COUNT_EDT_CREATE_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_COUNT_EVT_SATISFY
#define ENABLE_EDT_METRIC_COUNT_EVT_SATISFY 0
#define EDT_METRIC_COUNT_EVT_SATISFY 0
#endif
#ifndef ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_DETAILED
#define ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_DETAILED 0
#define EDT_METRIC_COUNT_EVT_SATISFY_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_AGG_EDT_WORKER 0
#define EDT_METRIC_COUNT_EVT_SATISFY_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE 0
#define EDT_METRIC_TIME_ACTION_EVT_CREATE 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_DETAILED
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_DETAILED 0
#define EDT_METRIC_TIME_ACTION_EVT_CREATE_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_ACTION_EVT_CREATE_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY 0
#define EDT_METRIC_TIME_ACTION_EVT_DESTROY 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_DETAILED
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_DETAILED 0
#define EDT_METRIC_TIME_ACTION_EVT_DESTROY_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_ACTION_EVT_DESTROY_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY 0
#define EDT_METRIC_TIME_ACTION_EVT_SATISFY 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_DETAILED
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_DETAILED 0
#define EDT_METRIC_TIME_ACTION_EVT_SATISFY_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_ACTION_EVT_SATISFY_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE
#define ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE 0
#define EDT_METRIC_TIME_ACTION_TPL_CREATE 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_DETAILED
#define ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_DETAILED 0
#define EDT_METRIC_TIME_ACTION_TPL_CREATE_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_ACTION_TPL_CREATE_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY
#define ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY 0
#define EDT_METRIC_TIME_ACTION_TPL_DESTROY 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_DETAILED
#define ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_DETAILED 0
#define EDT_METRIC_TIME_ACTION_TPL_DESTROY_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_ACTION_TPL_DESTROY_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE
#define ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE 0
#define EDT_METRIC_TIME_ACTION_EDT_CREATE 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_DETAILED
#define ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_DETAILED 0
#define EDT_METRIC_TIME_ACTION_EDT_CREATE_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_ACTION_EDT_CREATE_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY
#define ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY 0
#define EDT_METRIC_TIME_ACTION_EDT_DESTROY 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_DETAILED
#define ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_DETAILED 0
#define EDT_METRIC_TIME_ACTION_EDT_DESTROY_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_ACTION_EDT_DESTROY_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP
#define ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP 0
#define EDT_METRIC_TIME_ACTION_ADDDEP 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_DETAILED
#define ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_DETAILED 0
#define EDT_METRIC_TIME_ACTION_ADDDEP_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_ACTION_ADDDEP_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_TIME_USER
#define ENABLE_EDT_METRIC_TIME_USER 0
#define EDT_METRIC_TIME_USER 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_USER_DETAILED
#define ENABLE_EDT_METRIC_TIME_USER_DETAILED 0
#define EDT_METRIC_TIME_USER_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_TIME_USER_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_TIME_USER_AGG_EDT_WORKER 0
#define EDT_METRIC_TIME_USER_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_EDT_METRIC_RATIO_RT_USER
#define ENABLE_EDT_METRIC_RATIO_RT_USER 0
#define EDT_METRIC_RATIO_RT_USER 0
#endif
#ifndef ENABLE_EDT_METRIC_RATIO_RT_USER_DETAILED
#define ENABLE_EDT_METRIC_RATIO_RT_USER_DETAILED 0
#define EDT_METRIC_RATIO_RT_USER_DETAILED 0
#endif
#ifndef ENABLE_EDT_METRIC_RATIO_RT_USER_AGG_EDT_WORKER
#define ENABLE_EDT_METRIC_RATIO_RT_USER_AGG_EDT_WORKER 0
#define EDT_METRIC_RATIO_RT_USER_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_WORKER_METRIC_TIME_GETWORK_NAK
#define ENABLE_WORKER_METRIC_TIME_GETWORK_NAK 0
#define WORKER_METRIC_TIME_GETWORK_NAK 0
#endif
#ifndef ENABLE_WORKER_METRIC_TIME_GETWORK_NAK_DETAILED
#define ENABLE_WORKER_METRIC_TIME_GETWORK_NAK_DETAILED 0
#define WORKER_METRIC_TIME_GETWORK_NAK_DETAILED 0
#endif
#ifndef ENABLE_WORKER_METRIC_TIME_GETWORK_NAK_AGG_EDT_WORKER
#define ENABLE_WORKER_METRIC_TIME_GETWORK_NAK_AGG_EDT_WORKER 0
#define WORKER_METRIC_TIME_GETWORK_NAK_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_WORKER_METRIC_TIME_GETWORK_ACK
#define ENABLE_WORKER_METRIC_TIME_GETWORK_ACK 0
#define WORKER_METRIC_TIME_GETWORK_ACK 0
#endif
#ifndef ENABLE_WORKER_METRIC_TIME_GETWORK_ACK_DETAILED
#define ENABLE_WORKER_METRIC_TIME_GETWORK_ACK_DETAILED 0
#define WORKER_METRIC_TIME_GETWORK_ACK_DETAILED 0
#endif
#ifndef ENABLE_WORKER_METRIC_TIME_GETWORK_ACK_AGG_EDT_WORKER
#define ENABLE_WORKER_METRIC_TIME_GETWORK_ACK_AGG_EDT_WORKER 0
#define WORKER_METRIC_TIME_GETWORK_ACK_AGG_EDT_WORKER 0
#endif

#ifndef ENABLE_WORKER_METRIC_TIME_EXEC_EDT
#define ENABLE_WORKER_METRIC_TIME_EXEC_EDT 0
#define WORKER_METRIC_TIME_EXEC_EDT 0
#endif
#ifndef ENABLE_WORKER_METRIC_TIME_EXEC_EDT_DETAILED
#define ENABLE_WORKER_METRIC_TIME_EXEC_EDT_DETAILED 0
#define WORKER_METRIC_TIME_EXEC_EDT_DETAILED 0
#endif
#ifndef ENABLE_WORKER_METRIC_TIME_EXEC_EDT_AGG_EDT_WORKER
#define ENABLE_WORKER_METRIC_TIME_EXEC_EDT_AGG_EDT_WORKER 0
#define WORKER_METRIC_TIME_EXEC_EDT_AGG_EDT_WORKER 0
#endif


// EDT Metric enumerations


typedef enum _EDT_MetricsRatioEnum {
    EDT_METRIC_RATIO_BASE = 0,
#if ENABLE_EDT_METRIC_RATIO_RT_USER
    EDT_METRIC_RATIO_RT_USER,
#endif
    EDT_METRIC_RATIO_MAX
} EDT_MetricsRatioEnum;

typedef enum _EDT_MetricsCounterEnum {
    EDT_METRIC_COUNTER_BASE = 0,
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE
    EDT_METRIC_COUNT_EDT_CREATE,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY
    EDT_METRIC_COUNT_EVT_SATISFY,
#endif
    EDT_METRIC_COUNTER_MAX
} EDT_MetricsCounterEnum;

typedef enum _EDT_MetricsDiffEnum {
    EDT_METRIC_DIFF_BASE = 0,
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED
    EDT_METRIC_TIME_DEPV_ACQUIRED,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE
    EDT_METRIC_TIME_PROLOGUE,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC
    EDT_METRIC_TIME_FUNC,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE
    EDT_METRIC_TIME_EPILOGUE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE
    EDT_METRIC_TIME_ACTION_EVT_CREATE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY
    EDT_METRIC_TIME_ACTION_EVT_DESTROY,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY
    EDT_METRIC_TIME_ACTION_EVT_SATISFY,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE
    EDT_METRIC_TIME_ACTION_TPL_CREATE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY
    EDT_METRIC_TIME_ACTION_TPL_DESTROY,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE
    EDT_METRIC_TIME_ACTION_EDT_CREATE,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY
    EDT_METRIC_TIME_ACTION_EDT_DESTROY,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP
    EDT_METRIC_TIME_ACTION_ADDDEP,
#endif
#if ENABLE_EDT_METRIC_TIME_USER
    EDT_METRIC_TIME_USER,
#endif
    EDT_METRIC_DIFF_MAX
} EDT_MetricsDiffEnum;

typedef enum _EDT_MetricsDetailedEnum {
    EDT_METRIC_DETAILED_BASE = 0,
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_DETAILED
    EDT_METRIC_TIME_DEPV_ACQUIRED_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE_DETAILED
    EDT_METRIC_TIME_PROLOGUE_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC_DETAILED
    EDT_METRIC_TIME_FUNC_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE_DETAILED
    EDT_METRIC_TIME_EPILOGUE_DETAILED,
#endif
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE_DETAILED
    EDT_METRIC_COUNT_EDT_CREATE_DETAILED,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_DETAILED
    EDT_METRIC_COUNT_EVT_SATISFY_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_CREATE_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_DESTROY_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_DETAILED
    EDT_METRIC_TIME_ACTION_EVT_SATISFY_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_TPL_CREATE_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_TPL_DESTROY_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_DETAILED
    EDT_METRIC_TIME_ACTION_EDT_CREATE_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_DETAILED
    EDT_METRIC_TIME_ACTION_EDT_DESTROY_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_DETAILED
    EDT_METRIC_TIME_ACTION_ADDDEP_DETAILED,
#endif
#if ENABLE_EDT_METRIC_TIME_USER_DETAILED
    EDT_METRIC_TIME_USER_DETAILED,
#endif
#if ENABLE_EDT_METRIC_RATIO_RT_USER_DETAILED
    EDT_METRIC_RATIO_RT_USER_DETAILED,
#endif
    EDT_METRIC_DETAILED_MAX
} EDT_MetricsDetailedEnum;

// Worker/Edt aggregation metrics enumerations

typedef enum _EDT_WORKER_AGG_MetricsEnum {
    EDT_METRIC_AGG_EDT_WORKER_BASE = 0,
#if ENABLE_EDT_METRIC_TIME_DEPV_ACQUIRED_AGG_EDT_WORKER
    EDT_METRIC_TIME_DEPV_ACQUIRED_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_PROLOGUE_AGG_EDT_WORKER
    EDT_METRIC_TIME_PROLOGUE_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_FUNC_AGG_EDT_WORKER
    EDT_METRIC_TIME_FUNC_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_EPILOGUE_AGG_EDT_WORKER
    EDT_METRIC_TIME_EPILOGUE_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_COUNT_EDT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_COUNT_EDT_CREATE_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_COUNT_EVT_SATISFY_AGG_EDT_WORKER
    EDT_METRIC_COUNT_EVT_SATISFY_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_CREATE_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_DESTROY_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EVT_SATISFY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EVT_SATISFY_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_TPL_CREATE_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_TPL_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_TPL_DESTROY_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_CREATE_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EDT_CREATE_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_EDT_DESTROY_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_EDT_DESTROY_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_ACTION_ADDDEP_AGG_EDT_WORKER
    EDT_METRIC_TIME_ACTION_ADDDEP_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_TIME_USER_AGG_EDT_WORKER
    EDT_METRIC_TIME_USER_AGG_EDT_WORKER,
#endif
#if ENABLE_EDT_METRIC_RATIO_RT_USER_AGG_EDT_WORKER
    EDT_METRIC_RATIO_RT_USER_AGG_EDT_WORKER,
#endif
    EDT_METRIC_AGG_EDT_WORKER_MAX
} EDT_WORKER_AGG_MetricsEnum;

// WORKER Metric enumerations

typedef enum _WORKER_MetricsRatioEnum {
    WORKER_METRIC_RATIO_BASE = 0,
    WORKER_METRIC_RATIO_MAX
} WORKER_MetricsRatioEnum;

typedef enum _WORKER_MetricsCounterEnum {
    WORKER_METRIC_COUNTER_BASE = 0,
    WORKER_METRIC_COUNTER_MAX
} WORKER_MetricsCounterEnum;

typedef enum _WORKER_MetricsDiffEnum {
    WORKER_METRIC_DIFF_BASE = 0,
#if ENABLE_WORKER_METRIC_TIME_GETWORK_NAK
    WORKER_METRIC_TIME_GETWORK_NAK,
#endif
#if ENABLE_WORKER_METRIC_TIME_GETWORK_ACK
    WORKER_METRIC_TIME_GETWORK_ACK,
#endif
#if ENABLE_WORKER_METRIC_TIME_EXEC_EDT
    WORKER_METRIC_TIME_EXEC_EDT,
#endif
    WORKER_METRIC_DIFF_MAX
} WORKER_MetricsDiffEnum;

typedef enum _WORKER_MetricsDetailedEnum {
    WORKER_METRIC_DETAILED_BASE = 0,
#if ENABLE_WORKER_METRIC_TIME_GETWORK_NAK_DETAILED
    WORKER_METRIC_TIME_GETWORK_NAK_DETAILED,
#endif
#if ENABLE_WORKER_METRIC_TIME_GETWORK_ACK_DETAILED
    WORKER_METRIC_TIME_GETWORK_ACK_DETAILED,
#endif
#if ENABLE_WORKER_METRIC_TIME_EXEC_EDT_DETAILED
    WORKER_METRIC_TIME_EXEC_EDT_DETAILED,
#endif
    WORKER_METRIC_DETAILED_MAX
} WORKER_MetricsDetailedEnum;
//End copy-paste to metrics.h from generateMacroMetricDefs.sh

// To assert if max entry count is reached in detailed profiling
#ifndef METRIC_MAX_ENTRY_COUNT_ERROR
#define METRIC_MAX_ENTRY_COUNT_ERROR 0
#endif

// Maximum number of entries a detailed metric can accomodate
// before recycling older entries.
// When METRIC_MAX_ENTRY_COUNT_ERROR is defined an assert
// occurs to indicate the maximum entry count as been reached
#ifndef METRIC_MAX_ENTRY_COUNT
#define METRIC_MAX_ENTRY_COUNT 300
#endif

#ifndef STAT_MSG_DETAILED_THRESHOLD
#define STAT_MSG_DETAILED_THRESHOLD 500000
#endif

#ifndef STAT_MSG_DETAILED_RECYCLE_EVERY
#define STAT_MSG_DETAILED_RECYCLE_EVERY 128
#endif

// Default number of metrics that are allocated
#define METRICS_MAX_DFLT_COUNT 32

#define METRIC_ID_AS_SLOT(METRIC_ID) ((METRIC_ID)-1)

typedef struct _counterMetric_t {
    u64 value;
} counterMetric_t;

typedef struct _diffMetric_t {
    u64 value;
    u64 tmp;
} diffMetric_t;

typedef struct _ratioMetric_t {
    double value;
} ratioMetric_t;

typedef struct _detailedMetric_t {
    u64 count;
    u64 avg;
    u64 dev;
    u64 min;
    u64 max;
    u64 * vals;
} detailedMetric_t;
// for running average I need
// - storage size to do a modulo
// - current pointer to the storage (that would be count ?)
// - is u64 data type a problem or can we assume that the accessor to this would do all the casting ?
// - flag stored there or rely on define to know what algorithm to apply.
//  - if we do define, then we need to know the calling context and pass it down to the compute part

//
// Per EDT Metrics
//

typedef struct _EDT_MetricStore_t {
    u64 * key;
    u64 rtApiNesting;
    counterMetric_t COUNTER_Metrics[EDT_METRIC_COUNTER_MAX-1];
    diffMetric_t DIFF_Metrics[EDT_METRIC_DIFF_MAX-1];
    ratioMetric_t RATIO_Metrics[EDT_METRIC_RATIO_MAX-1];
    detailedMetric_t DETAILED_Metrics[EDT_METRIC_DETAILED_MAX-1];
} EDT_MetricStore_t;

typedef struct _EDT_WORKER_AGG_MetricStore_t {
    EDT_MetricStore_t base;
    detailedMetric_t DETAILED_Metrics[EDT_METRIC_AGG_EDT_WORKER_MAX-1];
} EDT_WORKER_AGG_MetricStore_t;

typedef struct _WORKER_MetricStore_t {
    counterMetric_t COUNTER_Metrics[WORKER_METRIC_COUNTER_MAX-1];
    diffMetric_t DIFF_Metrics[WORKER_METRIC_DIFF_MAX-1];
    ratioMetric_t RATIO_Metrics[WORKER_METRIC_RATIO_MAX-1];
    detailedMetric_t DETAILED_Metrics[WORKER_METRIC_DETAILED_MAX-1];
} WORKER_MetricStore_t;

#define INIT_METRIC_COUNTER(metricPtr) \
    { (metricPtr)->value = 0; }

#define INIT_METRIC_DIFF(metricPtr) \
    { (metricPtr)->value = 0; \
      (metricPtr)->tmp = 0; \
    }

#define INIT_METRIC_RATIO(metricPtr) \
    { (metricPtr)->value = 0.0; }

#define INIT_METRIC_DETAILED(metricPtr) \
    { (metricPtr)->count = 0; \
        (metricPtr)->avg = 0; \
        (metricPtr)->dev = 0; \
        (metricPtr)->min = ULLONG_MAX; \
        (metricPtr)->max = 0; \
        (metricPtr)->vals = NULL; \
    }

// Initializes all the stores of a given entity
#define INIT_METRIC_STORE(metricStorePtr, ENTITY) \
    { \
        u16 i; \
        for (i=0; i<(ENTITY##_METRIC_COUNTER_MAX-1); i++) { \
            INIT_METRIC_COUNTER(&((metricStorePtr)->COUNTER_Metrics[i])) \
        } \
        for (i=0; i<(ENTITY##_METRIC_DIFF_MAX-1); i++) { \
            INIT_METRIC_DIFF(&((metricStorePtr)->DIFF_Metrics[i])) \
        } \
        for (i=0; i<(ENTITY##_METRIC_RATIO_MAX-1); i++) { \
            INIT_METRIC_RATIO(&((metricStorePtr)->RATIO_Metrics[i])) \
        } \
        for (i=0; i<(ENTITY##_METRIC_DETAILED_MAX-1); i++) { \
            INIT_METRIC_DETAILED(&((metricStorePtr)->DETAILED_Metrics[i])) \
        } \
    }

#define INIT_EDT_METRIC_STORE(METRICSTORE_PTR, KEY) { \
    (METRICSTORE_PTR)->key = (KEY); \
    (METRICSTORE_PTR)->rtApiNesting = 0; \
    INIT_METRIC_STORE((METRICSTORE_PTR), EDT) \
    }

// On even calls save current time stamp, on odd calls substract
// saved time stamp from current time stamp and accumulate result
#define RECORD_TIME(metricStorePtr, ENTITY, METRIC_NAME) \
    if (ENABLE_##ENTITY##_METRICS && ENABLE_##METRIC_NAME) { \
      diffMetric_t * metric = &(((ENTITY##_MetricStore_t *) (metricStorePtr))->DIFF_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME)]); \
      if (metric->tmp == 0)  \
        metric->tmp = salGetTime(); \
      else {                   \
        metric->value += (salGetTime() - metric->tmp); \
        metric->tmp = 0; \
      } \
    }

#define RT_SCOPE   (0)
#define USER_SCOPE (1)

#define START_TIME_SCOPED(ENTITY, METRIC_NAME, SCOPE) { \
    if (ENABLE_EDT_METRICS && ENABLE_##ENTITY##_METRICS && ENABLE_##METRIC_NAME) { \
        ocrTask_t * curEdt = NULL; \
        getCurrentEnv(NULL, NULL, &curEdt, NULL); \
        if (curEdt != NULL) { \
            if (curEdt->metricStore.rtApiNesting == 0) { \
                diffMetric_t * metric = &(((ENTITY##_MetricStore_t *) (&curEdt->metricStore))->DIFF_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME)]); \
                if (metric->tmp == 0)  \
                    metric->tmp = salGetTime(); \
                else {                   \
                    metric->value += (salGetTime() - metric->tmp); \
                    metric->tmp = 0; \
                } \
            } \
            if (SCOPE == RT_SCOPE) { \
                curEdt->metricStore.rtApiNesting++; \
            } \
        } \
    } \
}

#define STOP_TIME_SCOPED(ENTITY, METRIC_NAME, SCOPE) { \
    if (ENABLE_EDT_METRICS && ENABLE_##ENTITY##_METRICS && ENABLE_##METRIC_NAME) { \
        ocrTask_t * curEdt = NULL; \
        getCurrentEnv(NULL, NULL, &curEdt, NULL); \
        if (curEdt != NULL) { \
            if (SCOPE == RT_SCOPE) { \
                curEdt->metricStore.rtApiNesting--; \
            } \
            if (curEdt->metricStore.rtApiNesting == 0) { \
                diffMetric_t * metric = &(((ENTITY##_MetricStore_t *) (&curEdt->metricStore))->DIFF_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME)]); \
                if (metric->tmp == 0)  \
                    metric->tmp = salGetTime(); \
                else {                   \
                    metric->value += (salGetTime() - metric->tmp); \
                    metric->tmp = 0; \
                } \
            } \
        } \
    } \
}

// Read the diff metric's current value
#define READ_DIFF(metricStorePtr, ENTITY, METRIC_NAME, VRES) \
    if (ENABLE_##ENTITY##_METRICS && ENABLE_##METRIC_NAME) { \
        diffMetric_t * metric = &(((ENTITY##_MetricStore_t *) (metricStorePtr))->DIFF_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME)]); \
        VRES = metric->value; \
    }

// Read the ratio metric's current value
#define READ_RATIO(metricStorePtr, ENTITY, METRIC_NAME, VRES) \
    if (ENABLE_##ENTITY##_METRICS && ENABLE_##METRIC_NAME) { \
        ratioMetric_t * metric = &(((ENTITY##_MetricStore_t *) (metricStorePtr))->RATIO_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME)]); \
        VRES = metric->value; \
    }

#define u64_INITIALIZER (0)
#define double_INITIALIZER ((double) 0.0)

double metricsSqrt(double i);

//TODO: can't get double version in avg
#define COMPUTE_AVG(metricPTR, TYPE) { \
    if ((metricPTR)->count) { \
        u32 j = 0; \
        TYPE avg = TYPE##_INITIALIZER; \
        u64 modd = (metricPTR)->count % METRIC_MAX_ENTRY_COUNT; \
        u64 ub = !modd /*full*/ ? METRIC_MAX_ENTRY_COUNT : modd; \
        while (j < ub) { \
            avg += ((TYPE *)(metricPTR)->vals)[j++]; \
        } \
        avg = (avg / (TYPE) ub); \
        j = 0; \
        TYPE dev = 0; \
        while (j < ub) { \
            TYPE diff = (avg > ((TYPE *)(metricPTR)->vals)[j]) ? (avg - ((TYPE *)(metricPTR)->vals)[j]) : (((TYPE *)(metricPTR)->vals)[j] - avg); \
            dev += (diff * diff); \
            j++; \
        } \
        (metricPTR)->dev = (TYPE) metricsSqrt(dev/(TYPE)ub); \
        TYPE oldAvg = ((TYPE)(metricPTR)->avg); \
        (metricPTR)->avg = ((TYPE)(metricPTR)->avg) + avg; \
        if (oldAvg != 0) { \
            (metricPTR)->avg = ((TYPE)(metricPTR)->avg) / ((TYPE)2); \
        } \
    } \
}

#define RECORD_DETAILED(detailedMetricPtr, TYPE, VALUE, METRIC_DETAILED_KIND) { \
    TYPE tvalue = (TYPE) VALUE; \
    if (METRIC_DETAILED_KIND == METRIC_DETAILED_KIND_SMA) { \
    } else { \
        if ((detailedMetricPtr)->vals == NULL) { \
            ocrPolicyDomain_t *pd = NULL; \
            getCurrentEnv(&pd, NULL, NULL, NULL); \
            (detailedMetricPtr)->vals = pd->fcts.pdMalloc(pd, sizeof(TYPE)*METRIC_MAX_ENTRY_COUNT); \
        } \
        ((detailedMetricPtr)->vals)[(detailedMetricPtr)->count % METRIC_MAX_ENTRY_COUNT] = tvalue; \
        (detailedMetricPtr)->count++; \
        if (tvalue < ((TYPE)(detailedMetricPtr)->min)) { \
            (detailedMetricPtr)->min = tvalue; \
        } \
        if (tvalue > ((TYPE)(detailedMetricPtr)->max)) { \
            (detailedMetricPtr)->max = tvalue; \
        } \
        if (((detailedMetricPtr)->count % METRIC_MAX_ENTRY_COUNT) == 0) { \
            if (METRIC_MAX_ENTRY_COUNT_ERROR) { \
                ocrAssert(false && "Maximum of metric allowed reached"); \
            } else { \
                COMPUTE_AVG((detailedMetricPtr), TYPE); \
            } \
        } \
    } \
}

void recordDetailed(detailedMetric_t * metric, u64 value);

#define RECORD_COUNT(metricStorePtr, ENTITY, METRIC_NAME, VALUE) \
    if (ENABLE_##ENTITY##_METRICS && ENABLE_##METRIC_NAME) { \
      counterMetric_t * metric = &(((ENTITY##_MetricStore_t *) (metricStorePtr))->COUNTER_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME)]); \
      metric->value += VALUE; \
    }

// Record difference between two given value and accumulate
#define RECORD_DIFF(metricStorePtr, ENTITY, METRIC_NAME, V0, V1) \
    if (ENABLE_##ENTITY##_METRICS && ENABLE_##METRIC_NAME) { \
        diffMetric_t * metric = &(((ENTITY##_MetricStore_t *) (metricStorePtr))->DIFF_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME)]); \
        metric->value += (V1 - V0); \
        if (ENABLE_##METRIC_NAME##_DETAILED) { \
            detailedMetric_t * dMetric = &(((ENTITY##_MetricStore_t *) (metricStorePtr))->DETAILED_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME##_DETAILED)]); \
            recordDetailed(dMetric, (V1 - V0)); \
        } \
    } \

#define RECORD_RATIO(metricStorePtr, ENTITY, METRIC_NAME, VALUE) \
    if (ENABLE_##ENTITY##_METRICS && ENABLE_##METRIC_NAME) { \
        ratioMetric_t * metric = &(((ENTITY##_MetricStore_t *) (metricStorePtr))->RATIO_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME)]); \
        metric->value += (VALUE); \
        if (ENABLE_##METRIC_NAME##_DETAILED) { \
            detailedMetric_t * dMetric = &(((ENTITY##_MetricStore_t *) (metricStorePtr))->DETAILED_Metrics[METRIC_ID_AS_SLOT(METRIC_NAME##_DETAILED)]); \
            RECORD_DETAILED((dMetric), double, (VALUE), METRIC_DETAILED_KIND_AVG) \
        } \
    }

#if ENABLE_WORKER_METRICS
void dumpWorkerMetric(struct _ocrWorker_t * worker, WORKER_MetricStore_t * store);
#endif

#if ENABLE_EDT_METRICS
void dumpEdtMetric(struct _ocrWorker_t * worker, EDT_WORKER_AGG_MetricStore_t * store, u16 count);
#endif

#if (ENABLE_EDT_METRICS && ENABLE_WORKER_METRICS)
void aggregateEDTMetricStoreToWorker(EDT_MetricStore_t * metricStore);
#endif

#endif /* __METRICS_H__ */
