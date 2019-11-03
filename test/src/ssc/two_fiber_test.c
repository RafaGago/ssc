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
typedef struct two_fiber_tests_ctx {
  ssc* sim;
}
two_fiber_tests_ctx;
/*---------------------------------------------------------------------------*/
/*TRANSLATION UNIT GLOBALS*/
/*---------------------------------------------------------------------------*/
static const bl_u8    fiber1_match     = 0xcc;
static const bl_u8    fiber1_resp      = 0xdd;
static const bl_u8    fiber2_match     = 0xee;
static const bl_u8    fiber2_resp      = 0xff;
static const bl_uword queue_timeout_us = 1000;
/*---------------------------------------------------------------------------*/
static two_fiber_tests_ctx g_ctx;
static sim_env             g_env;
/*---------------------------------------------------------------------------*/
static inline void check_has_response(
  two_fiber_tests_ctx* ctx, bl_u8 exp, bl_timeoft32 t
  )
{
  bl_uword count;
  ssc_output_data read;
  bl_err err = ssc_read (ctx->sim, &count, &read, 1, t);
  assert_true (!err.bl);
  assert_true (read.type == ssc_type_static_bytes);
  assert_true (read.gid == 0);
  bl_memr16 rd = ssc_output_read_as_bytes (&read);
  assert_true (!bl_memr16_is_null (rd));
  assert_true (bl_memr16_size (rd) == 1);
  assert_true (*bl_memr16_beg_as (rd, bl_u8) == exp);
  ssc_dealloc_read_data (ctx->sim, &read);
}
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
static void queue_output_fiber1(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);

  bl_memr16 match = bl_memr16_rv ((void*) &fiber1_match, 1);
  while (true) {
    bl_memr16 in = ssc_peek_input_head_match (h, match);
    assert_true (!bl_memr16_is_null (in));
    assert_true (bl_memr16_size (in) == 1);
    assert_true (*bl_memr16_beg_as (in, bl_u8) == fiber1_match);
    ssc_drop_input_head (h);
    ssc_produce_static_output (h, bl_memr16_rv ((void*) &fiber1_resp, 1));
  }
}
/*---------------------------------------------------------------------------*/
static void queue_output_fiber2(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);

  bl_memr16 match = bl_memr16_rv ((void*) &fiber2_match, 1);
  while (true) {
    bl_memr16 in = ssc_peek_input_head_match (h, match);
    assert_true (!bl_memr16_is_null (in));
    assert_true (bl_memr16_size (in) == 1);
    assert_true (*bl_memr16_beg_as (in, bl_u8) == fiber2_match);
    ssc_drop_input_head (h);
    ssc_produce_static_output (h, bl_memr16_rv ((void*) &fiber2_resp, 1));
  }
}
/*---------------------------------------------------------------------------*/
static void wait_wake_fiber1(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);

  bl_memr16 match = bl_memr16_rv ((void*) &fiber1_match, 1);
  while (true) {
    bl_memr16 in = ssc_peek_input_head_match (h, match);
    assert_true (!bl_memr16_is_null (in));
    assert_true (bl_memr16_size (in) == 1);
    assert_true (*bl_memr16_beg_as (in, bl_u8) == fiber1_match);
    ssc_drop_input_head (h);
    ssc_wake (h, 1, 1);
  }
}
/*---------------------------------------------------------------------------*/
static void wait_wake_fiber2(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);

  while (true) {
    ssc_wait (h, 1, 0);
    ssc_produce_static_output (h, bl_memr16_rv ((void*) &fiber2_resp, 1));
  }
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
  assert_true (!err.bl);
  *state = (void*) &g_ctx;
}
/*---------------------------------------------------------------------------*/
static int test_teardown (void **state)
{
  two_fiber_tests_ctx* ctx = (two_fiber_tests_ctx*) *state;
  if (!ctx) {
    return 1;
  }
  ssc_destroy (ctx->sim);
  return 0;
}
/*---------------------------------------------------------------------------*/
static int queue_two_fiber_test_setup (void **state)
{
  ssc_fiber_cfg fibers[2];
  fibers[0] = ssc_fiber_cfg_rv(
    0, queue_output_fiber1, nullptr, nullptr, nullptr
    );
  fibers[1] = ssc_fiber_cfg_rv(
    0, queue_output_fiber2, nullptr, nullptr, nullptr
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static void queue_two_fiber_test (void **state)
{
  two_fiber_tests_ctx* ctx = (two_fiber_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err.bl);

  bl_uword count;
  ssc_output_data read;

  err = ssc_try_run_some (ctx->sim);
  assert_true (!err.bl);
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err.bl == bl_timeout);

  for (bl_uword i = 0; i < 5; ++i) {
    err = ssc_try_run_some (ctx->sim);
    assert_true (err.bl == bl_nothing_to_do);
    err = ssc_read (ctx->sim, &count, &read, 1, 0);
    assert_true (err.bl == bl_timeout);
  }

  for (bl_uword i = 0; i < 5; ++i) {
    /*match for fiber 1*/
    bl_u8* send = ssc_alloc_write_bytestream (ctx->sim, 1);
    assert_non_null (send);
    *send = fiber1_match;
    err  = ssc_write (ctx->sim, 0, send, 1);
    assert (!err.bl);

    /*running fiber 1*/
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err.bl);

    /*checking that fiber 1 replied*/
    check_has_response (ctx, fiber1_resp, 0);
  }

  for (bl_uword i = 0; i < 5; ++i) {
    /*match for fiber 2*/
    bl_u8* send = ssc_alloc_write_bytestream (ctx->sim, 1);
    assert_non_null (send);
    *send = fiber2_match;
    err  = ssc_write (ctx->sim, 0, send, 1);
    assert (!err.bl);

    /*running fiber 2*/
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err.bl);

    /*checking that fiber 2 replied*/
    check_has_response (ctx, fiber2_resp, 0);
  }

  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.bl);
}
/*---------------------------------------------------------------------------*/
static int wait_wake_test_setup (void **state)
{
  ssc_fiber_cfg fibers[2];
  fibers[0] = ssc_fiber_cfg_rv(
    0, wait_wake_fiber1, nullptr, nullptr, nullptr
    );
  fibers[1] = ssc_fiber_cfg_rv(
    0, wait_wake_fiber2, nullptr, nullptr, nullptr
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static void wait_wake_test (void **state)
{
  two_fiber_tests_ctx* ctx = (two_fiber_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err.bl);

  bl_uword count;
  ssc_output_data read;

  err = ssc_try_run_some (ctx->sim);
  assert_true (!err.bl);
  err = ssc_read (ctx->sim, &count, &read, 1, 0);
  assert_true (err.bl == bl_timeout);

  for (bl_uword i = 0; i < 5; ++i) {
    err = ssc_try_run_some (ctx->sim);
    assert_true (err.bl == bl_nothing_to_do);
    err = ssc_read (ctx->sim, &count, &read, 1, 0);
    assert_true (err.bl == bl_timeout);
  }

  for (bl_uword i = 0; i < 5; ++i) {
    /*match for fiber 1*/
    bl_u8* send = ssc_alloc_write_bytestream (ctx->sim, 1);
    assert_non_null (send);
    *send = fiber1_match;
    err  = ssc_write (ctx->sim, 0, send, 1);
    assert (!err.bl);

    /*running fiber 1 and fiber 2*/
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err.bl);
    /*  fiber can be just woken but not executed on the same slice*/
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err.bl || err.bl == bl_nothing_to_do);

    /*checking that fiber 2 replied*/
    check_has_response (ctx, fiber2_resp, 0);
  }
  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.bl);
}
/*---------------------------------------------------------------------------*/
static const bl_uword select_lowest_next_msgs     = 3;
static const bl_uword select_lowest_short_timeout = 1000;
/*---------------------------------------------------------------------------*/
static void select_lowest_next_fiber1(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);

  for (bl_uword i = 0; i < select_lowest_next_msgs; ++i) {
    ssc_wait (h, 1, 10000000); /*10s*/
    ssc_produce_static_output (h, bl_memr16_rv ((void*) &fiber1_resp, 1));
    ssc_yield (h);
  }
}
/*---------------------------------------------------------------------------*/
static void select_lowest_next_fiber2(
  ssc_handle h, void* fiber_context, void* sim_context
  )
{
  assert_true (sim_context == (void*) &g_env);

  for (bl_uword i = 0; i < select_lowest_next_msgs; ++i) {
    bool unexpired = ssc_wait (h, 1, select_lowest_short_timeout); /*1ms*/
    assert_true (!unexpired);
    ssc_produce_static_output (h, bl_memr16_rv ((void*) &fiber2_resp, 1));
    ssc_yield (h);
  }
}
/*---------------------------------------------------------------------------*/
static int select_lowest_next_test_setup (void **state)
{
  ssc_fiber_cfg fibers[2];
  fibers[0] = ssc_fiber_cfg_rv(
    0, select_lowest_next_fiber1, nullptr, nullptr, nullptr
    );
  fibers[1] = ssc_fiber_cfg_rv(
    0, select_lowest_next_fiber2, nullptr, nullptr, nullptr
    );
  generic_test_setup (state, fibers, bl_arr_elems (fibers));
  return 0;
}
/*---------------------------------------------------------------------------*/
static void select_lowest_next_test (void **state)
{
  /*tests that the event with the lowest bl_deadline is always selected*/
  two_fiber_tests_ctx* ctx = (two_fiber_tests_ctx*) *state;
  bl_err err = ssc_run_setup (ctx->sim);
  assert_true (!err.bl);

  for (bl_uword i = 0; i < select_lowest_next_msgs; ++i) {
    /*i == 0 -> initial run, i != 0 -> yield */
    err = ssc_try_run_some (ctx->sim);
    assert_true (!err.bl);
    /*wait + produce*/
    err = ssc_run_some (ctx->sim, select_lowest_short_timeout);
    assert_true (!err.bl);
    check_has_response (ctx, fiber2_resp, 0);
  }
  err = ssc_run_teardown (ctx->sim);
  assert_true (!err.bl);
}
/*---------------------------------------------------------------------------*/
static const struct CMUnitTest tests[] = {
  cmocka_unit_test_setup_teardown(
    queue_two_fiber_test, queue_two_fiber_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    wait_wake_test, wait_wake_test_setup, test_teardown
    ),
  cmocka_unit_test_setup_teardown(
    select_lowest_next_test, select_lowest_next_test_setup, test_teardown
    ),
};
/*---------------------------------------------------------------------------*/
int two_fiber_tests (void)
{
  return cmocka_run_group_tests (tests, nullptr, nullptr);
}
/*---------------------------------------------------------------------------*/

