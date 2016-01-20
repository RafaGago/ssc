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
  uword                   max_func_count;
}
gsched_fiber_cfg;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fiber_wait_data {
  tstamp   execute_time;
  uword_d2 id;
}
gsched_fiber_wait_data;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fiber_queue_read_data {
  u8 const* match;
  u8 const* mask;
  u16       match_size;
  u16       mask_size;
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
  tstamp                    time;
  uword                     func_count;
  gsched_fiber_state_params params;
  uword_d2                  id;
}
gsched_fiber_state;
/*----------------------------------------------------------------------------*/
define_ringb_types (gsched_fiber_queue, u8*)
/*----------------------------------------------------------------------------*/
typedef struct gsched_fiber {
  gsched*            parent;
  coro_context       coro_ctx;
  struct coro_stack  stack;
  gsched_fiber_queue queue;
  gsched_fiber_cfg   cfg;
  gsched_fiber_state state;
}
gsched_fiber;
/*----------------------------------------------------------------------------*/
typedef struct gsched_fibers_node {
  tailq_entry (gsched_fibers_node) hook;
  gsched_fiber                     fiber;
}
gsched_fibers_node;
/*----------------------------------------------------------------------------*/
typedef tailq_head (gsched_fibers, gsched_fibers_node) gsched_fibers;
/*----------------------------------------------------------------------------*/
typedef struct gsched_wake_data {
  uword_d2 id;
  uword_d2 count;
}
gsched_wake_data;
/*----------------------------------------------------------------------------*/
typedef union gsched_timed_value {
  gsched_fibers_node* fn;
  gsched_wake_data    wake;
}
gsched_timed_value;
/*----------------------------------------------------------------------------*/
define_flat_deadlines_types (gsched_timed, gsched_timed_value)
/*----------------------------------------------------------------------------*/
typedef struct gsched_mainloop_vars {
  tstamp   now;
  u8*      unhandled_bstream;
  bool     has_prog;
  taskq_id prog_id;
  tstamp   prog_tstamp;
}
gsched_mainloop_vars;
/*----------------------------------------------------------------------------*/
typedef struct gsched {
  ssc_in_q              queue;
  gsched_timed          timed; /*state timeouts*/
  gsched_fibers         sq[3]; /*state queues*/
  gsched_timed          future_wakes;
  gsched_fibers         finished;
  ssc_fiber_cfgs const* fiber_cfgs;
  ssc_global*           global;
  ssc_group_id          gid;
  tstamp                look_ahead_offset;
  gsched_mainloop_vars  vars;
  uword                 active_fibers;
  u8*                   mem_chunk;
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
  alloc_tbl const*           alloc
  );
/*----------------------------------------------------------------------------*/
extern void gsched_destroy (gsched* gs, alloc_tbl const* alloc);
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
extern void ssc_api_wake (ssc_handle h, uword_d2 wait_id, uword_d2 count);
/*----------------------------------------------------------------------------*/
extern bool ssc_api_wait (ssc_handle h, uword_d2 wait_id, toffset us);
/*----------------------------------------------------------------------------*/
extern memr16 ssc_api_peek_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern memr16 ssc_api_timed_peek_input_head (ssc_handle h, toffset us);
/*----------------------------------------------------------------------------*/
extern memr16 ssc_api_try_peek_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern memr16 ssc_api_peek_input_head_match_mask(
  ssc_handle h, memr16 match, memr16 mask
  );
/*----------------------------------------------------------------------------*/
extern memr16 ssc_api_timed_peek_input_head_match_mask(
  ssc_handle h, memr16 match, memr16 mask, toffset us
  );
/*----------------------------------------------------------------------------*/
extern void ssc_api_drop_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_error(
  ssc_handle h, bl_err err, char const* static_string
  );
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_static_output (ssc_handle h, memr16 b);
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_dynamic_output (ssc_handle h, memr16 b);
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_static_string(
  ssc_handle h, char const* str, uword size_incl_trail_null
  );
/*----------------------------------------------------------------------------*/
extern void ssc_api_produce_dynamic_string(
  ssc_handle h, char const* str, uword size_incl_trail_null
  );
/*----------------------------------------------------------------------------*/
extern void ssc_api_delay (ssc_handle h, toffset us);
/*----------------------------------------------------------------------------*/
extern tstamp ssc_api_get_timestamp (ssc_handle h);
/*----------------------------------------------------------------------------*/
extern bool ssc_api_pattern_match (memr16 in, memr16 match);
/*----------------------------------------------------------------------------*/
extern bool ssc_api_pattern_match_mask (memr16 in, memr16 match, memr16 mask);
/*----------------------------------------------------------------------------*/
#endif /* __SSC_SCHED_H__ */
