/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "task/task-all.h"

const char * task_types [] = {
#ifdef ENABLE_TASK_HC
    "HC",
#endif
    NULL
};

const char * taskTemplate_types [] = {
#ifdef ENABLE_TASKTEMPLATE_HC
    "HC",
#endif
    NULL
};

ocrTaskFactory_t *newTaskFactory(taskType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_TASK_HC
    case taskHc_id:
        return newTaskFactoryHc(typeArg, typeArg->id);
#endif
    default:
        ASSERT(0);
    };
    return NULL;
}

ocrTaskTemplateFactory_t *newTaskTemplateFactory(taskTemplateType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_TASKTEMPLATE_HC
    case taskTemplateHc_id:
        return newTaskTemplateFactoryHc(typeArg, typeArg->id);
#endif
    default:
        ASSERT(0);
        return NULL;
    };
}
