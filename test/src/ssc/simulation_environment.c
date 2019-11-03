#include <bl/base/memory_range.h>
/*----------------------------------------------------------------------------*/
#include <ssc/simulation/simulation.h>
#include <ssc/simulation/simulation_src.h>
/*----------------------------------------------------------------------------*/
#include <ssc/simulation_environment.h>
/*----------------------------------------------------------------------------*/
bl_err ssc_sim_on_setup(
    ssc_handle h, void* passed_data, void** sim_context
    )
{
  sim_env* env             = passed_data;
  ssc_fiber_cfg const* cfg = env->cfg;
  for (bl_uword i = 0; i < env->cfg_count; ++i) {
    bl_err err = ssc_add_fiber (h, cfg);
    if (err.bl) {
      return err;
    }
    ++cfg;
  }
  *sim_context = env;
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
void ssc_sim_on_teardown (void* sim_context)
{
  sim_env* env = sim_context;
  env->teardown (sim_context);
}
/*----------------------------------------------------------------------------*/
void ssc_sim_dealloc(
  void const* mem, bl_uword size, ssc_group_id id, void* sim_context
  )
{
  sim_env* env = sim_context;
  env->dealloc (mem, size, id, sim_context);
}
/*----------------------------------------------------------------------------*/
#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT
void ssc_sim_before_fiber_context_switch (void* sim_context)
{
  sim_env* env = sim_context;
  ++env->context_switch_count;
}
#endif
/*----------------------------------------------------------------------------*/
