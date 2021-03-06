#ifndef __SSC_SIMULATION_H__
#define __SSC_SIMULATION_H__

#include <ssc/types.h>
#include <ssc/simulation/libexport.h>
#include <ssc/simulation/simulator_func_table.h>

/*============================================================================*/
/* Simulation functions => to be implemented on the simulation */
/*============================================================================*/
/*----------------------------------------------------------------------------*/
/* ssc_sim_on_setup: Simulation initialization.

   "simlib_passed_data" is the "simlib_passed_data" void pointer passed to the
    simulator on the "ssc_create(...) function (<simulator.h>). It allows to
    share a context between the simulation and the simulator.

    "sim_context" allows to share a context between the simulation functions. It
    is forwareded to "ssc_sim_on_teardown" and "ssc_sim_on_dealloc". */
/*----------------------------------------------------------------------------*/
extern SSC_EXPORT
  bl_err ssc_sim_on_setup(
    ssc_handle h, void* simlib_passed_data, void** sim_context
    );
/*----------------------------------------------------------------------------*/
/* ssc_sim_on_setup: Simulation cleanup. */
/*----------------------------------------------------------------------------*/
extern SSC_EXPORT
  void ssc_sim_on_teardown (void* sim_context);
/*----------------------------------------------------------------------------*/
/* ssc_sim_dealloc: Deallocates memory that was passed by the simulation through
     "ssc_produce_dynamic_output(...)" or "ssc_produce_dynamic_string(...)".

   This function has to be thread-safe. */
/*----------------------------------------------------------------------------*/
extern SSC_EXPORT
  void ssc_sim_dealloc(
    void const* mem, bl_uword size, ssc_group_id id, void* sim_context
    );
/*----------------------------------------------------------------------------*/
/* ssc_sim_before_fiber_context_switch: This is called from the simulator
    each time just before context switching the currently running fiber.
    Useful for preparing some kind of environment to do a context-switch */
/*----------------------------------------------------------------------------*/
#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT
extern SSC_EXPORT
  void ssc_sim_before_fiber_context_switch (void* sim_context);
#endif /*#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT*/
/*----------------------------------------------------------------------------*/
#ifdef SSC_SHAREDLIB
/*----------------------------------------------------------------------------*/
/* ssc_sim_manual_link: defined on "simulation_src.h". It just has to be
   included in just one translation unit of the simulation when compiling as
   a shared library.*/
  /*----------------------------------------------------------------------------*/
extern SSC_EXPORT
  void ssc_sim_manual_link (ssc_simulator_ftable const* t);
#endif
/*============================================================================*/
/* Fiber creation: functions that can only be called inside "ssc_sim_on_setup"*/
/*============================================================================*/
/*----------------------------------------------------------------------------*/
/* ssc_add_fiber: */
/*----------------------------------------------------------------------------*/
static inline bl_err ssc_add_fiber (ssc_handle h, ssc_fiber_cfg const* cfg);
/*============================================================================*/
/* Fiber API: functions to be called inside the fiber function */
/*============================================================================*/
/*----------------------------------------------------------------------------*/
/* ssc_yield: yields the fiber time slice. */
/*----------------------------------------------------------------------------*/
static inline void ssc_yield (ssc_handle h);
/*----------------------------------------------------------------------------*/
/* ssc_wait: blocks a fiber until either a signal through "ssc_wake(...)" is
    received or "us" times out. When timing out the function will return
    false.

    "wait_id" is a number that has to be the same on ssc_wake and ssc_wait. A
    "wait" just responds to "wakes" with the same "id".*/
/*----------------------------------------------------------------------------*/
static inline bool ssc_wait (ssc_handle h, bl_uword_d2 wait_id, bl_timeoft32 us);
/*----------------------------------------------------------------------------*/
/* ssc_wake: wakes one or more fibers blocked on ssc_wait.

   "wait_id" is a number that has to be the same on ssc_wake and ssc_wait. A
    "wait" just responds to "wakes" with the same "id".*/
