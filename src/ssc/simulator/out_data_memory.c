#include <bl/base/assert.h>
#include <bl/base/utility.h>

#include <ssc/simulator/out_data_memory.h>
/*----------------------------------------------------------------------------*/
void ssc_out_memory_dealloc(
  ssc_sim_dealloc_signature f,
  void*                     global_sim_context,
  ssc_output_data const*    d
  )
{
  bl_assert (f && d);
  if (ssc_output_is_dynamic (d)) {
    bl_assert (!ssc_output_is_error (d));
    f(
      bl_memr16_beg (d->data),
      bl_memr16_size (d->data),
      d->gid,
      global_sim_context
      );
  }
}
/*----------------------------------------------------------------------------*/
