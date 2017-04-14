#include <coro.h>

#include <bl/base/default_allocator.h>
#include <bl/base/dynarray.h>
#include <bl/base/atomic.h>
#include <bl/base/integer.h>
#include <bl/base/time.h>
#include <bl/base/dynarray.h>

#include <bl/task_queue/task_queue.h>

#include <ssc/log.h>
#include <ssc/simulation/simulation.h>
#include <ssc/simulator/simulator.h>
#include <ssc/simulator/simulation.h>
#include <ssc/simulator/global.h>
#include <ssc/simulator/cfg.h>
#include <ssc/simulator/in_bstream.h>
#include <ssc/simulator/out_data_memory.h>
#include <ssc/simulator/group_scheduler.h>

/*----------------------------------------------------------------------------*/
define_dynarray_types (gscheds, gsched)
declare_dynarray_funcs (gscheds, gsched)

define_dynarray_types (gsched_cfgs, ssc_fiber_cfgs)
declare_dynarray_funcs (gsched_cfgs, ssc_fiber_cfgs)
/*----------------------------------------------------------------------------*/
enum {
  ssc_on_setup,
  ssc_initialized,
  ssc_running,
  ssc_stopped,
};
/*----------------------------------------------------------------------------*/
struct ssc {
  ssc_global          global;
  alloc_tbl           alloc;
  ssc_simulation_var (lib);
  gscheds             groups;
  gsched_cfgs         fg_cfgs;
  bl_err              err;
  atomic_uword        state;
};
/*----------------------------------------------------------------------------*/
bl_err ssc_api_add_fiber (ssc_handle h, ssc_fiber_cfg const* cfg)
{
  ssc* sim = (ssc*) h;
  if (sim->err) {
    return sim->err;
  }
  ssc_fiber_cfgs* gcfg = nullptr;
  uword fg_cfgs_size   = gsched_cfgs_size (&sim->fg_cfgs);

  if (cfg->id == fg_cfgs_size) {
    /*fiber added to new fiber group*/
    sim->err = gsched_cfgs_grow (&sim->fg_cfgs, 1, &sim->alloc);
    if (sim->err) {
      log_error ("fiber group cfgs resize error:\n", sim->err);
      return sim->err;
    }
    gcfg = gsched_cfgs_at (&sim->fg_cfgs, cfg->id);
    ssc_fiber_cfgs_init (gcfg, 0, &sim->alloc);
  }
  else if (cfg->id == fg_cfgs_size - 1) {
    /*fiber added to current fiber group*/
    gcfg = gsched_cfgs_at (&sim->fg_cfgs, cfg->id);
  }
  else
  {
    log_error(
      "non-consecutive id when initializing fiber on group: %u\n", cfg->id
      );
    return bl_invalid;
  }
  sim->err = ssc_fiber_cfgs_grow (gcfg, 1, &sim->alloc);
  if (sim->err) {
    log_error ("fiber cfgs resize error:\n", sim->err);
    return sim->err;
  }
  *ssc_fiber_cfgs_last (gcfg) = *cfg;
  sim->err = gsched_fiber_cfg_validate_correct (ssc_fiber_cfgs_last (gcfg));
  if (sim->err) {
    log_error(
      "fiber cfg validation error:%u, on group:%u\n", sim->err, cfg->id
      );
  }
  return sim->err;
}
/*----------------------------------------------------------------------------*/
static void ssc_run_manual_link_to_simulator (ssc* d)
{
#ifdef SSC_SHAREDLIB
  ssc_simulator_ftable t;
  t.add_fiber                        = ssc_api_add_fiber;
  t.yield                            = ssc_api_yield;
  t.wake                             = ssc_api_wake;
  t.wait                             = ssc_api_wait;
  t.peek_input_head                  = ssc_api_peek_input_head;
  t.timed_peek_input_head            = ssc_api_timed_peek_input_head;
  t.try_peek_input_head              = ssc_api_try_peek_input_head;
  t.drop_input_head                  = ssc_api_drop_input_head;
  t.delay                            = ssc_api_delay;
  t.get_timestamp                    = ssc_api_get_timestamp;
  t.produce_static_output            = ssc_api_produce_static_output;
  t.produce_dynamic_output           = ssc_api_produce_dynamic_output;
  t.produce_error                    = ssc_api_produce_error;
  t.produce_static_string            = ssc_api_produce_static_string;
  t.produce_dynamic_string           = ssc_api_produce_dynamic_string;
  t.peek_input_head_match_mask       = ssc_api_peek_input_head_match_mask;
  t.timed_peek_input_head_match_mask =
    ssc_api_timed_peek_input_head_match_mask;
  t.fiber_get_run_cfg                = ssc_api_fiber_get_run_cfg;
  t.fiber_set_run_cfg                = ssc_api_fiber_set_run_cfg;
  d->lib.manual_link (&t);
#endif
}
/*----------------------------------------------------------------------------*/
static void ssc_estimate_taskq_size(
  ssc* sim, uword* regular, uword* delayed
  )
{
  uword fiber_count   = 0;
  ssc_fiber_cfgs *g = gsched_cfgs_beg (&sim->fg_cfgs);
  while (g < gsched_cfgs_end (&sim->fg_cfgs)) {
    fiber_count += ssc_fiber_cfgs_size (g);
    ++g;
  }
  *regular = fiber_count * 4;
  *delayed = fiber_count;
}
/*----------------------------------------------------------------------------*/
static void ssc_destroy_fiber_groups (ssc* sim)
{
  gsched *g = gscheds_beg (&sim->groups);
  while (g < gscheds_end (&sim->groups)) {
    gsched_destroy (g, &sim->alloc);
    ++g;
  }
  gscheds_destroy (&sim->groups, &sim->alloc);
}
/*----------------------------------------------------------------------------*/
static void ssc_destroy_fiber_group_cfgs (ssc* sim)
{
  ssc_fiber_cfgs *g = gsched_cfgs_beg (&sim->fg_cfgs);
  while (g < gsched_cfgs_end (&sim->fg_cfgs)) {
    ssc_fiber_cfgs_destroy (g, &sim->alloc);
    ++g;
  }
  gsched_cfgs_destroy (&sim->fg_cfgs, &sim->alloc);
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_create(
  ssc**     instance_out,
  char const* simlib_path,
  void*       simlib_passed_data
  )
{
  if (!instance_out) {
    return bl_invalid;
  }
  /*allocation*/
  alloc_tbl def_alloc = get_default_alloc();
  ssc* sim          = (ssc*) bl_alloc (&def_alloc, sizeof *sim);
  if (!sim) {
    log_error ("error when allocating ssc\n");
    return bl_alloc;
  }
  memset (sim, 0, sizeof *sim);
  sim->alloc        = def_alloc;
  sim->global.alloc = &sim->alloc;
  atomic_uword_store_rlx (&sim->state, ssc_on_setup);

  gscheds_init (&sim->groups, 0, &sim->alloc); /*no allocation*/
  gsched_cfgs_init (&sim->fg_cfgs, 0, &sim->alloc); /*no allocation*/

  /*libload*/
  bl_err err = ssc_simulation_load (&sim->lib, simlib_path);
  if (err) {
    /*error already logged*/
    goto mem_dealloc;
  }
  sim->global.sim_dealloc = ssc_simulation_dealloc_func (&sim->lib);
#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT
  sim->global.sim_before_fiber_context_switch =
    ssc_simulation_before_fiber_context_switch_func (&sim->lib);
#endif
  /*init out queue*/
  ssc_cfg global_cfg;
  ssc_cfg_init (&global_cfg); /*exposing cfg is TBD TODO*/
  err = ssc_out_q_init(
    &sim->global.out_queue,
    global_cfg.min_out_queue_size,
    &sim->global
    );
  if (err) {
    log_error ("error initializing out queue:%u\n", err);
    goto libunload;
  }
  /*retrieve cfg from simulation (the simulation will call
    "ssc_api_setup_global" and "ssc_api_add_fiber") */
  ssc_run_manual_link_to_simulator (sim);
  err = ssc_simulation_on_setup(
    &sim->lib, sim, simlib_passed_data, &sim->global.sim_context
    );
  if (err) {
    log_error ("error when running \"ssc_sim_on_setup\":%u\n", err);
    goto destroy_cfgs;
  }
  uword group_count = gsched_cfgs_size (&sim->fg_cfgs);
  if (group_count == 0) {
    log_error ("no fibers created on \"ssc_sim_on_setup\"\n");
    err = bl_error;
    goto destroy_cfgs;
  }
  if (sim->err) {
    /*error already logged*/
    err = sim->err;
    goto simulator_teardown;
  }

  /*init task queue*/
  uword regular, delayed;
  ssc_estimate_taskq_size (sim, &regular, &delayed);
  err = taskq_init (&sim->global.tq, &sim->alloc, regular, delayed);
  if (err) {
    log_error ("error creating task queue:%u\n", err);
    goto simulator_teardown;
  }

  /*init fiber groups*/
  ssc_fiber_group_cfg group_cfg;
  ssc_fiber_group_cfg_init (&group_cfg); /*exposing cfg is TBD TODO*/
  for (uword i = 0; i < group_count; ++i) {
    err = gscheds_grow (&sim->groups, 1, &sim->alloc);
    if (err) {
      log_error ("error allocating fiber group:%u\n", err);
      goto destroy_taskq;
    }
    gsched* g               = gscheds_last (&sim->groups);
    ssc_fiber_cfgs* gf_cfgs = gsched_cfgs_at (&sim->fg_cfgs, i);
    err = gsched_init(
      g, i, &sim->global, &group_cfg, gf_cfgs, &sim->alloc
      );
    if (err) {
      log_error ("error intializing fiber group %u: %u\n", i, err);
      goto destroy_fiber_groups;
    }
  }
  /*ready to run fibers setup + main loop*/
  atomic_uword_store (&sim->state, ssc_initialized, mo_release);
  *instance_out = sim;
  return bl_ok;

destroy_fiber_groups:
  ssc_destroy_fiber_groups (sim);
destroy_taskq:
  taskq_destroy (sim->global.tq, &sim->alloc);
simulator_teardown:
  ssc_simulation_on_teardown (&sim->lib, sim->global.sim_context);
destroy_cfgs:
  ssc_destroy_fiber_group_cfgs (sim);
/*destroy_out_queue:*/
  ssc_out_q_destroy (&sim->global.out_queue);
libunload:
  ssc_simulation_unload (&sim->lib);
  gsched_cfgs_destroy (&sim->fg_cfgs, &sim->alloc);
  gscheds_destroy (&sim->groups, &sim->alloc);
mem_dealloc:
  bl_dealloc (&sim->alloc, sim);
  return err;
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_destroy (ssc* sim)
{
  uword state = atomic_uword_load_rlx (&sim->state);
  if (state == ssc_initialized) {
    ssc_run_setup (sim);
  }
  if (state == ssc_running) {
    ssc_run_teardown (sim);
  }
  if (state != ssc_stopped) {
    return bl_not_allowed;
  }
  ssc_destroy_fiber_groups (sim);
  taskq_destroy (sim->global.tq, &sim->alloc);
  ssc_out_q_destroy (&sim->global.out_queue);
  ssc_destroy_fiber_group_cfgs (sim);
  ssc_simulation_unload (&sim->lib);
  gsched_cfgs_destroy (&sim->fg_cfgs, &sim->alloc);
  gscheds_destroy (&sim->groups, &sim->alloc);
  bl_dealloc (&sim->alloc, sim);
  return bl_ok;
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_run_setup (ssc* sim)
{
  /*the global setup function is run on ssc_create, we need to ensure memory
    visibility*/
  if (atomic_uword_load (&sim->state, mo_acquire) != ssc_initialized) {
    return bl_not_allowed;
  }
  bl_err err            = bl_ok;
  gsched *g  = gscheds_beg (&sim->groups);
  gsched *eg;

  while (g < gscheds_end (&sim->groups)) {
    err = gsched_run_setup (g);
    if (err) {
      goto rollback;
    }
    ++g;
  }
  atomic_uword_store_rlx (&sim->state, ssc_running);
  return err;
rollback:
  eg = gscheds_beg (&sim->groups);
  while (eg < g) {
    gsched_run_teardown (eg);
    ++eg;
  }
  return err;
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_run_teardown (ssc* sim)
{
  if (atomic_uword_load_rlx (&sim->state) != ssc_running) {
    return bl_not_allowed;
  }
  gsched *g = gscheds_beg (&sim->groups);
  while (g < gscheds_end (&sim->groups)) {
    gsched_run_teardown (g);
    ++g;
  }
  ssc_simulation_on_teardown (&sim->lib, sim->global.sim_context);
  atomic_uword_store_rlx (&sim->state, ssc_stopped);
  return bl_ok;
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT void ssc_block (ssc* sim)
{
  gsched *g = gscheds_beg (&sim->groups);
  while (g < gscheds_end (&sim->groups)) {
    ssc_in_q_block (&g->queue);
    ++g;
  }
  taskq_block (sim->global.tq);
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_run_some (ssc* sim, u32 usec_timeout)
{
  bl_assert (atomic_uword_load_rlx (&sim->state) == ssc_running);
  return taskq_run_one (sim->global.tq, usec_timeout);
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_try_run_some (ssc* sim)
{
  bl_assert (atomic_uword_load_rlx (&sim->state) == ssc_running);
  return taskq_try_run_one (sim->global.tq);
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT u8* ssc_alloc_write_bytestream (ssc* sim, uword capacity)
{
  u8* bstream = in_bstream_alloc (capacity, &sim->alloc);
  return bstream ? in_bstream_payload (bstream) : nullptr;
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_write(
  ssc* sim, ssc_group_id q, u8* bytestream, u16 size
  )
{
  bl_err err     = bl_ok;
  u8* in_bstream = in_bstream_from_payload (bytestream);
  bl_assert (in_bstream_pattern_validate (in_bstream)); /*big bug on the user side*/

  if (q >= gscheds_size (&sim->groups)) {
    err = bl_invalid;
    goto dealloc;
  }
  *in_bstream_tstamp (in_bstream)       = bl_get_tstamp();
  *in_bstream_payload_size (in_bstream) = size;

  gsched* g = gscheds_at (&sim->groups, q);
  bool idle_signal;
  err = ssc_in_q_produce (&g->queue, in_bstream, &idle_signal);
  if (err) {
    goto dealloc;
  }
  if (idle_signal) {
    /*no error when failing to schedule, as it isn't possible to roll back the
      insertion*/
    gsched_program_schedule (g);
  }
  return err;
dealloc:
  /*the frame is deallocated in all error cases to prevent leaks*/
  in_bstream_dealloc (in_bstream, &sim->alloc);
  return err;
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_read(
  ssc*             sim,
  uword*           d_consumed,
  ssc_output_data* d,
  uword            d_capacity,
  u32              timeout_us
  )
{
  return ssc_out_q_consume(
    &sim->global.out_queue, d_consumed, d, d_capacity, timeout_us
    );
}
/*----------------------------------------------------------------------------*/
SSC_SIM_EXPORT bl_err ssc_dealloc_read_data(
  ssc* sim, ssc_output_data* read_data
  )
{
  ssc_out_memory_dealloc(
    sim->global.sim_dealloc, sim->global.sim_context, read_data
    );
  return bl_ok;
}
/*----------------------------------------------------------------------------*/

