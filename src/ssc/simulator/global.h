#ifndef __SSC_GLOBAL_DATA_H__
#define __SSC_GLOBAL_DATA_H__

#include <coro.h>

#include <bl/base/allocator.h>
#include <bl/task_queue/task_queue.h>

#include <ssc/simulator/out_queue.h>

/*----------------------------------------------------------------------------*/
typedef struct ssc_global {
  coro_context                                  main_coro_ctx;
  bl_taskq*                                     tq;
  ssc_out_q                                     out_queue;
  void*                                         sim_context;
  ssc_sim_dealloc_signature                     sim_dealloc;
#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT
  ssc_sim_before_fiber_context_switch_signature sim_before_fiber_context_switch;
#endif
  bl_alloc_tbl const*                           alloc;
}
ssc_global;
/*----------------------------------------------------------------------------*/

#endif /* __SSC_GLOBAL_DATA_H__ */

