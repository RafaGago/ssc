#ifndef __SSC_OUT_QUEUE_H__
#define __SSC_OUT_QUEUE_H__

#include <bl/base/platform.h>
#include <bl/base/integer.h>
#include <bl/base/time.h>
#include <bl/base/flat_deadlines.h>

#include <bl/nonblock/mpmc_bt.h>

#include <ssc/types.h>
#include <ssc/simulator/simulation.h>

struct ssc_global;
/*----------------------------------------------------------------------------*/
typedef struct out_q_sorted_value {
  bl_memr16    data;
  ssc_group_id gid;
  ssc_out_type type;
}
out_q_sorted_value;
/*----------------------------------------------------------------------------*/
typedef struct ssc_out_q {
  bl_mpmc_bt               queue;
  bl_flat_deadlines        tsorted;
  struct ssc_global const* global;
}
ssc_out_q;
/*----------------------------------------------------------------------------*/
extern bl_err ssc_out_q_init(
  ssc_out_q* q, bl_uword size, struct ssc_global const* global
  );
/*----------------------------------------------------------------------------*/
extern void ssc_out_q_destroy (ssc_out_q* q);
/*----------------------------------------------------------------------------*/
extern bl_err ssc_out_q_produce (ssc_out_q* q, ssc_output_data* d);
/*----------------------------------------------------------------------------*/
extern bl_err ssc_out_q_consume(
  ssc_out_q*       q,
  bl_uword*        d_consumed,
  ssc_output_data* d,
  bl_uword         d_capacity,
  bl_timeoft32       timeout_us
  );
/*----------------------------------------------------------------------------*/

#endif /* __SSC_OUT_QUEUE_H__ */

