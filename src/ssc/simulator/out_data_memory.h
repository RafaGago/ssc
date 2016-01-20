#ifndef __SSC_OUT_DATA_MEMORY_H__
#define __SSC_OUT_DATA_MEMORY_H__

#include <ssc/simulator/simulation.h>

/*----------------------------------------------------------------------------*/
extern void ssc_out_memory_dealloc(
  ssc_sim_dealloc_signature f,
  void*                     global_sim_context,
  ssc_output_data const*    d
  );
/*----------------------------------------------------------------------------*/

#endif /* __SSC_OUT_DATA_MEMORY_H__ */

