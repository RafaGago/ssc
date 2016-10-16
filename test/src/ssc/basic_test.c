#include <string.h>

#include <bl/base/utility.h>
#include <bl/base/time.h>

#include <ssc/simulation/simulation.h>
#include <ssc/simulator/simulator.h>
#include <ssc/simulator/in_bstream.h>

#include <ssc/simulation_environment.h>

#include <ssc/cmocka_pre.h>

/*not needed*/
#include <stdio.h>

/*A quick check that the timestamp calculations aren't very broken by
  sleeping the thread*/
/*---------------------------------------------------------------------------*/
typedef struct sim_dealloc_data {
  uword          count;
  void const*    mem;
  uword          size;
  ssc_group_id id;
}
sim_dealloc_data;
/*---------------------------------------------------------------------------*/
typedef struct basic_tests_ctx {
  ssc*             sim;
  uword            fsetup_count;
  uword            fteardown_count;
  uword            teardown_count;
  sim_dealloc_data dealloc;
  toffset          tstamp_diff;
}
basic_tests_ctx;
/*---------------------------------------------------------------------------*/
/*TRANSLATION UNIT GLOBALS*/
/*---------------------------------------------------------------------------*/
static const void* fiber_context_const = (void*) 0xfe;
static const u8    fiber_match         = 0xdd;
static const u8    fiber_resp          = 0xee;
static const uword queue_timeout_us    = 1000;
/*---------------------------------------------------------------------------*/
static basic_tests_ctx g_ctx;
static sim_env         g_env;
/*---------------------------------------------------------------------------*/
/*SIMULATION*/
/*---------------------------------------------------------------------------*/
static bl_err test_fiber_setup (void* fiber_context, void* sim_context)
{
  sim_env* env         = (sim_env*) sim_context;
  basic_tests_ctx* ctx = (basic_tests_ctx*) env->ctx;
  ++ctx->fsetup_count;
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == env->ctx);
  assert_true (fiber_context == (void*) &g_ctx);
  return bl_ok;
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
static void queue_test_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env         = (sim_env*) sim_context;
  basic_tests_ctx* ctx = (basic_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  memr16 match = memr16_rv ((void*) &fiber_match, 1);
  while (1) {
    memr16 in = ssc_peek_input_head_match (h, match);
    assert_true (!memr16_is_null (in));
    assert_true (memr16_size (in) == 1);
    assert_true (*memr16_beg_as (in, u8) == fiber_match);
    ssc_drop_input_head (h);
    /*this should be "static_output" but I want to test the dealloc func*/
    ssc_produce_dynamic_output (h, memr16_rv ((void*) &fiber_resp, 1));
  }
}
/*---------------------------------------------------------------------------*/
static void queue_test_timeout_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env         = (sim_env*) sim_context;
  basic_tests_ctx* ctx = (basic_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  memr16 match = memr16_rv ((void*) &fiber_match, 1);
  tstamp start = ssc_get_timestamp (h);
  memr16 in = ssc_timed_peek_input_head_match(
    h, match, queue_timeout_us
    );
  tstamp end  = ssc_get_timestamp (h);
  assert_true (bl_tstamp_to_usec (end - start) >= queue_timeout_us);
  assert_true (memr16_is_null (in));
  /*this should be "static_output" but I want to test the dealloc func*/
  ssc_produce_dynamic_output (h, memr16_rv ((void*) &fiber_resp, 1));
}
/*---------------------------------------------------------------------------*/
static void test_delay_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env         = (sim_env*) sim_context;
  basic_tests_ctx* ctx = (basic_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  tstamp start = ssc_get_timestamp (h);
  /*this will just increment the fiber time counter without context-switching*/
  ssc_delay (h, queue_timeout_us);
  tstamp end  = ssc_get_timestamp (h);
  assert_true (bl_tstamp_to_usec (end - start) >= queue_timeout_us);
  ssc_produce_dynamic_output (h, memr16_rv ((void*) &fiber_resp, 1));
}
/*---------------------------------------------------------------------------*/
static void test_wait_timeout(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env         = (sim_env*) sim_context;
  basic_tests_ctx* ctx = (basic_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);
  assert_true (fiber_context == (void*) &g_ctx);

  tstamp start = ssc_get_timestamp (h);
  bool unexpired = ssc_wait (h, 1, queue_timeout_us);
  assert_true (!unexpired);
  tstamp end  = ssc_get_timestamp (h);
  assert_true (bl_tstamp_to_usec (end - start) >= queue_timeout_us);
  ssc_produce_dynamic_output (h, memr16_rv ((void*) &fiber_resp, 1));
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
  void const* mem, uword size, ssc_group_id id, void* sim_context
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
  void **state, ssc_fiber_cfg* fibers, uword fibers_count
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
  assert_true (!err);
  *state = (void*) &g_ctx;
}
/*---------------------------------------------------------------------------*/
/* Basic test */
/*---------------------------------------------------------------------------*/
static int queue_test_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0, queue_test_fiber, test_fiber_setup, test_fiber_teardown, &g_ctx
    );
  generic_test_setup (state, fibers, arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static int queue_timeout_test_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0, queue_test_timeout_fiber, test_fiber_setup, test_fiber_teardown, &g_ctx
    );
  generic_test_setup (state, fibers, arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static int delay_test_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0, test_delay_fiber, test_fiber_setup, test_fiber_teardown, &g_ctx
    );
  generic_test_setup (state, fibers, arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static int test_wait_timeout_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0, test_wait_timeout, test_fiber_setup, test_fiber_teardown, &g_ctx
    );
  generic_test_setup (state, fibers, arr_elems (fibers));
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
  assert_true (!err);
  assert_true (ctx->fsetup_count == 1);

  uword           count;
  ssc_output_data read;

  err = ssc_try_run_some (ctx->sim);
  assert_true (!err);
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err == bl_timeout);

  for (uword i = 0; i < 5; ++i) {
    err = ssc_try_run_some (ctx->sim);
    assert_true (err == bl_nothing_to_do);
    err = ssc_read (ctx->sim, &count, &read, 1, 0);
    assert_true (err == bl_timeout);
  }
  err = ssc_run_teardown (ctx->sim);
  assert_true (!err);
  assert_true (ctx->fteardown_count == 1);
  assert_true (ctx->teardown_count == 1);
}
/*---------------------------------------------------------------------------*/
static void queue_match_test (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err);
  assert_true (ctx->fsetup_count == 1);

  err = ssc_try_run_some (ctx->sim);
  assert_true (!err);
  uword count;
  ssc_output_data read;
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err == bl_timeout);

  /*writing non matching data*/
  u8* send = ssc_alloc_write_bytestream (ctx->sim, 1);
  assert_non_null (send);

  *send = fiber_match - 1;
  err  = ssc_write (ctx->sim, 0, send, 1);
  assert (!err);

  do {
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err || err == bl_nothing_to_do);
  }
  while (err != bl_nothing_to_do);

  /*checking that it didn't match*/
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err == bl_timeout);

  /*writing matching data*/
  send = ssc_alloc_write_bytestream (ctx->sim, 1);
  assert_non_null (send);

  *send = fiber_match;
  err  = ssc_write (ctx->sim, 0, send, 1);
  assert (!err);

  do {
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err || err == bl_nothing_to_do);
  }
  while (err != bl_nothing_to_do);

  /*checking that it did match*/
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (!err);
  assert_true (read.type == ssc_type_dynamic_bytes);
  assert_true (read.gid == 0);
  memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!memr16_is_null (rd));
  assert_true (memr16_size (rd) == 1);
  assert_true (*memr16_beg_as (rd, u8) == fiber_resp);

  ssc_dealloc_read_data (ctx->sim, &read);
  assert_true (ctx->dealloc.count == 1);
  assert_true (ctx->dealloc.mem == memr16_beg (rd));
  assert_true (ctx->dealloc.size == 1);
  assert_true (ctx->dealloc.id == 0);

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err);
  assert_true (ctx->fteardown_count == 1);
  assert_true (ctx->teardown_count == 1);
}
/*---------------------------------------------------------------------------*/
static void answer_after_blocking_timeout_test (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err);
  assert_true (ctx->fsetup_count == 1);

  uword count;
  ssc_output_data read;

  err = ssc_run_some (ctx->sim, queue_timeout_us * 20);
  assert_true (!err);
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err == bl_timeout);

  err = ssc_run_some (ctx->sim, queue_timeout_us * 20);
  assert_true (!err);

  /*checking that the fiber did actually time out and went out of scope*/
  err = ssc_read (ctx->sim, &count, &read, 1, queue_timeout_us * 20);
  assert_true (!err);
  assert_true (read.type == ssc_type_dynamic_bytes);
  assert_true (read.gid == 0);
  memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!memr16_is_null (rd));
  assert_true (memr16_size (rd) == 1);
  assert_true (*memr16_beg_as (rd, u8) == fiber_resp);

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err);
  assert_true (ctx->teardown_count == 1);
}
/*---------------------------------------------------------------------------*/
static void delay_test (void **state)
{
  basic_tests_ctx* ctx = (basic_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err);
  assert_true (ctx->fsetup_count == 1);

  uword count;
  ssc_output_data read;

  err = ssc_run_some (ctx->sim, queue_timeout_us * 20);
  assert_true (!err);
  /*on this test the delays are just making forward progress without waiting
    (ahead of real time), so the content is already on the output queue with
    a time point on the future. We will succeed always, as the read
    timeout is "queue_timeout_us * 20". With a timeout of 0 this test would
    fail (given a regular OS scheduler time slice duration)*/
  err = ssc_read (ctx->sim, &count, &read, 1, queue_timeout_us * 20);
  assert_true (!err);
  assert_true (read.type == ssc_type_dynamic_bytes);
  assert_true (read.gid == 0);
  memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!memr16_is_null (rd));
  assert_true (memr16_size (rd) == 1);
  assert_true (*memr16_beg_as (rd, u8) == fiber_resp);

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err);
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
    delay_test, delay_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    answer_after_blocking_timeout_test, test_wait_timeout_setup, test_teardown
    ),
};
/*---------------------------------------------------------------------------*/
int basic_tests (void)
{
  return cmocka_run_group_tests (tests, nullptr, nullptr);
}
/*---------------------------------------------------------------------------*/

