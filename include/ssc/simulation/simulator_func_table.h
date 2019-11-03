#ifndef __SIMULATOR_FUNC_TABLE_H__
#define __SIMULATOR_FUNC_TABLE_H__

#include <bl/base/memory_range.h>
#include <bl/base/error.h>
#include <bl/base/time.h>

#include <ssc/types.h>

typedef struct ssc_simulator_ftable {
  /*SETUP TIME FUNCS ---------------------------------------------------------*/
  bl_err (*add_fiber)(
    ssc_handle h, ssc_fiber_cfg const* cfg
    );
  /*FIBER FUNCS---------------------------------------------------------------*/
  void      (*yield)                  (ssc_handle h);
  void      (*wake)(
    ssc_handle h, bl_uword_d2 id, bl_uword_d2 cnt
    );
  bool      (*wait)                   (
    ssc_handle h, bl_uword_d2 id, bl_timeoft32 us
    );
  bl_memr16 (*peek_input_head)        (ssc_handle h);
  bl_memr16 (*timed_peek_input_head)  (ssc_handle h, bl_timeoft32 us);
  bl_memr16 (*try_peek_input_head)    (ssc_handle h);
  void      (*drop_input_head)        (ssc_handle h);
  void      (*drop_all_input)         (ssc_handle h);
  void      (*delay)                  (ssc_handle h, bl_timeoft32 us);
  bl_timept32 (*get_timestamp)          (ssc_handle h);
  void      (*produce_static_output)  (ssc_handle h, bl_memr16 o);
  void      (*produce_dynamic_output) (ssc_handle h, bl_memr16 o);
  void      (*produce_error)(
    ssc_handle h, bl_err err, char const* static_string
    );
  void      (*produce_static_string)(
    ssc_handle h, char const* str, bl_uword size_incl_trail_null
    );
  void      (*produce_dynamic_string)(
    ssc_handle h, char const* str, bl_uword size_incl_trail_null
    );
  bl_memr16 (*consume_input_head_match_mask)(
    ssc_handle h, bl_memr16 match, bl_memr16 mask
    );
  bl_memr16 (*timed_consume_input_head_match_mask)(
    ssc_handle h, bl_memr16 match, bl_memr16 mask, bl_timeoft32 us
    );
  ssc_fiber_run_cfg (*fiber_get_run_cfg) (ssc_handle h);
  bl_err     (*fiber_set_run_cfg) (ssc_handle h, ssc_fiber_run_cfg const* c);
  /*--------------------------------------------------------------------------*/
}
ssc_simulator_ftable;

#endif /* __SIMULATOR_FUNC_TABLE_H__ */

