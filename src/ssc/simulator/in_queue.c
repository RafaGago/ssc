
#include <bl/base/assert.h>
#include <bl/base/alignment.h>
#include <ssc/simulator/in_queue.h>

/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_init (ssc_in_q* q, uword queue_size, alloc_tbl const* alloc)
{
  q->last_op = mpmc_b_first_op;
  bl_err err = mpmc_bt_init(
    &q->queue, alloc, queue_size, sizeof (u8*), bl_alignof (u8*)
    );
  if (!err.bl) {
    bl_assert_side_effect(
      mpmc_bt_producer_signal_try_set_tmatch(
        &q->queue, &q->last_op, in_q_sig_idle
        ).bl == bl_ok);
  }
  return err;
}
/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_destroy (ssc_in_q* q, alloc_tbl const* alloc)
{
  u8* dat;
  while ((dat = ssc_in_q_try_consume (q))) { /*deliberate assingment*/
    bl_dealloc (alloc, dat);
  }
  mpmc_bt_destroy (&q->queue, alloc);
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
u8* ssc_in_q_try_consume (ssc_in_q* q)
{
  u8* ret = nullptr;
  mpmc_bt_consume_sc (&q->queue, &q->last_op, &ret);
  return ret;
}
/*---------------------------------------------------------------------------*/
bl_err ssc_in_q_block (ssc_in_q* q)
{
  mpmc_b_op expected = mpmc_b_op_encode (q->last_op, in_q_ok);
  while(
    mpmc_bt_producer_signal_try_set_tmatch(
      &q->queue, &expected, in_q_blocked
      ).bl == bl_preconditions
    );
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_try_switch_to_idle (ssc_in_q* q, ssc_in_q_sig* prev_sig)
{
  mpmc_b_op expected = q->last_op;
  bl_err err = mpmc_bt_producer_signal_try_set_tmatch(
    &q->queue, &expected, in_q_sig_idle
    );
  *prev_sig = mpmc_b_sig_decode (expected);
  return err;
}
/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_produce (ssc_in_q* q, u8* in_bstream, bool* idle)
{
  mpmc_b_op op;
  bl_err err = mpmc_bt_produce_sig_fallback(
    &q->queue, &op, &in_bstream, true, in_q_ok, in_q_blocked, in_q_blocked
    );
  if (err.bl == bl_ok) {
    *idle = (mpmc_b_sig_decode (op) == in_q_sig_idle);
  }
  else {
    err.bl = (err.bl != bl_preconditions) ? err.bl : bl_locked;
  }
  return err;
}
/*----------------------------------------------------------------------------*/

