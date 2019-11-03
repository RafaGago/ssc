#ifndef __SSC_GSCHED_H__
#define __SSC_GSCHED_H__

#include <coro.h>

#include <bl/base/platform.h>
#include <bl/base/error.h>
#include <bl/base/integer.h>
#include <bl/base/bsd_queue.h>
#include <bl/base/flat_deadlines.h>
#include <bl/base/ringbuffer.h>

#include <bl/task_queue/task_queue.h>

#include <ssc/types.h>
#include <ssc/simulator/in_queue.h>
#include <ssc/simulator/cfg.h>
#include <ssc/simulator/global.h>

/*----------------------------------------------------------------------------*/
typedef struct gsched gsched;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fiber_cfg {
  ssc_fiber_func          fiber;
  ssc_fiber_teardown_func teardown;
  void*                   context;
  ssc_fiber_run_cfg       run_cfg;
}
gsched_fiber_cfg;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fiber_wait_data {
  bl_timept32   execute_time;
  bl_uword_d2 id;
}
gsched_fiber_wait_data;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fiber_queue_read_data {
  bl_u8 const* match;
  bl_u8 const* mask;
  bl_u16       match_size;
  bl_u16       mask_size;
}
gsched_fiber_queue_read_data;
/*----------------------------------------------------------------------------*/
typedef union gsched_fiber_state_params {
  gsched_fiber_wait_data       wait;
  gsched_fiber_queue_read_data qread;
}
gsched_fiber_state_params;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fiber_state {
  bl_timept32                 time;
  bl_uword                  func_count;
  gsched_fiber_state_params params;
  bl_u8                     id;
}
gsched_fiber_state;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fiber {
  gsched*            parent;
  coro_context       coro_ctx;
  struct coro_stack  stack;
  bl_ringb           queue;
  gsched_fiber_cfg   cfg;
  gsched_fiber_state state;
}
gsched_fiber;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fibers_node {
  bl_tailq_entry (gsched_fibers_node) hook;
  gsched_fiber                        fiber;
}
gsched_fibers_node;
/*----------------------------------------------------------------------------*/
typedef bl_tailq_head (gsched_fibers, gsched_fibers_node) gsched_fibers;
/*----------------------------------------------------------------------------*/
typedef struct gsched_wake_data {
  bl_uword_d2 id;
  bl_uword_d2 count;
}
gsched_wake_data;
/*----------------------------------------------------------------------------*/
typedef union gsched_timed_value {
  gsched_fibers_node* fn;
  gsched_wake_data    wake;
}
gsched_timed_value;
/*----------------------------------------------------------------------------*/
typedef struct gsched_mainloop_vars {
  bl_timept32   now;
  bl_u8*      unhandled_bstream;
  bool        has_prog;
  bl_taskq_id prog_id;
  bl_timept32   prog_timept32;
}
gsched_mainloop_vars;
/*----------------------------------------------------------------------------*/
typedef struct gsched {
  ssc_in_q              queue;
  bl_flat_deadlines     timed; /*state timeouts*/
  gsched_fibers         sq[3]; /*state queues*/
  bl_flat_deadlines     future_wakes;
  gsched_fibers         finished;
  ssc_fiber_cfgs const* fiber_cfgs;
  ssc_global*           global;
  ssc_group_id          gid;
  bl_timept32             look_ahead_offset;
  gsched_mainloop_vars  vars;
  bl_uword              active_fibers;
  bl_uword              produce_only_fibers;
  bl_u8*                mem_chunk;
}
gsched;
/*----------------------------------------------------------------------------*/
/* FIBER SCHEDULER */
/*----------------------------------------------------------------------------*/
extern bl_err gsched_init(
  gsched*                    gs,
  ssc_group_id               id,
  ssc_global*                global,
  ssc_fiber_group_cfg const* fgroup_cfg,
  ssc_fiber_cfgs const*      fiber_cfgs,
  bl_alloc_tbl const*        alloc
  );
/*----------------------------------------------------------------------------*/
extern void gsched_destroy (gsched* gs, bl_alloc_tbl const* alloc);
/*----------------------------------------------------------------------------*/
extern bl_err gsched_run_setup (gsched* gs);
/*----------------------------------------------------------------------------*/
extern void gsched_run_teardown (gsched* gs);
/*----------------------------------------------------------------------------*/
extern bl_err gsched_program_schedule (gsched* gs);
/*----------------------------------------------------------------------------*/
extern bl_err gsched_fiber_cfg_validate_correct (ssc_fiber_cfg* cfg);
/*----------------------------------------------------------------------------*/
/* SIMULATION INTERFACE */
/*----------------------------------------------------------------------------*/
extern void ssc_api_yield (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern void ssc_api_wake (ssc_handle h, bl_uword_d2 wait_id, bl_uword_d2 count);
/*----------------------------------------------------------------------------*/
extern bool ssc_api_wait (ssc_handle h, bl_uword_d2 wait_id, bl_timeoft32 us);
/*----------------------------------------------------------------------------*/
extern bl_memr16 ssc_api_peek_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern bl_memr16 ssc_api_timed_peek_input_head (ssc_handle h, bl_timeoft32 us);
/*----------------------------------------------------------------------------*/
extern bl_memr16 ssc_api_try_peek_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern bl_memr16 ssc_api_peek_input_head_match_mask(
  ssc_handle h, bl_memr16 match, bl_memr16 mask
  );
/*----------------------------------------------------------------------------*/
extern bl_memr16 ssc_api_timed_peek_input_head_match_mask(
  ssc_handle h, bl_memr16 match, bl_memr16 mask, bl_timeoft32 us
  );
/*----------------------------------------------------------------------------*/
extern void ssc_api_drop_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern void ssc_api_drop_all_input (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_error(
  ssc_handle h, bl_err err, char const* static_string
  );
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_static_output (ssc_handle h, bl_memr16 b);
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_dynamic_output (ssc_handle h, bl_memr16 b);
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_static_string(
  ssc_handle h, char const* str, bl_uword size_incl_trail_null
  );
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_dynamic_string(
  ssc_handle h, char const* str, bl_uword size_incl_trail_null
  );
/*----------------------------------------------------------------------------*/
extern void ssc_api_delay (ssc_handle h, bl_timeoft32 us);
/*----------------------------------------------------------------------------*/
extern bl_timept32 ssc_api_get_timestamp (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern bool ssc_api_pattern_match (bl_memr16 in, bl_memr16 match);
/*----------------------------------------------------------------------------*/
extern bool ssc_api_pattern_match_mask (bl_memr16 in, bl_memr16 match, bl_memr16 mask);
/*----------------------------------------------------------------------------*/
extern ssc_fiber_run_cfg ssc_api_fiber_get_run_cfg (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern bl_err ssc_api_fiber_set_run_cfg(
  ssc_handle h, ssc_fiber_run_cfg const* c
  );
/*----------------------------------------------------------------------------*/
#endif /* __SSC_SCHED_H__ */
