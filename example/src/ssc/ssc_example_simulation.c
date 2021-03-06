
#include <string.h>
#include <assert.h>
#include <bl/base/default_allocator.h>
#include <ssc/simulation/simulation.h>

/*----------------------------------------------------------------------------*/
typedef struct fiber_setup_teardown_data {
  char* str;
}
fiber_setup_teardown_data;
/*----------------------------------------------------------------------------*/
typedef struct context {
  bl_uword                  timebase_us;
  bl_alloc_tbl              alloc;
  ssc_sem                   sem;
  fiber_setup_teardown_data setup_teardown_data;
}
context;
/*----------------------------------------------------------------------------*/
/* Produce */
/*----------------------------------------------------------------------------*/
void produce_error_fiber (ssc_handle h, void* fiber_context, void* sim_context)
{
  context* c = (context*) sim_context;
  while (true) {
    ssc_delay (h, c->timebase_us);
    ssc_produce_error (h, bl_mkok(), "bl_ok: custom string");
    /*fibers that don't read the input queue must release each input message
      reference count manually to allow resource deallocation*/
    ssc_drop_all_input (h);
  }
}
/*----------------------------------------------------------------------------*/
void produce_static_bytes_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const bl_u8 bytes[] = { 0, 1, 2, 3 };
  context* c = (context*) sim_context;
  ssc_set_fiber_as_produce_only (h);
  while (true) {
    ssc_delay (h, c->timebase_us);
    ssc_produce_static_output (h, bl_memr16_rv ((void*) bytes, sizeof bytes));
  }
}
/*----------------------------------------------------------------------------*/
void produce_dynamic_bytes_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const bl_u8 bytes[] = { 0, 1, 2, 3 };
  context* c = (context*) sim_context;
  ssc_set_fiber_as_produce_only (h);
  while (true) {
    ssc_delay (h, c->timebase_us);
    bl_u8* mem = (bl_u8*) bl_alloc (&c->alloc, sizeof bytes);
    if (!mem) {
      break;
    }
    memcpy (mem, bytes, sizeof bytes);/*the value could be different each time*/
    ssc_produce_dynamic_output (h, bl_memr16_rv ((void*) mem, sizeof bytes));
    /* we have lost "mem" ownership, the chunk may be deallocated by
       through "ssc_sim_dealloc" before arriving here, don't reuse "mem" */
  }
}
/*----------------------------------------------------------------------------*/
void produce_static_string_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const char str[] = "static string";
  context* c = (context*) sim_context;
  ssc_set_fiber_as_produce_only (h);
  while (true) {
    ssc_delay (h, c->timebase_us);
    ssc_produce_static_string (h, str, sizeof str);
  }
}
/*----------------------------------------------------------------------------*/
void produce_dynamic_string_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const char str[] = "dynamic string";
  context* c = (context*) sim_context;
  ssc_set_fiber_as_produce_only (h);
  while (true) {
    ssc_delay (h, c->timebase_us);
    char* mem = (char*) bl_alloc (&c->alloc, sizeof str);
    if (!mem) {
      break;
    }
    memcpy (mem, str, sizeof str);/*the value could be different each time*/
    ssc_produce_dynamic_string (h, mem, sizeof str);
    /* we have lost "mem" ownership, the chunk may be deallocated by
       through "ssc_sim_dealloc" before arriving here, don't reuse "mem" */
  }
}
/*----------------------------------------------------------------------------*/
/* Consume */
/*----------------------------------------------------------------------------*/
void try_peek_input_head_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const char str[] = "I answer to everything: non-blocking";
  context* c = (context*) sim_context;
  while (true) {
    bl_memr16 in = ssc_try_peek_input_head (h);
    if (!bl_memr16_is_null (in)) {
      ssc_drop_input_head (h);
      ssc_produce_static_string (h, str, sizeof str);
    }
    else {
      /*This is a cooperative scheduler that can't preempt the taks, so we have
        to return control ourselves, ssc_yield would suffice. Not doing so would
        just run in a never ending loop with all the other tasks starved.*/
      ssc_delay (h, 10000);
    }
  }
}
/*----------------------------------------------------------------------------*/
void peek_input_head_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const char str[] = "I answer to everything";
  context* c = (context*) sim_context;
  while (true) {
    bl_memr16 in = ssc_peek_input_head (h);
    assert (!bl_memr16_is_null (in));
    ssc_drop_input_head (h);
    ssc_produce_static_string (h, str, sizeof str);
  }
}
/*----------------------------------------------------------------------------*/
void peek_input_head_match_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const bl_u8 match[] = { 1, 1 };
  static const char str[] = "I answer to 0101";
  context* c = (context*) sim_context;
  while (true) {
    bl_memr16 in = ssc_peek_input_head_match(
      h, bl_memr16_rv ((void*) match, sizeof match)
      );
    assert (!bl_memr16_is_null (in));
    ssc_drop_input_head (h);
    ssc_produce_static_string (h, str, sizeof str);
  }
}
/*----------------------------------------------------------------------------*/
void peek_input_head_match_mask_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const bl_u8 match[] = { 0x01, 0x01 };
  static const bl_u8 mask[]  = { 0xff, 0x01 };
  static const char str[] = "I answer to 01 + odd bytes";
  context* c = (context*) sim_context;
  while (true) {
    bl_memr16 in = ssc_peek_input_head_match_mask(
      h,
      bl_memr16_rv ((void*) match, sizeof match),
      bl_memr16_rv ((void*) mask, sizeof mask)
      );
    assert (!bl_memr16_is_null (in));
    ssc_drop_input_head (h);
    ssc_produce_static_string (h, str, sizeof str);
  }
}
/*----------------------------------------------------------------------------*/
/* Timed Consume */
/*----------------------------------------------------------------------------*/
void timed_peek_input_head_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const char str[]  = "I answer to everything w timeout";
  static const char str2[] = "I answer to everything w timeout: timed out";
  context* c = (context*) sim_context;
  while (true) {
    bl_memr16 in = ssc_timed_peek_input_head (h, c->timebase_us);
    if (!bl_memr16_is_null (in)) {
      ssc_drop_input_head (h);
      ssc_produce_static_string (h, str, sizeof str);
    }
    else {
      ssc_produce_static_string (h, str2, sizeof str2);
    }
  }
}
/*----------------------------------------------------------------------------*/
void timed_peek_input_head_match_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const bl_u8 match[]  = { 2, 2 };
  static const char str[]  = "I answer to 0202 or I time out";
  static const char str2[] = "I answer to 0202 or I time out: timed out";
  context* c = (context*) sim_context;
  while (true) {
    bl_memr16 in = ssc_timed_peek_input_head_match(
      h, bl_memr16_rv ((void*) match, sizeof match), c->timebase_us
      );
    if (!bl_memr16_is_null (in)) {
      ssc_drop_input_head (h);
      ssc_produce_static_string (h, str, sizeof str);
    }
    else {
      ssc_produce_static_string (h, str2, sizeof str2);
    }
  }
}
/*----------------------------------------------------------------------------*/
void timed_peek_input_head_match_mask_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const bl_u8 match[]  = { 0x02, 0x00 };
  static const bl_u8 mask[]   = { 0xff, 0x01 };
  static const char str[]  = "I answer to 02 + even numbers or I time out";
  static const char str2[] =
    "I answer to 02 + even numbers or I time out: timed out";
  context* c = (context*) sim_context;
  while (true) {
    bl_memr16 in = ssc_timed_peek_input_head_match_mask(
      h,
      bl_memr16_rv ((void*) match, sizeof match),
      bl_memr16_rv ((void*) mask, sizeof mask),
      c->timebase_us
      );
    if (!bl_memr16_is_null (in)) {
      ssc_drop_input_head (h);
      ssc_produce_static_string (h, str, sizeof str);
    }
    else {
      ssc_produce_static_string (h, str2, sizeof str2);
    }
  }
}
/*----------------------------------------------------------------------------*/
/* Timestamps */
/*----------------------------------------------------------------------------*/
void timestamp_fiber (ssc_handle h, void* fiber_context, void* sim_context)
{
  static const char str[] = "bl_deadline expired";
  context* c = (context*) sim_context;
  ssc_set_fiber_as_produce_only (h);
  /* don't use bl_timept32_get() inside fibers, use ssc_get_timestamp() instead.
     The current fiber time can be slightly in the past or in the future */
  bl_timept32 bl_deadline = ssc_get_timestamp (h);
  while (true) {
    bl_timept32 now = ssc_get_timestamp (h);
    if (bl_timept32_get_diff (now, bl_deadline) >= 0) {
      ssc_produce_static_string (h, str, sizeof str);
      bl_deadline = now + bl_usec_to_timept32 (c->timebase_us);
    }
    ssc_delay (h, 100000);
  }
}
/*----------------------------------------------------------------------------*/
/* Semaphore */
/*----------------------------------------------------------------------------*/
void sem_wake_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const bl_u8 match[]  = { 0xff, 0xff };
  static const char str[]  = "ffff received: signaling semaphore";
  context* c = (context*) sim_context;
  while (true) {
    bl_memr16 in = ssc_peek_input_head_match(
      h, bl_memr16_rv ((void*) match, sizeof match)
      );
    assert (!bl_memr16_is_null (in));
    ssc_drop_input_head (h);
    ssc_produce_static_string (h, str, sizeof str);
    ssc_sem_wake (&c->sem, h, 1);
  }
}
/*----------------------------------------------------------------------------*/
void sem_wait_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  static const char str[]  = "semaphore signal received";
  context* c = (context*) sim_context;
  ssc_set_fiber_as_produce_only (h);
  while (true) {
    bool signaled = ssc_sem_wait (&c->sem, h, 100000);
    if (signaled) {
      ssc_produce_static_string (h, str, sizeof str);
    }
    /*fibers that don't read the input queue must release each input message
      reference count manually to allow resource deallocation*/
  }
}
/*----------------------------------------------------------------------------*/
/* Fiber with setup + teardown functions */
/*----------------------------------------------------------------------------*/
bl_err fiber_setup_func (void* fiber_context, void* sim_context)
{
#define FIBER_SETUP_TEARDOWN_LIT "this fiber has setup teardown"
  context* c = (context*) sim_context;
  fiber_setup_teardown_data* fd = (fiber_setup_teardown_data*) fiber_context;
  fd->str = bl_alloc (&c->alloc, sizeof FIBER_SETUP_TEARDOWN_LIT);
  if (!fd->str) {
    return bl_mkerr (bl_alloc);
  }
  memcpy (fd->str, FIBER_SETUP_TEARDOWN_LIT, sizeof FIBER_SETUP_TEARDOWN_LIT);
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
void fiber_teardown_func (void* fiber_context, void* sim_context)
{
  context* c = (context*) sim_context;
  fiber_setup_teardown_data* fd = (fiber_setup_teardown_data*) fiber_context;
  bl_dealloc (&c->alloc, fd->str);
}
/*----------------------------------------------------------------------------*/
void setup_and_teardown_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  context* c = (context*) sim_context;
  fiber_setup_teardown_data* fd = (fiber_setup_teardown_data*) fiber_context;
  /* the string is dynamic, but we don't want it to be deallocated by
    "ssc_sim_dealloc"*/
  ssc_produce_static_string (h, fd->str, sizeof FIBER_SETUP_TEARDOWN_LIT);
}
/*----------------------------------------------------------------------------*/
/* Config */
/*----------------------------------------------------------------------------*/
bl_err ssc_sim_on_setup(
    ssc_handle h, void* simlib_passed_data, void** sim_context
    )
{

  context* c = (context*) malloc (sizeof *c);
  if (!c) {
    return bl_mkerr (bl_alloc);
  }
  c->timebase_us = *((bl_uword*) simlib_passed_data);
  c->alloc       = bl_get_default_alloc();
  ssc_sem_init (&c->sem, 0); /* the id can be any number, just be sure to
                                don't init two semaphores with the same id. */
  *sim_context = c; /*exporting the new context*/

  ssc_fiber_cfg cfg;
  bl_err        err;
  cfg = ssc_fiber_cfg_rv (0, nullptr, nullptr, nullptr, nullptr);

  cfg.fiber = produce_error_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = produce_static_bytes_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = produce_dynamic_bytes_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = produce_static_string_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = produce_dynamic_string_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = try_peek_input_head_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = peek_input_head_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = peek_input_head_match_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = peek_input_head_match_mask_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = timed_peek_input_head_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = timed_peek_input_head_match_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = timed_peek_input_head_match_mask_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = timestamp_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = sem_wake_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber = sem_wait_fiber;
  err       = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  cfg.fiber         = setup_and_teardown_fiber;
  cfg.setup         = fiber_setup_func;
  cfg.teardown      = fiber_teardown_func;
  cfg.fiber_context = &c->setup_teardown_data;
  err               = ssc_add_fiber (h, &cfg);
  if (err.own) { return err; }

  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
void ssc_sim_on_teardown (void* sim_context)
{
  free (sim_context);
}
/*----------------------------------------------------------------------------*/
void ssc_sim_dealloc(
  void const* mem, bl_uword size, ssc_group_id id, void* sim_context
  )
{
  #define SSC_EXAMPLE_PRINTT_DEALLOCATED
  /* all memory passed through the "produce_xxx_dynamic" calls will be (if the
     user doesn't leak it) deallocated here at some point. This function has
     to be reentrant (thread-safe) */
  context* c = (context*) sim_context;
  bl_dealloc (&c->alloc, mem);
}
/*----------------------------------------------------------------------------*/