/*----------------------------------------------------------------------------*/
static inline void ssc_wake (ssc_handle h, bl_uword_d2 wait_id, bl_uword_d2 count);
/*----------------------------------------------------------------------------*/
/* ssc_delay: Puts a fiber to sleep*/
/*----------------------------------------------------------------------------*/
static inline void ssc_delay (ssc_handle h, bl_timeoft32 us);
/*----------------------------------------------------------------------------*/
static inline bl_timept32 ssc_get_timestamp (ssc_handle h);
/*----------------------------------------------------------------------------*/
/* ssc_produce_static_output: Sends bytes to the output queue (retrieved by
    ssc_read).

    The memory passed to this function is static and doesn't need deallocation.
*/
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_static_output (ssc_handle h, bl_memr16 o);
/*----------------------------------------------------------------------------*/
/* ssc_produce_dynamic_output: Sends bytes to the output queue (retrieved by
    ssc_read).

    The memory passed to this function is dynamic and will be deallocated by
    ssc_sim_dealloc(...).*/
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_dynamic_output (ssc_handle h, bl_memr16 o);
/*----------------------------------------------------------------------------*/
/* ssc_produce_error: Sends an error code to the output queue (retrieved by
    ssc_read).*/
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_error(
  ssc_handle h, bl_err err, char const* static_string
  );
/*----------------------------------------------------------------------------*/
/* ssc_produce_static_string: Sends a string to the output queue (retrieved by
    ssc_read).

    The memory passed to this function is static and doesn't need deallocation.
*/
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_static_string(
  ssc_handle h, char const* str, bl_uword size_incl_trail_null
  );
/*----------------------------------------------------------------------------*/
/* ssc_produce_dynamic_string: Sends a string to the output queue (retrieved by
    ssc_read).

    The memory passed to this function is dynamic and will be deallocated by
    ssc_sim_dealloc(...).*/
/*----------------------------------------------------------------------------*/
static inline void ssc_produce_dynamic_string(
  ssc_handle h, char const* str, bl_uword size_incl_trail_null
  );
/*----------------------------------------------------------------------------*/
/*ssc_peek_input_head: peeks the input queue blocking as long as it's necessary
    until data is available. The input queue head isn't consumed. */
/*----------------------------------------------------------------------------*/
static inline bl_memr16 ssc_peek_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
/*ssc_timed_peek_input_head: peeks the input queue blocking as until the timeout
    expires. The input queue head isn't consumed.. If "us" == 0 the call is
    non-blocking. */
/*----------------------------------------------------------------------------*/
static inline bl_memr16 ssc_timed_peek_input_head (ssc_handle h, bl_timeoft32 us);
/*----------------------------------------------------------------------------*/
/*ssc_try_peek_input_head: peeks the input queue without blocking. The input
    queue head isn't consumed. */
/*----------------------------------------------------------------------------*/
static inline bl_memr16 ssc_try_peek_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
/*ssc_drop_input_head: Drops (consumes) the input queue head without reading
    it.*/
/*----------------------------------------------------------------------------*/
static inline void ssc_drop_input_head (ssc_handle h);
/*----------------------------------------------------------------------------*/
/*ssc_drop_all_input: Drops (consumes) all the data present on the input queue
  without reading it.*/
/*----------------------------------------------------------------------------*/
static inline void ssc_drop_all_input (ssc_handle h);
/*----------------------------------------------------------------------------*/
/* match/match+mask variants of the functions above:

  The functions below consume the input queue head until a "match" is found.

  A match is defined as applying a bitwise AND to the input queue head and the
  "mask" parameter bytestreams and comparing the result against the "match"
  parameter. In a match the length of the input head bytestream is always bigger
  or equal than the length of the "match" parameter.

  If the "mask" bytestream is shorter that the "match" bytestream the "mask"
  bytestream is automatically expanded with the "0xff" value.

  If the "match" bytestream is shorter that the "mask" bytestream the exceeding
  bytes on the "mask" bytestream are ignored.
*/
/*----------------------------------------------------------------------------*/
static inline bl_memr16 ssc_peek_input_head_match_mask(
  ssc_handle h, bl_memr16 match, bl_memr16 mask
  );
/*----------------------------------------------------------------------------*/
static inline bl_memr16 ssc_timed_peek_input_head_match_mask(
  ssc_handle h, bl_memr16 match, bl_memr16 mask, bl_timeoft32 us
  );
