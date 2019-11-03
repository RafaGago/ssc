#ifndef __SSC_SIMULATOR_SIMULATION_H__
#define __SSC_SIMULATOR_SIMULATION_H__

#include <bl/base/platform.h>
#include <bl/base/integer.h>
#include <bl/base/error.h>

#include <ssc/types.h>
/*----------------------------------------------------------------------------*/
typedef void (*ssc_sim_dealloc_signature)(
  void const* mem, bl_uword size, ssc_group_id id, void* sim_context
  );
/*----------------------------------------------------------------------------*/
#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT
typedef void (*ssc_sim_before_fiber_context_switch_signature)(
  void* sim_context
  );
#endif /*#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT*/
/*----------------------------------------------------------------------------*/
#ifdef SSC_SHAREDLIB
#include <bl/base/sharedlib_load.h>
/*----------------------------------------------------------------------------*/
struct ssc_simulator_ftable;

typedef void (*ssc_sim_manual_link_signature)(
  struct ssc_simulator_ftable const* t
  );
/*----------------------------------------------------------------------------*/
typedef bl_err (*ssc_sim_on_setup_signature)(
  ssc_handle h, void const* passed_data, void** sim_context
  );
/*----------------------------------------------------------------------------*/
typedef void (*ssc_sim_on_teardown_signature) (void* sim_context);
/*----------------------------------------------------------------------------*/
typedef struct ssc_simlib {
  bl_sharedlib                                  lib;
  ssc_sim_manual_link_signature                 manual_link;
  ssc_sim_on_setup_signature                    on_setup;
  ssc_sim_on_teardown_signature                 on_teardown;
  ssc_sim_dealloc_signature                     dealloc;
  ssc_sim_before_fiber_context_switch_signature before_fiber_context_switch;
}
ssc_simlib;
/*----------------------------------------------------------------------------*/
extern void ssc_simlib_unload (ssc_simlib* sl);
/*----------------------------------------------------------------------------*/
extern bl_err ssc_simlib_load (ssc_simlib* si, char const* path);
/*----------------------------------------------------------------------------*/
#define ssc_simulation_var(varname)\
  ssc_simlib varname

#define ssc_simulation_load(simptr, path)\
  ssc_simlib_load (simptr, path)

#define ssc_simulation_unload(simptr)\
  ssc_simlib_unload (simptr)

#define ssc_simulation_on_setup(simptr, h, pd, sc)\
  (simptr)->on_setup (h, pd, sc)

#define ssc_simulation_on_teardown(simptr, sc)\
  (simptr)->on_teardown (sc)

#define ssc_simulation_dealloc_func(simptr)\
  (simptr)->dealloc

#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT
#define ssc_simulation_before_fiber_context_switch_func(simptr)\
  (simptr)->before_fiber_context_switch
#endif
/*----------------------------------------------------------------------------*/
#else /*SSC_SHAREDLIB*/
/*----------------------------------------------------------------------------*/
#define ssc_simulation_var(varname)

#define ssc_simulation_load(simptr, path) bl_mkok()
#define ssc_simulation_unload(simptr)

#define ssc_simulation_on_setup(simptr, h, pd, sc)\
  ssc_sim_on_setup (h, pd, sc)

#define ssc_simulation_on_teardown(simptr, sc)\
  ssc_sim_on_teardown (sc)

#define ssc_simulation_dealloc_func(simptr)\
  ssc_sim_dealloc

#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT
#define ssc_simulation_before_fiber_context_switch_func(simptr)\
  ssc_sim_before_fiber_context_switch
#endif
/*----------------------------------------------------------------------------*/
#endif /*SSC_SHAREDLIB*/
/*----------------------------------------------------------------------------*/

#endif /* __SSC_SIMULATOR_SIMULATION_H__ */

