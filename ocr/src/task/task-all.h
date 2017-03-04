/**
 * @brief OCR tasks
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __TASK_ALL_H__
#define __TASK_ALL_H__

#include "debug.h"
#include "ocr-config.h"
#include "ocr-task.h"
#include "utils/ocr-utils.h"

typedef enum _taskType_t {
#ifdef ENABLE_TASK_HC
    taskHc_id,
#endif
    taskMax_id
} taskType_t;

typedef enum _taskTemplateType_t {
#ifdef ENABLE_TASKTEMPLATE_HC
    taskTemplateHc_id,
#endif
    taskTemplateMax_id
} taskTemplateType_t;

extern const char * task_types[];

extern const char * taskTemplate_types[];

#ifdef ENABLE_TASK_HC
#include "task/hc/hc-task.h"
#endif

ocrTaskFactory_t *newTaskFactory(taskType_t type, ocrParamList_t *typeArg);

ocrTaskTemplateFactory_t *newTaskTemplateFactory(taskTemplateType_t type, ocrParamList_t *typeArg);

#endif /* __TASK_ALL_H__ */
