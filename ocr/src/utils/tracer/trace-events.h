 /*
  * This file is subject to the license agreement located in the file LICENSE
  * and cannot be distributed without it. This notice cannot be
  * removed or modified.
  */

#ifndef __TRACE_EVENTS_H__
#define __TRACE_EVENTS_H__

//Strings to identify user/runtime created objects
const char *evt_type[] = {
    "RUNTIME",
    "USER"
};

//Strings for traced OCR objects
const char *obj_type[] = {
    "EDT",
    "API_EDT",
    "EVENT",
    "API_EVENT",
    "DATABLOCK",
    "API_DATABLOCK",
    "MESSAGE",
    "WORKER",
    "SCHEDULER",
    "API_AFFINITY",
    "API_HINT",
};

//Strings for traced OCR events
const char *action_type[] = {
    "CREATE",
    "TEMPLATE_CREATE",
    "DESTROY",
    "RUNNABLE",
    "SCHEDULED",
    "ADD_DEP",
    "SATISFY",
    "EXECUTE",
    "FINISH",
    "DATA_ACQUIRE",
    "DATA_RELEASE",
    "END_TO_END",
    "WORK_REQUEST",
    "WORK_TAKEN",
    "SCHED_MSG_SEND",
    "SCHED_MSG_RCV",
    "SCHED_INVOKE",
    "GET_CURRENT",
    "GET_AT",
    "GET_COUNT",
    "QUERY",
    "INIT",
    "SET_VAL",
    "RANGE_CREATE"
};

#endif /* __TRACE_EVENTS_H__ */
