#ifndef __SSC_IN_QUEUE_H__
#define __SSC_IN_QUEUE_H__

#include <bl/base/platform.h>
#include <bl/base/integer.h>
#include <bl/base/utility.h>
#include <bl/nonblock/mpmc_bt.h>

/*---------------------------------------------------------------------------*/
enum ssc_in_q_sig_e {
  in_q_ok       = 0,
  in_q_sig_idle = 1,
  in_q_blocked  = 2,
};
typedef bl_uword ssc_in_q_sig;
/*----------------------------------------------------------------------------*/
typedef struct ssc_in_q {
  bl_mpmc_bt   queue;
  bl_mpmc_b_op last_op;
}
ssc_in_q;
/*----------------------------------------------------------------------------*/
extern bl_err ssc_in_q_init(
  ssc_in_q* q, bl_uword queue_size, bl_alloc_tbl const* alloc
  );
/*----------------------------------------------------------------------------*/
extern bl_err ssc_in_q_destroy (ssc_in_q* q, bl_alloc_tbl const* alloc);
/*----------------------------------------------------------------------------*/
extern bl_u8* ssc_in_q_try_consume (ssc_in_q* q);
/*---------------------------------------------------------------------------*/
extern bl_err ssc_in_q_block (ssc_in_q* q);
/*----------------------------------------------------------------------------*/
extern bl_err ssc_in_q_try_switch_to_idle(
  ssc_in_q* q, ssc_in_q_sig* prev_sig
  );
/*----------------------------------------------------------------------------*/
extern bl_err ssc_in_q_produce(
  ssc_in_q* q, bl_u8* in_bstream, bool* idle_signal
  );
/*----------------------------------------------------------------------------*/

#endif /* __SSC_IN_QUEUE_H__ */

