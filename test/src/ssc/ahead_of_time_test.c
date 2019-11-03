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
typedef struct ahot_tests_ctx {
  ssc* sim;
  bl_timept32 t[16];
  bl_uword  fiber_count;
}
ahot_tests_ctx;
/*---------------------------------------------------------------------------*/
/*TRANSLATION UNIT GLOBALS*/
/*---------------------------------------------------------------------------*/
static const bl_u8 fiber1_resp = 0xdd;
/*---------------------------------------------------------------------------*/
static ahot_tests_ctx g_ctx;
static sim_env        g_env;
/*---------------------------------------------------------------------------*/
/*SIMULATION*/
/*---------------------------------------------------------------------------*/
static void sim_on_teardown_test (void* sim_context)
{
  /*tested on basic_test*/
}
/*----------------------------------------------------------------------------*/
static void sim_dealloc_test(
  void const* mem, bl_uword size, ssc_group_id id, void* sim_context
  )
{
  /*tested on basic_test*/
}
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
  assert_true (!err.bl);
  *state = (void*) &g_ctx;
}
/*---------------------------------------------------------------------------*/
static int test_teardown (void **state)
{
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) *state;
  if (!ctx) {
    return 1;
  }
  ssc_destroy (ctx->sim);
  return 0;
}
/*---------------------------------------------------------------------------*/
static inline void check_has_response(
    ahot_tests_ctx* ctx, bl_u8 exp, bl_timept32 time, bl_timeoft32 timeout
    )
{
  bl_uword count;
  ssc_output_data read;
  bl_err err = ssc_read (ctx->sim, &count, &read, 1, timeout);
  assert_true (!err.bl);
  assert_true (read.type == ssc_type_static_bytes);
  assert_true (read.gid == 0);
  assert_true (read.time == time);
  bl_memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!bl_memr16_is_null (rd));
  assert_true (bl_memr16_size (rd) == 1);
  assert_true (*bl_memr16_beg_as (rd, bl_u8) == exp);
  ssc_dealloc_read_data (ctx->sim, &read);
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static const bl_uword future_wake_delay_us = 6000; /*6ms*/
/*---------------------------------------------------------------------------*/
static void future_wake_fiber1(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env        = (sim_env*) sim_context;
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);

  ++g_ctx.fiber_count;

  ssc_delay (h, future_wake_delay_us);
  ssc_wake (h, 1, 1); /*this signal is deferred, as the delay didn't wait*/
  ctx->t[0] = ssc_get_timestamp (h);

  ssc_delay (h, future_wake_delay_us);
  ssc_wake (h, 1, 1); /*this signal is deferred too*/
  ctx->t[1] = ssc_get_timestamp (h);

  --g_ctx.fiber_count;
}
/*---------------------------------------------------------------------------*/
static void future_wake_fiber2(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env        = (sim_env*) sim_context;
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);

  ++g_ctx.fiber_count;

  bool unexpired = ssc_wait (h, 1, 0);
  assert_true (unexpired);
  ctx->t[2] = ssc_get_timestamp (h);

  unexpired = ssc_wait (h, 1, 0);
  assert_true (unexpired);
    ctx->t[3] = ssc_get_timestamp (h);

  --g_ctx.fiber_count;
}
/*---------------------------------------------------------------------------*/
static int future_wake_test_setup (void **state)
{
  ssc_fiber_cfg fibers[2];
  fibers[0] = ssc_fiber_cfg_rv(
    0, future_wake_fiber1, nullptr, nullptr, nullptr
    );
  fibers[1] = ssc_fiber_cfg_rv(
    0, future_wake_fiber2, nullptr, nullptr, nullptr
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static void future_wake_test (void **state)
{
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) *state;
  bl_err err          = ssc_run_setup (ctx->sim);
  assert_true (!err.bl);

  do {
    err = ssc_run_some (ctx->sim, 1000);
    assert_true (!err.bl || err.bl == bl_nothing_to_do || err.bl == bl_timeout);
  }
  while (ctx->fiber_count > 0);

  assert_true (bl_timept32_get_diff (ctx->t[0], ctx->t[1]) < 0);
  assert_true (bl_timept32_get_diff (ctx->t[2], ctx->t[3]) < 0);

  assert_true (bl_timept32_get_diff (ctx->t[0], ctx->t[2]) <= 0);
  assert_true (bl_timept32_get_diff (ctx->t[1], ctx->t[3]) <= 0);

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.bl);
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static const bl_uword future_wait_delay_us = 6000; /*6ms*/
/*---------------------------------------------------------------------------*/
static void future_wait_fiber1(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env        = (sim_env*) sim_context;
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);

  ++g_ctx.fiber_count;

  ctx->t[0] = ssc_get_timestamp (h);
  bl_timept32 bl_deadline = ctx->t[0]  + bl_timept32_to_usec (future_wait_delay_us);
  while (bl_timept32_get_diff (bl_deadline, ssc_get_timestamp (h)) >= 0) {
    ssc_yield (h);
  }
  ctx->t[1] = ssc_get_timestamp (h);
  ssc_wake (h, 1, 1); /*non deferred wake*/

  --g_ctx.fiber_count;
}
/*---------------------------------------------------------------------------*/
static void future_wait_fiber2(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env        = (sim_env*) sim_context;
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);

  ++g_ctx.fiber_count;

  ctx->t[2] = ssc_get_timestamp (h);
  ssc_delay (h, future_wake_delay_us); /*fiber ahead of time*/
  bool unexpired = ssc_wait (h, 1, 0); /*future wait*/
  assert_true (unexpired);
  ctx->t[3] = ssc_get_timestamp (h);

  --g_ctx.fiber_count;
}
/*---------------------------------------------------------------------------*/
static int future_wait_test_setup (void **state)
{
  ssc_fiber_cfg fibers[2];
  fibers[0] = ssc_fiber_cfg_rv(
    0, future_wait_fiber1, nullptr, nullptr, nullptr
    );
  fibers[1] = ssc_fiber_cfg_rv(
    0, future_wait_fiber2, nullptr, nullptr, nullptr
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static void future_wait_test (void **state)
{
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) *state;
  bl_err err          = ssc_run_setup (ctx->sim);
  assert_true (!err.bl);

  do {
    err = ssc_run_some (ctx->sim, future_wait_delay_us);
    assert_true (!err.bl || err.bl == bl_nothing_to_do || err.bl == bl_timeout);
  }
  while (ctx->fiber_count > 0);

  assert_true (bl_timept32_get_diff (ctx->t[0], ctx->t[1]) < 0);
  assert_true (bl_timept32_get_diff (ctx->t[2], ctx->t[3]) < 0);

  assert_true (bl_timept32_get_diff (ctx->t[0], ctx->t[2]) <= 0);
  assert_true (bl_timept32_get_diff (ctx->t[1], ctx->t[3]) <= 0);

  assert_true(
      bl_timept32_get_diff (ctx->t[1], ctx->t[0]) >=
        bl_timept32_to_usec (future_wait_delay_us)
      );

  assert_true(
    bl_timept32_get_diff (ctx->t[3], ctx->t[2]) >=
      bl_timept32_to_usec (future_wait_delay_us)
    );
  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.bl);
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static const bl_uword future_produce_delay_us = 1000;
static const bl_uword future_produce_count    = 4;
/*---------------------------------------------------------------------------*/
static void future_produce_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env        = (sim_env*) sim_context;
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);

  ++g_ctx.fiber_count;

  for (bl_uword i = 0; i < future_produce_count; ++i) {
    ssc_delay (h, future_produce_delay_us);
    ssc_produce_static_output (h, bl_memr16_rv ((void*) &fiber1_resp, 1));
    ctx->t[i] = ssc_get_timestamp (h);
  }
  --g_ctx.fiber_count;
}
/*---------------------------------------------------------------------------*/
static int future_produce_test_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0, future_produce_fiber, nullptr, nullptr, nullptr
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static void future_produce_test (void **state)
{
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) *state;
  bl_err err          = ssc_run_setup (ctx->sim);
  assert_true (!err.bl);

  err = ssc_run_some (ctx->sim, future_produce_delay_us);
  assert_true (!err.bl);
  /*everything should be generated on the queue now*/
  for (bl_uword i = 0; i < future_produce_count; ++i) {
    check_has_response (ctx, fiber1_resp, ctx->t[i], future_produce_delay_us);
  }
  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.bl);
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static const bl_uword future_context_switch_produce_delay_us     = 1000;
static const bl_uword future_context_switch_produce_delay_big_us = 50000;
static const bl_uword future_context_switch_produce_count        = 3;
/*---------------------------------------------------------------------------*/
static void future_context_switch_fiber(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  sim_env* env        = (sim_env*) sim_context;
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) env->ctx;
  assert_true (sim_context == (void*) &g_env);

  ++g_ctx.fiber_count;

  bl_uword i = 0;
  for (i = 0; i < future_context_switch_produce_count; ++i) {
    ssc_delay (h, future_context_switch_produce_delay_us);
    ssc_produce_static_output (h, bl_memr16_rv ((void*) &fiber1_resp, 1));
    ctx->t[i] = ssc_get_timestamp (h);
  }
  /*this delay will force an internal context switch*/
  ssc_delay (h, future_context_switch_produce_delay_big_us);
  ctx->t[i] = ssc_get_timestamp (h);
  ssc_produce_static_output (h, bl_memr16_rv ((void*) &fiber1_resp, 1));

  --g_ctx.fiber_count;
}
/*---------------------------------------------------------------------------*/
static int future_context_switch_test_setup (void **state)
{
  ssc_fiber_cfg fibers[1];
  fibers[0] = ssc_fiber_cfg_rv(
    0, future_context_switch_fiber, nullptr, nullptr, nullptr
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static void future_context_switch_produce_test (void **state)
{
  ahot_tests_ctx* ctx = (ahot_tests_ctx*) *state;
  bl_err err          = ssc_run_setup (ctx->sim);
  assert_true (!err.bl);

  err = ssc_run_some (ctx->sim, future_context_switch_produce_delay_us);
  assert_true (!err.bl);
  /*retrieving messages before the context switch*/
  bl_uword i = 0;
  for (; i < future_context_switch_produce_count; ++i) {
    check_has_response(
      ctx, fiber1_resp, ctx->t[i], future_context_switch_produce_delay_us
      );
  }
  /*checking that there are no more messages left*/
  bl_uword count;
  ssc_output_data read;
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err.bl == bl_timeout);

  /*running another time slice and checking that the data is present*/
  err = ssc_run_some (ctx->sim, future_context_switch_produce_delay_big_us);
  assert_true (!err.bl);
  check_has_response (ctx, fiber1_resp, ctx->t[i], 0);

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.bl);
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static const struct CMUnitTest tests[] = {
  cmocka_unit_test_setup_teardown(
    future_wake_test, future_wake_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    future_wait_test, future_wait_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    future_produce_test, future_produce_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
      future_context_switch_produce_test,
      future_context_switch_test_setup,
      test_teardown
    ),
};
/*---------------------------------------------------------------------------*/
int ahead_of_time_tests (void)
{
  return cmocka_run_group_tests (tests, nullptr, nullptr);
}
/*---------------------------------------------------------------------------*/

