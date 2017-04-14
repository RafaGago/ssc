
#include <string.h>

#include <bl/base/assert.h>
#include <bl/base/alignment.h>
#include <bl/base/integer.h>
#include <bl/base/integer_manipulation.h>
#include <bl/base/integer_math.h>
#include <bl/base/deadline.h>
#include <bl/nonblock/backoff.h>

#include <ssc/simulator/out_queue.h>
#include <ssc/simulator/out_data_memory.h>
#include <ssc/simulator/global.h>
/*----------------------------------------------------------------------------*/
static inline word out_q_sorted_value_cmp(
  out_q_sorted_value const* a, out_q_sorted_value const* b
  )
{
  return memcmp (a, b, sizeof *a);
}
/*----------------------------------------------------------------------------*/
declare_flat_deadlines_funcs(
  out_q_sorted, out_q_sorted_value, static inline
  )
define_flat_deadlines_funcs(
  out_q_sorted, out_q_sorted_value, out_q_sorted_value_cmp, static inline
  )
/*----------------------------------------------------------------------------*/
bl_err ssc_out_q_init (ssc_out_q* q, uword size, ssc_global const* global)
{
  bl_assert (q && global);
  size       = round_next_pow2_u (size);
  bl_err err = mpmc_bt_init(
    &q->queue,
    global->alloc,
    size,
    sizeof (ssc_output_data),
    bl_alignof (ssc_output_data)
    );
  if (err) { return err; }
  err = out_q_sorted_init (&q->tsorted, size * 4, global->alloc);
  if (err) {
    mpmc_bt_destroy (&q->queue, global->alloc);
  }
  q->global = global;
  return err;
}
/*----------------------------------------------------------------------------*/
void ssc_out_q_destroy (ssc_out_q* q)
{
  bl_assert (q);
  ssc_output_data dat;
  uword           v;

  while (ssc_out_q_consume (q, &v, &dat, 1, 0) == bl_ok) {
    ssc_out_memory_dealloc(
      q->global->sim_dealloc, q->global->sim_context, &dat
      );
  }
  mpmc_bt_destroy (&q->queue, q->global->alloc);
  out_q_sorted_destroy (&q->tsorted, q->global->alloc);
}
/*----------------------------------------------------------------------------*/
bl_err ssc_out_q_produce (ssc_out_q* q, ssc_output_data* d)
{
  bl_assert (q && d);
  mpmc_b_op op;
  return mpmc_bt_produce_sp (&q->queue, &op, d);
}
/*----------------------------------------------------------------------------*/
static inline void copy_to_output_data(
  ssc_output_data* d, out_q_sorted_entry const* e
  )
{
  d->data = e->value.data;
  d->gid  = e->value.gid;
  d->type = e->value.type;
  d->time = e->key;
}
/*----------------------------------------------------------------------------*/
static inline void  copy_to_sorted_data(
  out_q_sorted_entry* e, ssc_output_data const* d
  )
{
   e->value.data = d->data;
   e->value.gid  = d->gid;
   e->value.type = d->type;
   e->key        = d->time;
}
/*----------------------------------------------------------------------------*/
static uword ssc_out_q_try_read (ssc_out_q* q, ssc_output_data* d, uword count)
{
  tstamp now         = bl_get_tstamp();
  uword  copied      = 0;
  uword  last_copied = 0;
  out_q_sorted_entry const* outdata;

try_again:
  while (copied < count) {
    outdata = out_q_sorted_get_head_if_expired_explicit (&q->tsorted, now);
    if (!outdata) {
      break;
    }
    copy_to_output_data (d + copied, outdata);
    out_q_sorted_drop_head (&q->tsorted);
    ++copied;
  }
  if ((copied - last_copied) == 0) {
    return copied;
  }
  last_copied = copied;
  now         = bl_get_tstamp(); /*trying to save syscalls ot the tstamp*/
  goto try_again; /* "while (1)" with no indentation */
}
/*----------------------------------------------------------------------------*/
static bool ssc_out_q_transfer (ssc_out_q* q)
{
  bl_err err = bl_ok;
  while (!err && out_q_sorted_can_insert (&q->tsorted)) {
    mpmc_b_op       op;
    ssc_output_data d;
    out_q_sorted_entry e;
    err = mpmc_bt_consume_sc (&q->queue, &op, &d);
    if (err) { break; }
    copy_to_sorted_data (&e, &d);
    out_q_sorted_insert (&q->tsorted, &e);
  }
  return out_q_sorted_can_insert (&q->tsorted);
}
/*----------------------------------------------------------------------------*/
bl_err ssc_out_q_consume(
  ssc_out_q*       q,
  uword*           d_consumed,
  ssc_output_data* d,
  uword            d_capacity,
  toffset          timeout_us
  )
{
  bl_assert (q && d && d_consumed);
  bl_assert (timeout_us >= 0);
  bl_assert (d_capacity > 0);

  tstamp           deadline;
  nonblock_backoff nb;
  bool             tsorted_not_full;

  deadline_init (&deadline, (u32) timeout_us);
  nonblock_backoff_init_default (&nb, timeout_us / 4);

try_again:
  tsorted_not_full = ssc_out_q_transfer (q);
  *d_consumed      = ssc_out_q_try_read (q, d, d_capacity);

  switch ((u_bitv (*d_consumed == 0, 1) | u_bitv (tsorted_not_full, 0))) {
  case 0:
    ssc_out_q_transfer (q); /*making room on the SPSC queue now if necessary*/
    return bl_ok;
  case 1:
    return bl_ok; /*fast-path*/
  case 2:{ /*edge case*/
    mpmc_b_op        op;
    ssc_output_data  d;
    out_q_sorted_entry e;
    bl_err err = mpmc_bt_consume_sc (&q->queue, &op, &d);
    if (!err) {
      out_q_sorted_entry const* drop = out_q_sorted_get_head (&q->tsorted);
      bl_assert (drop);
      copy_to_output_data (&d, drop);
      ssc_out_memory_dealloc(
        q->global->sim_dealloc, q->global->sim_context, &d
        );
      out_q_sorted_drop_head (&q->tsorted);
      copy_to_sorted_data (&e, &d);
      out_q_sorted_insert (&q->tsorted, &e);
      goto try_again;
    }
    break;
  }
  default: break;
  }
  d_capacity = 1; /*lowest latency to return something to the caller*/

  if (deadline_expired (deadline)) {
     return bl_timeout;
  }
  nonblock_backoff_run (&nb);
  goto try_again; /* "while (1)" with no indentation */
}
/*----------------------------------------------------------------------------*/
