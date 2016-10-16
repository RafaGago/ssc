#ifdef SSC_SHAREDLIB
#include <ssc/simulator/simulation.h>
#include <ssc/log.h>

/*----------------------------------------------------------------------------*/
void ssc_simlib_unload (ssc_simlib* sl)
{
  bl_sharedlib_unload (sl->lib);
  sl->manual_link                 = nullptr;
  sl->on_setup                    = nullptr;
  sl->on_teardown                 = nullptr;
  sl->dealloc                     = nullptr;
  sl->before_fiber_context_switch = nullptr;
}
/*----------------------------------------------------------------------------*/
#define ssc_simlib_fn_load_impl_priv(si, name)\
  si->name = bl_sharedlib_loadsym (si->lib, "ssc_sim_" #name);\
  if (!si->name) {\
    log_error ("unable to load \"ssc_sim_" #name "\".");\
    goto fail;\
  }
/*----------------------------------------------------------------------------*/
bl_err ssc_simlib_load (ssc_simlib* si, char const* path)
{
  if (!path) {
    return bl_invalid;
  }
  si->lib             = bl_sharedlib_load (path);
  char const* err_str = bl_sharedlib_last_load_error (si);
  if (err_str) {
    log_error ("error loading simulation instance\n", err_str);
    return bl_error;
  }
  ssc_simlib_fn_load_impl_priv (si, manual_link)
  ssc_simlib_fn_load_impl_priv (si, on_setup)
  ssc_simlib_fn_load_impl_priv (si, on_teardown)
  ssc_simlib_fn_load_impl_priv (si, dealloc)
  ssc_simlib_fn_load_impl_priv (si, before_fiber_context_switch)
  return bl_ok;
fail:
  ssc_simlib_unload (si);
  return bl_error;
}
/*----------------------------------------------------------------------------*/
#endif
