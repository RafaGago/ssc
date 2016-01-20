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
  void   (*yield)                  (ssc_handle h);
  void   (*wake)                   (ssc_handle h, uword_d2 id, uword_d2 count);
  bool   (*wait)                   (ssc_handle h, uword_d2 id, toffset us);
  memr16 (*peek_input_head)        (ssc_handle h);
  memr16 (*timed_peek_input_head)  (ssc_handle h, toffset us);
  memr16 (*try_peek_input_head)    (ssc_handle h);
  void   (*drop_input_head)        (ssc_handle h);
  void   (*delay)                  (ssc_handle h, toffset us);
  tstamp (*get_timestamp)          (ssc_handle h);  
  void   (*produce_static_output)  (ssc_handle h, memr16 o);
  void   (*produce_dynamic_output) (ssc_handle h, memr16 o);
  void   (*produce_error)(
    ssc_handle h, bl_err err, char const* static_string
    );
  void   (*produce_static_string)(
    ssc_handle h, char const* str, uword size_incl_trail_null
    );
  void   (*produce_dynamic_string)(
    ssc_handle h, char const* str, uword size_incl_trail_null
    );
  memr16 (*consume_input_head_match_mask)(
    ssc_handle h, memr16 match, memr16 mask
    );
  memr16 (*timed_consume_input_head_match_mask)(
    ssc_handle h, memr16 match, memr16 mask, toffset us
    );
  /*--------------------------------------------------------------------------*/
}
ssc_simulator_ftable;

#endif /* __SIMULATOR_FUNC_TABLE_H__ */

