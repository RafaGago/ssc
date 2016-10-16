#ifndef __SSC_TEST_SIMULATION_ENVIRONMENT_H__
#define __SSC_TEST_SIMULATION_ENVIRONMENT_H__

#include <bl/base/integer.h>

#include <ssc/types.h>

/*----------------------------------------------------------------------------*/
typedef void (*ssc_sim_env_dealloc_signature)(
  void const* mem, uword size, ssc_group_id id, void* sim_context
  );
/*----------------------------------------------------------------------------*/
typedef void (*ssc_sim_env_on_teardown_signature) (void* sim_context);
/*----------------------------------------------------------------------------*/
typedef struct sim_env {
  ssc_fiber_cfg*                    cfg;
  uword                             cfg_count;
  uword                             context_switch_count;
  ssc_sim_env_dealloc_signature     dealloc;
  ssc_sim_env_on_teardown_signature teardown;
  void*                             ctx;
}
sim_env;
/*----------------------------------------------------------------------------*/
#endif /* __SSC_TEST_SIMULATION_ENVIRONMENT_H__ */

