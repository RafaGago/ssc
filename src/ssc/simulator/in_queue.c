
#include <bl/base/assert.h>
#include <bl/base/alignment.h>
#include <ssc/simulator/in_queue.h>

/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_init(
  ssc_in_q* q, bl_uword queue_size, bl_alloc_tbl const* alloc
  )
{
  q->last_op = bl_mpmc_b_first_op;
  bl_err err = bl_mpmc_bt_init(
    &q->queue, alloc, queue_size, sizeof (bl_u8*), bl_alignof (bl_u8*)
    );
  if (!err.bl) {
    bl_assert_side_effect(
      bl_mpmc_bt_producer_signal_try_set_tmatch(
        &q->queue, &q->last_op, in_q_sig_idle
        ).bl == bl_ok);
  }
  return err;
}
/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_destroy (ssc_in_q* q, bl_alloc_tbl const* alloc)
{
  bl_u8* dat;
  while ((dat = ssc_in_q_try_consume (q))) { /*deliberate assingment*/
    bl_dealloc (alloc, dat);
  }
  bl_mpmc_bt_destroy (&q->queue, alloc);
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
bl_u8* ssc_in_q_try_consume (ssc_in_q* q)
{
  bl_u8* ret = nullptr;
  bl_mpmc_bt_consume_sc (&q->queue, &q->last_op, &ret);
  return ret;
}
/*---------------------------------------------------------------------------*/
bl_err ssc_in_q_block (ssc_in_q* q)
{
  bl_mpmc_b_op expected = bl_mpmc_b_op_encode (q->last_op, in_q_ok);
  while(
    bl_mpmc_bt_producer_signal_try_set_tmatch(
      &q->queue, &expected, in_q_blocked
      ).bl == bl_preconditions
    );
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_try_switch_to_idle (ssc_in_q* q, ssc_in_q_sig* prev_sig)
{
  bl_mpmc_b_op expected = q->last_op;
  bl_err err = bl_mpmc_bt_producer_signal_try_set_tmatch(
    &q->queue, &expected, in_q_sig_idle
    );
  *prev_sig = bl_mpmc_b_sig_decode (expected);
  return err;
}
/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_produce (ssc_in_q* q, bl_u8* in_bstream, bool* idle)
{
  bl_mpmc_b_op op;
  bl_err err = bl_mpmc_bt_produce_sig_fallback(
    &q->queue, &op, &in_bstream, true, in_q_ok, in_q_blocked, in_q_blocked
    );
  if (err.bl == bl_ok) {
    *idle = (bl_mpmc_b_sig_decode (op) == in_q_sig_idle);
  }
  else {
    err.bl = (err.bl != bl_preconditions) ? err.bl : bl_locked;
  }
  return err;
}
/*----------------------------------------------------------------------------*/

