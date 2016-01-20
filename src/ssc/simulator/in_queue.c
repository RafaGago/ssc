
#include <bl/base/assert.h>
#include <ssc/simulator/in_queue.h>

/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_init (ssc_in_q* q, uword queue_size, alloc_tbl const* alloc)
{
  q->last_inf.transaction = mpmc_b_unset_transaction;
  q->last_inf.signal      = 0;
  bl_err err = mpmc_b_init (&q->queue, alloc, queue_size, u8*);
  if (!err) {
    bl_assert_side_effect(
      mpmc_b_producer_signal_try_set_tmatch(
        &q->queue, &q->last_inf, in_q_sig_idle
        ) == bl_ok);
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
  mpmc_b_destroy (&q->queue, alloc);
}
/*----------------------------------------------------------------------------*/
u8* ssc_in_q_try_consume (ssc_in_q* q)
{
  mpmc_b_info inf;
  u8*         ret = nullptr;
  bl_err err = mpmc_b_consume_single_c (&q->queue, &q->last_inf, &ret);
  return ret;
}
/*---------------------------------------------------------------------------*/
bl_err ssc_in_q_block (ssc_in_q* q)
{
  mpmc_b_info expected;
  expected        = q->last_inf;
  expected.signal = in_q_ok;
  while(
    mpmc_b_producer_signal_try_set_tmatch(
      &q->queue, &expected, in_q_blocked
      ) == bl_preconditions
    );
  return bl_ok;
}
/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_try_switch_to_idle (ssc_in_q* q, ssc_in_q_sig* prev_sig)
{
  mpmc_b_info expected = q->last_inf;
  bl_err err = mpmc_b_producer_signal_try_set_tmatch(
    &q->queue, &expected, in_q_sig_idle
    );
  *prev_sig = expected.signal;
  return err;
}
/*----------------------------------------------------------------------------*/
bl_err ssc_in_q_produce (ssc_in_q* q, u8* in_bstream, bool* idle)
{
  mpmc_b_info inf;
  bl_err err = mpmc_b_produce_sig_fallback(
    &q->queue, &inf, &in_bstream, true, in_q_ok, in_q_blocked, in_q_blocked
    );
  if (err == bl_ok) {
    *idle = (inf.signal == in_q_sig_idle);
  }
  else {
    err = (err != bl_preconditions) ? err : bl_locked;
  }
  return err;
}
/*----------------------------------------------------------------------------*/

