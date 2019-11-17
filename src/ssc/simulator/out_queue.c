
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
typedef struct out_q_sorted_entry {
  bl_timept32        time;
  out_q_sorted_value value;
}
out_q_sorted_entry;
/*----------------------------------------------------------------------------*/
static inline bl_word out_q_sorted_value_cmp (void const* a, void const* b)
{
  out_q_sorted_entry const* ae = (out_q_sorted_entry const*) a;
  out_q_sorted_entry const* be = (out_q_sorted_entry const*) b;
  return memcmp (&ae->value, &be->value, sizeof ae->value);
}
/*----------------------------------------------------------------------------*/
bl_define_flat_deadlines_funcs(
  out_q_sorted, out_q_sorted_entry, out_q_sorted_value_cmp
  )
/*----------------------------------------------------------------------------*/
bl_err ssc_out_q_init (ssc_out_q* q, bl_uword size, ssc_global const* global)
{
  bl_assert (q && global);
  size       = bl_round_next_pow2_u (size);
  bl_err err = bl_mpmc_bt_init(
    &q->queue,
    global->alloc,
    size,
    sizeof (ssc_output_data),
    bl_alignof (ssc_output_data)
    );
  if (err.own) { return err; }
  err = out_q_sorted_init(
    &q->tsorted, bl_timept32_get(), size * 4, global->alloc
    );
  if (err.own) {
    bl_mpmc_bt_destroy (&q->queue, global->alloc);
  }
  q->global = global;
  return err;
}
/*----------------------------------------------------------------------------*/
void ssc_out_q_destroy (ssc_out_q* q)
{
  bl_assert (q);
  ssc_output_data dat;
  bl_uword        v;

  while (ssc_out_q_consume (q, &v, &dat, 1, 0).own == bl_ok) {
    ssc_out_memory_dealloc(
      q->global->sim_dealloc, q->global->sim_context, &dat
      );
  }
  bl_mpmc_bt_destroy (&q->queue, q->global->alloc);
  out_q_sorted_destroy (&q->tsorted, q->global->alloc);
}
/*----------------------------------------------------------------------------*/
bl_err ssc_out_q_produce (ssc_out_q* q, ssc_output_data* d)
{
  bl_assert (q && d);
  bl_mpmc_b_op op;
  return bl_mpmc_bt_produce_sp (&q->queue, &op, d);
}
/*----------------------------------------------------------------------------*/
static inline void copy_to_output_data(
  ssc_output_data* d, out_q_sorted_entry const* e
  )
{
  d->data = e->value.data;
  d->gid  = e->value.gid;
  d->type = e->value.type;
  d->time = e->time;
}
/*----------------------------------------------------------------------------*/
static inline void  copy_to_sorted_data(
  out_q_sorted_entry* e, ssc_output_data const* d
  )
{
   e->value.data = d->data;
   e->value.gid  = d->gid;
   e->value.type = d->type;
   e->time       = d->time;
}
/*----------------------------------------------------------------------------*/
static bl_uword
  ssc_out_q_try_read (ssc_out_q* q, ssc_output_data* d, bl_uword count)
{
  bl_timept32 now         = bl_timept32_get();
  bl_uword  copied      = 0;
  bl_uword  last_copied = 0;
  out_q_sorted_entry const* outdata;

try_again:
  while (copied < count) {
    outdata = out_q_sorted_get_head_if_expired (&q->tsorted, true, now);
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
  now         = bl_timept32_get(); /*trying to save syscalls ot the bl_timept32*/
  goto try_again; /* "while (1)" with no indentation */
}
/*----------------------------------------------------------------------------*/
static bool ssc_out_q_transfer (ssc_out_q* q)
{
  bl_err err = bl_mkok();
  while (!err.own && out_q_sorted_can_insert (&q->tsorted)) {
    bl_mpmc_b_op       op;
    ssc_output_data d;
    out_q_sorted_entry e;
    err = bl_mpmc_bt_consume_sc (&q->queue, &op, &d);
    if (err.own) { break; }
    copy_to_sorted_data (&e, &d);
    out_q_sorted_insert (&q->tsorted, &e);
  }
  return out_q_sorted_can_insert (&q->tsorted);
}
/*----------------------------------------------------------------------------*/
bl_err ssc_out_q_consume(
  ssc_out_q*       q,
  bl_uword*        d_consumed,
  ssc_output_data* d,
  bl_uword         d_capacity,
  bl_timeoft32     timeout_us
  )
{
  bl_assert (q && d && d_consumed);
  bl_assert (timeout_us >= 0);
  bl_assert (d_capacity > 0);

  bl_timept32           bl_deadline;
  bl_nonblock_backoff nb;
  bool             tsorted_not_full;

  bl_timept32_deadline_init_usec (&bl_deadline, (bl_u32) timeout_us);
  bl_nonblock_backoff_init_default (&nb, timeout_us / 4);

try_again:
  tsorted_not_full = ssc_out_q_transfer (q);
  *d_consumed      = ssc_out_q_try_read (q, d, d_capacity);

  switch ((bl_u_bitv (*d_consumed == 0, 1) | bl_u_bitv (tsorted_not_full, 0))) {
  case 0:
    ssc_out_q_transfer (q); /*making room on the SPSC queue now if necessary*/
    return bl_mkok();
  case 1:
    return bl_mkok(); /*fast-path*/
  case 2:{ /*edge case*/
    bl_mpmc_b_op       op;
    ssc_output_data    d;
    out_q_sorted_entry e;
    bl_err err = bl_mpmc_bt_consume_sc (&q->queue, &op, &d);
    if (!err.own) {
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

  if (bl_timept32_deadline_expired (bl_deadline)) {
     return bl_mkerr (bl_timeout);
  }
  bl_nonblock_backoff_run (&nb);
  goto try_again; /* "while (1)" with no indentation */
}
/*----------------------------------------------------------------------------*/
