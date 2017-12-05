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
  memr16       data;
  ssc_group_id gid;
  ssc_out_type type;
}
out_q_sorted_value;
/*----------------------------------------------------------------------------*/
typedef struct ssc_out_q {
  mpmc_bt                  queue;
  flat_deadlines           tsorted;
  struct ssc_global const* global;
}
ssc_out_q;
/*----------------------------------------------------------------------------*/
extern bl_err ssc_out_q_init(
  ssc_out_q* q, uword size, struct ssc_global const* global
  );
/*----------------------------------------------------------------------------*/
extern void ssc_out_q_destroy (ssc_out_q* q);
/*----------------------------------------------------------------------------*/
extern bl_err ssc_out_q_produce (ssc_out_q* q, ssc_output_data* d);
/*----------------------------------------------------------------------------*/
extern bl_err ssc_out_q_consume(
  ssc_out_q*       q,
  uword*           d_consumed,
  ssc_output_data* d,
  uword            d_capacity,
  toffset          timeout_us
  );
/*----------------------------------------------------------------------------*/

#endif /* __SSC_OUT_QUEUE_H__ */

