#include "ocr.h"

#ifdef ENABLE_EXTENSION_RTITF
#include "extensions/ocr-runtime-itf.h"
/**
 * DESC: Test extension APIs to query current EDT
 */


ocrGuid_t checker(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t *depv) {
  ocrGuid_t queryGuid;
  ASSERT(!ocrCurrentEdtGet(&queryGuid) && !ocrGuidIsNull(queryGuid));
  ASSERT(!ocrCurrentEdtOutputGet(&queryGuid) && ocrGuidIsNull(queryGuid));
  PRINTF("Checks out ok\n");
  ocrShutdown();
  return NULL_GUID;
}

ocrGuid_t creator(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t *depv) {
  ocrGuid_t queryGuid;
  ocrGuid_t *input = (ocrGuid_t *)depv[0].ptr;

  ASSERT(!ocrCurrentEdtGet(&queryGuid) && !ocrGuidIsNull(queryGuid));
  ASSERT(!ocrCurrentEdtOutputGet(&queryGuid) && !ocrGuidIsNull(queryGuid));
  ASSERT(ocrGuidIsEq(queryGuid, *input));

  ocrDbDestroy(depv[0].guid);
  return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t *depv) {
  ocrGuid_t creator_template;
  ocrGuid_t creator_edt;
  ocrGuid_t creator_done_event;
  // The ready events ensure the edts don't run before we
  // can add a dependence to its output event
  ocrGuid_t ready_event, ready2_event;
  ocrEventCreate(&ready_event,OCR_EVENT_ONCE_T, EVT_PROP_NONE);
  ocrEventCreate(&ready2_event,OCR_EVENT_ONCE_T, EVT_PROP_NONE);
  ocrEdtTemplateCreate(&creator_template, creator, 0, 1);
  ocrEdtCreate(&creator_edt, creator_template, 0, NULL, 1, &ready_event,
	       EDT_PROP_NONE, NULL_HINT, &creator_done_event);
  ocrAddDependence(creator_done_event, ready2_event, 0, DB_MODE_NULL);

  ocrGuid_t dbGuid;
  ocrGuid_t *ptr;
  ocrDbCreate(&dbGuid, (void **)&ptr, sizeof(ocrGuid_t),
                     DB_PROP_NONE, NULL_HINT, NO_ALLOC);
  *ptr = creator_done_event;

  ocrGuid_t checker_template, checker_edt;
  ocrEdtTemplateCreate(&checker_template, checker, 0, 1);
  ocrEdtCreate(&checker_edt, checker_template, 0, NULL, 1, &ready2_event,
	       EDT_PROP_NONE, NULL_HINT, NULL);

  ocrEventSatisfy(ready_event, dbGuid);
  return NULL_GUID;
}
#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("No RT API\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif // ENABLE_EXTENSION_RTITF