/*----------------------------------------------------------------------------*/
static inline bl_memr16 ssc_peek_input_head_match (ssc_handle h, bl_memr16 match)
{
  return ssc_peek_input_head_match_mask (h, match, bl_memr16_null());
}
/*----------------------------------------------------------------------------*/
static inline bl_memr16 ssc_timed_peek_input_head_match(
  ssc_handle h, bl_memr16 match, bl_timeoft32 us
  )
{
  return ssc_timed_peek_input_head_match_mask (h, match, bl_memr16_null(), us);
}
/*----------------------------------------------------------------------------*/
/*ssc_fiber_get_run_cfg: Gets the fiber runtime parameters*/
/*----------------------------------------------------------------------------*/
static inline ssc_fiber_run_cfg ssc_fiber_get_run_cfg (ssc_handle h);
/*----------------------------------------------------------------------------*/
/*ssc_fiber_set_run_cfg: Sets the current fiber runtime parameters*/
/*----------------------------------------------------------------------------*/
static inline bl_err ssc_fiber_set_run_cfg(
  ssc_handle h, ssc_fiber_run_cfg const* c
  );
/*----------------------------------------------------------------------------*/
/*ssc_set_fiber_as_produce_only: Tells the simulator that the fiber isn't in-
  terested in reading data from the input queue.

  This is to be used by fibers that never call "ssc_drop_input_head" or
  "ssc_drop_all_input". Messages on the input queues aren't copied but reference
  counted, so having fibers without consuming them would never allow to release
  the messages.

  This call isn't undoable. After this call all "*peek_input*" functions will
  return nothing (but still use timers) and all the *_drop_input_* functions
  will do nothing.*/
/*----------------------------------------------------------------------------*/
static inline bl_err ssc_set_fiber_as_produce_only (ssc_handle h)
{
  ssc_fiber_run_cfg cfg = ssc_fiber_get_run_cfg (h);
  cfg.run_flags         = fiber_set_produce_only (cfg.run_flags);
  return ssc_fiber_set_run_cfg (h, &cfg);
}
/*----------------------------------------------------------------------------*/
/*ssc_set_fiber_as_real_time: Disallows the task scheduler to run the fiber
  "to the future", so the calls to "ssc_delay" always place the fiber in the
  schedulers wait queue instead of just advancing the time counter.

  Useful to preserve time coherency when some fiber are sharing global
  variables.
  */
/*----------------------------------------------------------------------------*/
static inline bl_err ssc_set_fiber_as_real_time (ssc_handle h)
{
  ssc_fiber_run_cfg cfg    = ssc_fiber_get_run_cfg (h);
  cfg.look_ahead_offset_us = 0;
  return ssc_fiber_set_run_cfg (h, &cfg);
}
/*----------------------------------------------------------------------------*/
/* ssc_sem: Simple semaphore built with "ssc_wait" and "ssc_wake". */
/*----------------------------------------------------------------------------*/
typedef struct ssc_sem {
  bl_word_d2  count;
  bl_uword_d2 id;
}
ssc_sem;
/*----------------------------------------------------------------------------*/
static inline void ssc_sem_init (ssc_sem* s, bl_uword_d2 id)
{
  s->count = 0;
  s->id    = id;
}
/*----------------------------------------------------------------------------*/
static inline void ssc_sem_wake (ssc_sem* s, ssc_handle h, bl_uword_d2 count)
{
  if (s->count < 1) {
    ssc_wake (h, s->id, count);
  }
  bl_assert (s->count < bl_itype_max (bl_word_d2));
  ++s->count;
}
/*----------------------------------------------------------------------------*/
static inline bool ssc_sem_wait (ssc_sem* s, ssc_handle h, bl_timeoft32 us)
{
  bl_assert (s->count > bl_itype_min (bl_word_d2));
  --s->count;
  bool unexpired = true;
  if (s->count < 0) {
    unexpired = ssc_wait (h, s->id, us);
  }
  return unexpired;
}
/*----------------------------------------------------------------------------*/
#include <ssc/simulation/simulation_api_impl.h>
/*----------------------------------------------------------------------------*/

#endif /* __SSC_SIMULATION_H__ */
