#ifndef __SSC_SIMULATION_API_IMPL_H__
#define __SSC_SIMULATION_API_IMPL_H__

/*this file is actually a part of simulation.h hidden here because it has no
  relevance to show the user interface and would make the interface less
  readable (has some implementation details)
*/

#ifdef SSC_SHAREDLIB
/*----------------------------------------------------------------------------*/
extern ssc_simulator_ftable ssc_sim_tbl;
#define SSC_API_INVOKE_PRIV(name) ssc_sim_tbl.name
/*----------------------------------------------------------------------------*/
#else /*SSC_SHAREDLIB*/
/*----------------------------------------------------------------------------*/
#define SSC_API_INVOKE_PRIV(name) ssc_api_##name
extern bl_err ssc_api_add_fiber (ssc_handle h, ssc_fiber_cfg const* cfg);
extern void ssc_api_yield (ssc_handle h);
extern void ssc_api_wake (ssc_handle h, uword_d2 wait_id, uword_d2 count);
extern bool ssc_api_wait (ssc_handle h, uword_d2 wait_id, toffset us);
extern memr16 ssc_api_peek_input_head (ssc_handle h);
extern memr16 ssc_api_timed_peek_input_head (ssc_handle h, toffset us);
extern memr16 ssc_api_try_peek_input_head (ssc_handle h);
extern void ssc_api_drop_input_head (ssc_handle h);
extern void ssc_api_drop_all_input (ssc_handle h);
extern void ssc_api_delay (ssc_handle h, toffset us);
extern tstamp ssc_api_get_timestamp (ssc_handle h);
extern void ssc_api_produce_static_output (ssc_handle h, memr16 o);
extern void ssc_api_produce_dynamic_output (ssc_handle h, memr16 o);
extern void ssc_api_produce_error(
  ssc_handle h, bl_err err, char const* static_string
  );
extern void ssc_api_produce_static_string(
  ssc_handle h, char const* str, uword size_incl_trail_null
  );
extern void ssc_api_produce_dynamic_string(
  ssc_handle h, char const* str, uword size_incl_trail_null
  );
extern memr16 ssc_api_peek_input_head_match_mask(
  ssc_handle h, memr16 match, memr16 mask
  );
extern memr16 ssc_api_timed_peek_input_head_match_mask(
  ssc_handle h, memr16 match, memr16 mask, toffset us
  );
extern ssc_fiber_run_cfg ssc_api_fiber_get_run_cfg (ssc_handle h);
extern bl_err ssc_api_fiber_set_run_cfg(
  ssc_handle h, ssc_fiber_run_cfg const* c
  );
/*----------------------------------------------------------------------------*/
#endif /*SSC_SHAREDLIB*/

/*----------------------------------------------------------------------------*/
static inline bl_err ssc_add_fiber (ssc_handle h, ssc_fiber_cfg const* cfg)
{
  return SSC_API_INVOKE_PRIV (add_fiber) (h, cfg);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_yield (ssc_handle h)
{
  SSC_API_INVOKE_PRIV (yield) (h);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_wake (ssc_handle h, uword_d2 wait_id, uword_d2 count)
{
  SSC_API_INVOKE_PRIV (wake) (h, wait_id, count);
}
/*----------------------------------------------------------------------------*/
static inline bool ssc_wait (ssc_handle h, uword_d2 wait_id, toffset us)
{
  return SSC_API_INVOKE_PRIV (wait) (h, wait_id, us);
}
/*----------------------------------------------------------------------------*/
static inline memr16 ssc_peek_input_head (ssc_handle h)
{
  return SSC_API_INVOKE_PRIV (peek_input_head) (h);
}
/*----------------------------------------------------------------------------*/
static inline memr16 ssc_timed_peek_input_head (ssc_handle h, toffset us)
{
  return SSC_API_INVOKE_PRIV (timed_peek_input_head) (h, us);
}
/*----------------------------------------------------------------------------*/
static inline memr16 ssc_try_peek_input_head (ssc_handle h)
{
  return SSC_API_INVOKE_PRIV (try_peek_input_head) (h);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_drop_input_head (ssc_handle h)
{
  SSC_API_INVOKE_PRIV (drop_input_head) (h);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_drop_all_input (ssc_handle h)
{
  SSC_API_INVOKE_PRIV (drop_input_head) (h);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_delay (ssc_handle h, toffset us)
{
  SSC_API_INVOKE_PRIV (delay) (h, us);
}
/*----------------------------------------------------------------------------*/
static inline tstamp ssc_get_timestamp (ssc_handle h)
{
  return SSC_API_INVOKE_PRIV (get_timestamp) (h);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_static_output (ssc_handle h, memr16 o)
{
  SSC_API_INVOKE_PRIV (produce_static_output) (h, o);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_dynamic_output (ssc_handle h, memr16 o)
{
  SSC_API_INVOKE_PRIV (produce_dynamic_output) (h, o);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_error(
  ssc_handle h, bl_err err, char const* static_string
  )
{
  SSC_API_INVOKE_PRIV (produce_error) (h, err, static_string);
}
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_static_string(
  ssc_handle h, char const* str, uword size_incl_trail_null
  )
{
  SSC_API_INVOKE_PRIV (produce_static_string)(
    h, str, size_incl_trail_null
    );
}
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_dynamic_string(
  ssc_handle h, char const* str, uword size_incl_trail_null
  )
{
  SSC_API_INVOKE_PRIV (produce_dynamic_string)(
    h, str, size_incl_trail_null
    );
}
/*----------------------------------------------------------------------------*/
static inline memr16 ssc_peek_input_head_match_mask(
  ssc_handle h, memr16 match, memr16 mask
  )
{
  return SSC_API_INVOKE_PRIV (peek_input_head_match_mask) (h, match, mask);
}
/*----------------------------------------------------------------------------*/
static inline memr16 ssc_timed_peek_input_head_match_mask(
  ssc_handle h, memr16 match, memr16 mask, toffset us
  )
{
  return SSC_API_INVOKE_PRIV (timed_peek_input_head_match_mask)(
    h, match, mask, us
    );
}
/*----------------------------------------------------------------------------*/
static inline ssc_fiber_run_cfg ssc_fiber_get_run_cfg (ssc_handle h)
{
  return SSC_API_INVOKE_PRIV (fiber_get_run_cfg) (h);
}
/*----------------------------------------------------------------------------*/
static inline bl_err ssc_fiber_set_run_cfg(
  ssc_handle h, ssc_fiber_run_cfg const* c
  )
{
  return SSC_API_INVOKE_PRIV (fiber_set_run_cfg) (h, c);
}
/*----------------------------------------------------------------------------*/
#endif /* __SSC_SIMULATION_API_IMPL_H__ */

