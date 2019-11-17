#include <string.h>

#include <bl/base/utility.h>
#include <bl/base/time.h>

#include <ssc/simulation/simulation.h>
#include <ssc/simulator/simulator.h>
#include <ssc/simulator/in_bstream.h>

#include <ssc/simulation_environment.h>

#include <ssc/cmocka_pre.h>

/*A quick check that the timestamp calculations aren't very broken by
  sleeping the thread*/
/*---------------------------------------------------------------------------*/
typedef struct sim_dealloc_data {
  bl_uword     count;
  void const*  mem;
  bl_uword     size;
  ssc_group_id id;
}
sim_dealloc_data;
/*---------------------------------------------------------------------------*/
typedef struct basic_tests_ctx {
  ssc*             sim;
  bl_uword         fsetup_count;
  bl_uword         fteardown_count;
  bl_uword         teardown_count;
  sim_dealloc_data dealloc;
  bl_timeoft32       bl_timept32_diff;
}
basic_tests_ctx;
/*---------------------------------------------------------------------------*/
/*TRANSLATION UNIT GLOBALS*/
/*---------------------------------------------------------------------------*/
static const bl_u8    fiber_match           = 0xdd;
static const bl_u8    fiber_resp            = 0xee;
static const bl_uword queue_timeout_us      = 1000;
static const bl_uword queue_timeout_long_us = 150000;
/*---------------------------------------------------------------------------*/
static basic_tests_ctx g_ctx;
static sim_env         g_env;
/*---------------------------------------------------------------------------*/
/*SIMULATION*/
/*---------------------------------------------------------------------------*/
static bl_err test_fiber_setup (void* fiber_context, void* sim_context)
{
  sim_env* env = (sim_env*) sim_context;
  basic_tests_ctx* ctx = (basic_tests_ctx*) env->ctx;
  ++ctx->fsetup_count;
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == env->ctx);
  assert_true (fiber_context == (void*) &g_ctx);
  return bl_mkok();
}
/*---------------------------------------------------------------------------*/
static void test_fiber_teardown (void* fiber_context, void* sim_context)
{
  sim_env* env         = (sim_env*) sim_context;
  basic_tests_ctx* ctx = (basic_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);
  ++ctx->fteardown_count;
}
/*---------------------------------------------------------------------------*/
static void fiber_to_test_the_queue(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  bl_memr16 match = bl_memr16_rv ((void*) &fiber_match, 1);
  while (1) {
    bl_memr16 in = ssc_peek_input_head_match (h, match);
    assert_true (!bl_memr16_is_null (in));
    assert_true (bl_memr16_size (in) == 1);
    assert_true (*bl_memr16_beg_as (in, bl_u8) == fiber_match);
    ssc_drop_input_head (h);
    /*this should be "static_output" but I want to test the dealloc func*/
    ssc_produce_dynamic_output (h, bl_memr16_rv ((void*) &fiber_resp, 1));
  }
}
/*---------------------------------------------------------------------------*/
static void fiber_to_test_queue_timeout(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  bl_memr16 match = bl_memr16_rv ((void*) &fiber_match, 1);
  bl_timept32 start = ssc_get_timestamp (h);
  bl_memr16 in = ssc_timed_peek_input_head_match(
    h, match, queue_timeout_us
    );
  bl_timept32 end  = ssc_get_timestamp (h);
  assert_true (bl_timept32_to_usec (end - start) >= queue_timeout_us);
  assert_true (bl_memr16_is_null (in));
  /*this should be "static_output" but I want to test the dealloc func*/
  ssc_produce_dynamic_output (h, bl_memr16_rv ((void*) &fiber_resp, 1));
}
/*---------------------------------------------------------------------------*/
static void fiber_to_test_queue_timeout_cancellation(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  bl_memr16 match = bl_memr16_rv ((void*) &fiber_match, 1);
  bl_memr16 in    = ssc_timed_peek_input_head_match(
    h, match, queue_timeout_long_us
    );
  assert_true (!bl_memr16_is_null (in));
  ssc_drop_input_head (h);
  ssc_produce_dynamic_output (h, bl_memr16_rv ((void*) &fiber_resp, 1));
  /*this should block on the queue forever, we are trying to see if this wakes
    up, so we block on the queue again indefinitely, this should cause the
    internals to be in a "blocked on queue" state*/
  in = ssc_peek_input_head_match (h, match);
  /*this should never be reached, if the fiber gets rescheduled is that the
    timeout on ssc_timed_peek_input_head_match wasn't successfully cancelled.
    Waking up can trigger assertions, as "ssc_peek_input_head_match" can't
    return a null. This is a bug regression.
    */
  ssc_produce_dynamic_output (h, bl_memr16_rv ((void*) &fiber_resp, 1));
}
/*---------------------------------------------------------------------------*/
static void fiber_to_test_delay(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  bl_timept32 start = ssc_get_timestamp (h);
  /*this will just increment the fiber time counter without context-switching*/
  ssc_delay (h, queue_timeout_us);
  bl_timept32 end  = ssc_get_timestamp (h);
  assert_true (bl_timept32_to_usec (end - start) >= queue_timeout_us);
  ssc_produce_dynamic_output (h, bl_memr16_rv ((void*) &fiber_resp, 1));
}
/*---------------------------------------------------------------------------*/
static void fiber_to_test_wait_timeout(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  bl_timept32 start = ssc_get_timestamp (h);
  bool unexpired = ssc_wait (h, 1, queue_timeout_us);
  assert_true (!unexpired);
  bl_timept32 end  = ssc_get_timestamp (h);
  assert_true (bl_timept32_to_usec (end - start) >= queue_timeout_us);
  ssc_produce_dynamic_output (h, bl_memr16_rv ((void*) &fiber_resp, 1));
}
/*---------------------------------------------------------------------------*/
static void sim_on_teardown_test (void* sim_context)
{
  assert_true (sim_context == (void*) &g_env);
  basic_tests_ctx* ctx = (basic_tests_ctx*) ((sim_env*) (sim_context))->ctx;
  ++ctx->teardown_count;
}
/*----------------------------------------------------------------------------*/
static void sim_dealloc_test(
  void const* mem, bl_uword size, ssc_group_id id, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);
  basic_tests_ctx* ctx = (basic_tests_ctx*) ((sim_env*) (sim_context))->ctx;
  ++ctx->dealloc.count;
  ctx->dealloc.mem  = mem;
  ctx->dealloc.size = size;
  ctx->dealloc.id   = id;
}
/*---------------------------------------------------------------------------*/
/*Tests*/
/*---------------------------------------------------------------------------*/
static void generic_test_setup(
  void **state, ssc_fiber_cfg* fibers, bl_uword fibers_count
  )
{
  memset (&g_ctx, 0, sizeof g_ctx);

  *state          = nullptr;
  g_env.cfg       = fibers;
  g_env.cfg_count = fibers_count;
  g_env.ctx       = &g_ctx; /*this will become sim_context*/
  g_env.dealloc   = sim_dealloc_test;
  g_env.teardown  = sim_on_teardown_test;

  bl_err err = ssc_create (&g_ctx.sim, "", &g_env);
  assert_true (!err.own);
  *state = (void*) &g_ctx;
}
/*---------------------------------------------------------------------------*/
/* Basic test */
/*---------------------------------------------------------------------------*/
static int queue_test_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0, fiber_to_test_the_queue, test_fiber_setup, test_fiber_teardown, &g_ctx
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static int queue_timeout_test_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0,
    fiber_to_test_queue_timeout,
    test_fiber_setup,
    test_fiber_teardown,
     &g_ctx
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static int queue_timeout_test_timeout_cancellation_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0,
    fiber_to_test_queue_timeout_cancellation,
    test_fiber_setup,
    test_fiber_teardown,
    &g_ctx
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static int delay_test_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0, fiber_to_test_delay, test_fiber_setup, test_fiber_teardown, &g_ctx
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static int test_teardown (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  if (!ctx) {
    return 1;
  }
  ssc_destroy (ctx->sim);
  return 0;
}
/*---------------------------------------------------------------------------*/
static void queue_no_match_test (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->fsetup_count == 1);

  bl_uword           count;
  ssc_output_data read;

  err = ssc_try_run_some (ctx->sim);
  assert_true (!err.own);
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err.own == bl_timeout);

  for (bl_uword i = 0; i < 5; ++i) {
    err = ssc_try_run_some (ctx->sim);
    assert_true (err.own == bl_nothing_to_do);
    err = ssc_read (ctx->sim, &count, &read, 1, 0);
    assert_true (err.own == bl_timeout);
  }
  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->fteardown_count == 1);
  assert_true (ctx->teardown_count == 1);
}
/*---------------------------------------------------------------------------*/
static void queue_match_test (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->fsetup_count == 1);

  err = ssc_try_run_some (ctx->sim);
  assert_true (!err.own);
  bl_uword count;
  ssc_output_data read;
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err.own == bl_timeout);

  /*writing non matching data*/
  bl_u8* send = ssc_alloc_write_bytestream (ctx->sim, 1);
  assert_non_null (send);

  *send = fiber_match - 1;
  err  = ssc_write (ctx->sim, 0, send, 1);
  assert (!err.own);

  do {
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err.own || err.own == bl_nothing_to_do);
  }
  while (err.own != bl_nothing_to_do);

  /*checking that it didn't match*/
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err.own == bl_timeout);

  /*writing matching data*/
  send = ssc_alloc_write_bytestream (ctx->sim, 1);
  assert_non_null (send);

  *send = fiber_match;
  err  = ssc_write (ctx->sim, 0, send, 1);
  assert (!err.own);

  do {
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err.own || err.own == bl_nothing_to_do);
  }
  while (err.own != bl_nothing_to_do);

  /*checking that it did match*/
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (!err.own);
  assert_true (read.type == ssc_type_dynamic_bytes);
  assert_true (read.gid == 0);
  bl_memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!bl_memr16_is_null (rd));
  assert_true (bl_memr16_size (rd) == 1);
  assert_true (*bl_memr16_beg_as (rd, bl_u8) == fiber_resp);

  ssc_dealloc_read_data (ctx->sim, &read);
  assert_true (ctx->dealloc.count == 1);
  assert_true (ctx->dealloc.mem == bl_memr16_beg (rd));
  assert_true (ctx->dealloc.size == 1);
  assert_true (ctx->dealloc.id == 0);

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->fteardown_count == 1);
  assert_true (ctx->teardown_count == 1);
}
/*---------------------------------------------------------------------------*/
static void answer_after_blocking_timeout_test (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->fsetup_count == 1);

  bl_uword count;
  ssc_output_data read;
  err = ssc_run_some (ctx->sim, queue_timeout_us * 20);
  assert_true (!err.own);
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err.own == bl_timeout);

  err = ssc_run_some (ctx->sim, queue_timeout_us * 20);
  assert_true (!err.own);

  /*checking that the fiber did actually time out and went out of scope*/
  err = ssc_read (ctx->sim, &count, &read, 1, queue_timeout_us * 20);
  assert_true (!err.own);
  assert_true (read.type == ssc_type_dynamic_bytes);
  assert_true (read.gid == 0);
  bl_memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!bl_memr16_is_null (rd));
  assert_true (bl_memr16_size (rd) == 1);
  assert_true (*bl_memr16_beg_as (rd, bl_u8) == fiber_resp);

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->teardown_count == 1);
}
/*---------------------------------------------------------------------------*/
static void timeout_correctly_cancelled_test (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->fsetup_count == 1);

  bl_uword count;
  ssc_output_data read;

  err = ssc_try_run_some (ctx->sim);
  assert_true (!err.own); /*fiber blocked on queue with timeout active*/

  /*writing matching data*/
  bl_u8* send = ssc_alloc_write_bytestream (ctx->sim, 1);
  assert_non_null (send);

  *send = fiber_match;
  err  = ssc_write (ctx->sim, 0, send, 1);
  assert (!err.own);

  /*checking that the fiber did actually receive the message an is blocked*/
  err = ssc_try_run_some (ctx->sim);
  assert_true (!err.own);
  /*fiber blocked on queue without timeout now, check message on the read queue
    to confirm*/
  err = ssc_read (ctx->sim, &count, &read, 1, queue_timeout_us * 20);
  assert_true (!err.own);
  assert_true (read.type == ssc_type_dynamic_bytes);
  assert_true (read.gid == 0);
  bl_memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!bl_memr16_is_null (rd));
  assert_true (bl_memr16_size (rd) == 1);
  assert_true (*bl_memr16_beg_as (rd, bl_u8) == fiber_resp);

  /*checking that the timeout is correctly cancelled, the assertion on the fiber
    shouldn't trigger*/
  bl_timept32 bl_deadline = bl_timept32_get() +
    (3 * bl_usec_to_timept32 (queue_timeout_long_us));

  do {
    err = ssc_run_some (ctx->sim, 10000);
    err = ssc_read (ctx->sim, &count, &read, 1, 0);
    assert_true (count == 0);
  }
  while (bl_timept32_get_diff (bl_timept32_get(), bl_deadline) <= 0);

  /*Success*/
  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->teardown_count == 1);
}
/*---------------------------------------------------------------------------*/
static void delay_test (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->fsetup_count == 1);

  bl_uword count;
  ssc_output_data read;

  err = ssc_run_some (ctx->sim, queue_timeout_us * 20);
  assert_true (!err.own);
  /*on this test the delays are just making forward progress without waiting
    (ahead of real time), so the content is already on the output queue with
    a time point on the future. We will succeed always, as the read
    timeout is "queue_timeout_us * 20". With a timeout of 0 this test would
    fail (given a regular OS scheduler time slice duration)*/
  err = ssc_read (ctx->sim, &count, &read, 1, queue_timeout_us * 20);
  assert_true (!err.own);
  assert_true (read.type == ssc_type_dynamic_bytes);
  assert_true (read.gid == 0);
  bl_memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!bl_memr16_is_null (rd));
  assert_true (bl_memr16_size (rd) == 1);
  assert_true (*bl_memr16_beg_as (rd, bl_u8) == fiber_resp);

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.own);
  assert_true (ctx->teardown_count == 1);
}
/*---------------------------------------------------------------------------*/
static const struct CMUnitTest tests[] = {
  cmocka_unit_test_setup_teardown(
    queue_no_match_test, queue_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    queue_match_test, queue_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    answer_after_blocking_timeout_test, queue_timeout_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    timeout_correctly_cancelled_test,
    queue_timeout_test_timeout_cancellation_setup,
    test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    delay_test, delay_test_setup, test_teardown
    ),
};
/*---------------------------------------------------------------------------*/
int basic_tests (void)
{
  return cmocka_run_group_tests (tests, nullptr, nullptr);
}
/*---------------------------------------------------------------------------*/

