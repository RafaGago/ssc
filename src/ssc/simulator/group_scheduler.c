#include <bl/base/integer_math.h>
#include <bl/base/static_integer_math.h>

#include <ssc/log.h>
#include <ssc/simulator/group_scheduler.h>
#include <ssc/simulator/in_bstream.h>

/*----------------------------------------------------------------------------*/
/* GENERIC DATA STRUCTURES */
/*----------------------------------------------------------------------------*/
bl_define_ringb_funcs (gsched_fiber_queue, bl_u8*);
/*----------------------------------------------------------------------------*/
typedef struct gsched_timed_entry {
  bl_timept32          time;
  gsched_timed_value value;
}
gsched_timed_entry;
/*----------------------------------------------------------------------------*/
static inline bl_word gsched_fibers_node_cmp (void const* a, void const* b)
{
  gsched_timed_entry const* av = (gsched_timed_entry const*) a;
  gsched_timed_entry const* bv = (gsched_timed_entry const*) b;
  return ((bl_word) av->value.fn) - ((bl_word) bv->value.fn);
}
/*----------------------------------------------------------------------------*/
bl_define_flat_deadlines_funcs(
  gsched_timed, gsched_timed_entry, gsched_fibers_node_cmp
  )
/*----------------------------------------------------------------------------*/
/* CONSTANTS */
/*----------------------------------------------------------------------------*/
enum gsched_queue_ids{
  q_run, /*queue for actually running fibers*/
  q_blocked, /*queue for blocked fibers*/
  q_queue, /*queue for blocked __on queue__ fibers*/
  q_count,
};
/*----------------------------------------------------------------------------*/
enum gsched_fiber_states{
  fstate_run, /*state for running or scheduler preempted fibers*/
  fstate_wait, /*state for fibers waiting synchronization (wake)*/
  fstate_onqueue, /*state for fibers that consumed its queue through blocking calls*/
  fstate_timer_reschedule, /*state for fibers just rescheduled by a timer*/
};
/*----------------------------------------------------------------------------*/
bl_static_assert_ns (bl_arr_elems_member (gsched, sq) == q_count);
#define gsched_foreach_state_queue(gs, vname)\
  for (gsched_fibers* vname = &(gs)->sq[0]; vname < &(gs)->sq[q_count]; ++vname)
/*----------------------------------------------------------------------------*/
/* FIBERS */
/*----------------------------------------------------------------------------*/
static inline void node_queue_transfer_tail(
  gsched_fibers* to, gsched_fibers* from, gsched_fibers_node* n
  )
{
  bl_tailq_remove (from, n, hook);
  bl_tailq_insert_tail (to, n, hook);
}
/*----------------------------------------------------------------------------*/
static inline void fiber_node_yield_to_sched (gsched_fibers_node* fn)
{
  fn->fiber.state.func_count = 0;
  ssc_global* global         = fn->fiber.parent->global;
#ifdef SSC_BEFORE_FIBER_CONTEXT_SWITCH_EVT
  global->sim_before_fiber_context_switch (global->sim_context);
#endif
  coro_transfer (&fn->fiber.coro_ctx, &global->main_coro_ctx);
}
/*----------------------------------------------------------------------------*/
static void gsched_fiber_drop_all_input (gsched_fiber* f);
/*----------------------------------------------------------------------------*/
static void fiber_function (void* arg)
{
  gsched_fibers_node* f = (gsched_fibers_node*) arg;
  f->fiber.cfg.fiber(
    f, f->fiber.cfg.context, f->fiber.parent->global->sim_context
    );
  node_queue_transfer_tail(
    &f->fiber.parent->finished, &f->fiber.parent->sq[q_run], f
    );
  bl_uword produce_only = (bl_uword) fiber_is_produce_only(
    f->fiber.cfg.run_cfg.run_flags
    );
  if (!produce_only) {
    gsched_fiber_drop_all_input (&f->fiber);
  }
  f->fiber.parent->produce_only_fibers -= produce_only;
  --f->fiber.parent->active_fibers;
  fiber_node_yield_to_sched (f); /*we can't return on libcoro*/
}
/*----------------------------------------------------------------------------*/
static inline bl_err fiber_init(
  gsched_fibers_node*  fn,
  bl_timept32            t,
  gsched*              parent,
  bl_u8**              queue_mem,
  ssc_fiber_cfg const* cfg
  )
{
  gsched_fiber* f = &fn->fiber;
  memset (f, 0, sizeof *f);
  f->parent        = parent;
  bl_uword stack_size = bl_div_ceil (cfg->min_stack_size, sizeof (void*));
  int coro_noerr   = coro_stack_alloc (&f->stack, stack_size);
  if (!coro_noerr) {
    return bl_mkerr (bl_alloc);
  }
  coro_create(
    &f->coro_ctx, fiber_function, fn, f->stack.sptr, stack_size
    );
  gsched_fiber_queue_init_extern (&f->queue, queue_mem, cfg->min_queue_size);

  f->cfg.fiber    = cfg->fiber;
  f->cfg.teardown = cfg->teardown;
  f->cfg.context  = cfg->fiber_context;
  f->cfg.run_cfg  = cfg->run_cfg;
  f->state.id     = fstate_run;
  f->state.time   = t;
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
static void gsched_fiber_drop_input_head (gsched_fiber* f)
{
  if (bl_unlikely (gsched_fiber_queue_size (&f->queue) <= 0 ||
      fiber_is_produce_only (f->cfg.run_cfg.run_flags))
    ) {
    bl_assert(
      !fiber_is_produce_only (f->cfg.run_cfg.run_flags) &&
      "produce only fiber is trying to drop"
      );
    return;
  }
  bl_u8* in_bstream = *gsched_fiber_queue_at_head (&f->queue);
  bl_u8* refc       = in_bstream_refcount (in_bstream);
  gsched_fiber_queue_drop_head (&f->queue);
  bl_assert (*refc > 0);
  --*refc;
  if (!*refc) {
    bl_dealloc (f->parent->global->alloc, in_bstream);
  }
}
/*----------------------------------------------------------------------------*/
static void gsched_fiber_drop_all_input (gsched_fiber* f)
{
  while (gsched_fiber_queue_size (&f->queue) > 0) {
    gsched_fiber_drop_input_head (f);
  }
}
/*----------------------------------------------------------------------------*/
static void fiber_destroy (gsched_fiber* f)
{
  gsched_fiber_drop_all_input (f);
  coro_stack_free (&f->stack);
}
/*----------------------------------------------------------------------------*/
static void fiber_run_teardown (gsched_fiber* f)
{
  while (gsched_fiber_queue_size (&f->queue) > 0) {
    gsched_fiber_drop_input_head (f);
  }
  if (f->cfg.teardown) {
    f->cfg.teardown (f->cfg.context, f->parent->global->sim_context);
  }
}
/*----------------------------------------------------------------------------*/
static inline bl_uword fibers_get_chunk_size (ssc_fiber_cfgs const* fiber_cfgs)
{
  bl_uword size = 0;
  for (bl_uword i = 0; i < ssc_fiber_cfgs_size (fiber_cfgs); ++i) {
    bl_static_assert_ns(
      bl_next_offset_aligned_to_type (sizeof (gsched_fibers_node), bl_u8*) ==
      sizeof (gsched_fibers_node)
      );
    bl_uword fsize;
    fsize  = sizeof (gsched_fibers_node);
    fsize += ssc_fiber_cfgs_at (fiber_cfgs, i)->min_queue_size * sizeof (bl_u8*);
    fsize  = bl_next_offset_aligned_to_type (fsize, gsched_fibers_node);
    size  += fsize;
  }
  return size;
}
/*----------------------------------------------------------------------------*/
static void run_wake (gsched* gs, bl_uword_d2 id, bl_uword_d2 count, bl_timept32 now)
{
  gsched_fibers_node* next = bl_tailq_first (&gs->sq[q_blocked]);
  while (next && count != 0) {
    gsched_fibers_node* n = next;
    next                  = bl_tailq_next (next, hook);

    if (n->fiber.state.id != fstate_wait ||
        n->fiber.state.params.wait.id != id ||
        bl_timept32_get_diff (now, n->fiber.state.params.wait.execute_time) < 0
      ) {
      continue;
    }
    node_queue_transfer_tail (&gs->sq[q_run], &gs->sq[q_blocked], n);
    --count;
  }
}
/*----------------------------------------------------------------------------*/
/* SIMULATION INTERFACE */
/*----------------------------------------------------------------------------*/
static inline void fiber_node_program_timed(
  gsched* gs, gsched_fibers_node* n, bl_timept32 t
  )
{
  gsched_timed_entry e;
  e.time     = t;
  e.value.fn = n;
  bl_assert (gsched_timed_can_insert (&gs->timed));
  gsched_timed_insert (&gs->timed, &e);
}
/*----------------------------------------------------------------------------*/
static inline void fiber_node_cancel_timed(
  gsched* gs, gsched_fibers_node* n, bl_timept32 t
  )
{
  gsched_timed_entry tv, dummy;
  tv.time     = t;
  tv.value.fn = n;
  gsched_timed_try_get_and_drop (&gs->timed, &dummy, &tv);
}
/*----------------------------------------------------------------------------*/
static void fiber_node_yield_until_fiber_time(
  gsched* gs, gsched_fibers_node* fn
  )
{
  bl_assert (fn->fiber.state.id == fstate_run);
  fiber_node_program_timed (gs, fn, fn->fiber.state.time);
  node_queue_transfer_tail (&gs->sq[q_blocked], &gs->sq[q_run], fn);
  fiber_node_yield_to_sched (fn);
  bl_assert (fn->fiber.state.id == fstate_timer_reschedule);
  fn->fiber.state.id = fstate_run;
}
/*----------------------------------------------------------------------------*/
static void fiber_node_forward_progress_limit(
  gsched* gs, gsched_fibers_node* fn
  )
{
  bl_timeoft32 max_offset = bl_usec_to_timept32(
    fn->fiber.cfg.run_cfg.look_ahead_offset_us
    );
  if (bl_timept32_get_diff (fn->fiber.state.time, gs->vars.now + max_offset) >= 0) {
    fiber_node_yield_until_fiber_time (fn->fiber.parent, fn);
  }
  else if (fn->fiber.state.func_count > fn->fiber.cfg.run_cfg.max_func_count) {
    fiber_node_yield_to_sched (fn);
  }
  ++fn->fiber.state.func_count;
}
/*----------------------------------------------------------------------------*/
void ssc_api_yield (ssc_handle h)
{
  fiber_node_yield_to_sched ((gsched_fibers_node*) h);
}
/*----------------------------------------------------------------------------*/
void ssc_api_wake (ssc_handle h, bl_uword_d2 wait_id, bl_uword_d2 count)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;
  bl_timept32diff tdiff;
retry:
  tdiff = bl_timept32_get_diff (fn->fiber.state.time, gs->vars.now);
  if (tdiff == 0) {
    run_wake (gs, wait_id, count, gs->vars.now);
  }
  else if (gsched_timed_can_insert (&gs->future_wakes)) {
    bl_assert (tdiff > 0);
    gsched_timed_entry e;
    e.time             = fn->fiber.state.time;
    e.value.wake.id    = wait_id;
    e.value.wake.count = count;
    gsched_timed_insert (&gs->future_wakes, &e);
  }
  else {
    fiber_node_yield_until_fiber_time (gs, fn);
    goto retry;
  }
  fiber_node_forward_progress_limit (gs, fn);
}
/*----------------------------------------------------------------------------*/
bool ssc_api_wait (ssc_handle h, bl_uword_d2 wait_id, bl_timeoft32 us)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;

  fn->fiber.state.id                       = fstate_wait;
  fn->fiber.state.params.wait.execute_time = fn->fiber.state.time;
  fn->fiber.state.params.wait.id           = wait_id;
  node_queue_transfer_tail (&gs->sq[q_blocked], &gs->sq[q_run], fn);

  if (us != 0) {
    gsched_timed_entry e;
    e.time     = fn->fiber.state.time + bl_usec_to_timept32 (us);
    e.value.fn = fn;
    bl_assert_side_effect(
      gsched_timed_insert (&fn->fiber.parent->timed, &e).bl == bl_ok
      );
  }
  fiber_node_yield_to_sched (fn);
  bool ret           = fn->fiber.state.id != fstate_timer_reschedule;
  fn->fiber.state.id = fstate_run;
  return ret;
}
/*----------------------------------------------------------------------------*/
bl_memr16 ssc_api_try_peek_input_head (ssc_handle h)
{
  /*WARNING: Don't call "fiber_node_forward_progress_limit" with the current
    implementation, as it is used inside another api functions.
    */
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  if (gsched_fiber_queue_size (&fn->fiber.queue)) {
    bl_u8* in_bstream = *gsched_fiber_queue_at_head (&fn->fiber.queue);
    return bl_memr16_rv(
        in_bstream_payload (in_bstream),
        *in_bstream_payload_size (in_bstream)
        );
  }
  else {
    return bl_memr16_null();
  }
}
/*----------------------------------------------------------------------------*/
bl_memr16 ssc_api_peek_input_head (ssc_handle h)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;

  bl_memr16 ret = ssc_api_try_peek_input_head (h);
  if (!bl_memr16_is_null (ret)) {
    fiber_node_forward_progress_limit (gs, fn);
    return ret;
  }
  fn->fiber.state.id                 = fstate_onqueue;
  fn->fiber.state.params.qread.match = nullptr;
  fn->fiber.state.params.qread.mask  = nullptr;

  node_queue_transfer_tail (&gs->sq[q_queue], &gs->sq[q_run], fn);
  fiber_node_yield_to_sched (fn);
  /*will be rescheduled when some data is available*/
  fn->fiber.state.id = fstate_run;
  ret                = ssc_api_try_peek_input_head (h);
  bl_assert (!bl_memr16_is_null (ret) && "critical bug or design error");
  return ret;
}
/*----------------------------------------------------------------------------*/
bl_memr16 ssc_api_timed_peek_input_head (ssc_handle h, bl_timeoft32 us)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;

  bl_memr16 ret = ssc_api_try_peek_input_head (h);
  if (!bl_memr16_is_null (ret) || us == 0) {
    fiber_node_forward_progress_limit (gs, fn);
    return ret;
  }
  fn->fiber.state.id                 = fstate_onqueue;
  fn->fiber.state.params.qread.match = nullptr;
  fn->fiber.state.params.qread.mask  = nullptr;

  bl_timept32 timeout_deadline = fn->fiber.state.time + bl_usec_to_timept32 (us);
  fiber_node_program_timed (gs, fn, timeout_deadline);
  node_queue_transfer_tail (&gs->sq[q_queue], &gs->sq[q_run], fn);
  fiber_node_yield_to_sched (fn);
  /*will be rescheduled when some data is available or after timing out*/
  fn->fiber.state.id = fstate_run;
  ret                = ssc_api_try_peek_input_head (h);
  if (!bl_memr16_is_null (ret)) { /*no timeout: self remove from the timed queue*/
    fiber_node_cancel_timed (gs, fn, timeout_deadline);
  }
  return ret;
}
/*----------------------------------------------------------------------------*/
bool ssc_api_pattern_match (bl_memr16 in, bl_memr16 match)
{
  bl_assert (bl_memr16_is_valid (match) && bl_memr16_is_valid (in));
  if (bl_memr16_size (in) < bl_memr16_size (match) ||
      bl_memr16_size (in) == 0 ||
      bl_memr16_size (match) == 0
    ) {
    return false;
  }
  return memcmp (bl_memr16_beg (in), bl_memr16_beg (match), bl_memr16_size (match)) == 0;
}
/*----------------------------------------------------------------------------*/
bool ssc_api_pattern_match_mask (bl_memr16 in, bl_memr16 match, bl_memr16 mask)
{
  bl_assert(
    bl_memr16_is_valid (match) && bl_memr16_is_valid (mask) && bl_memr16_is_valid (in)
    );
  if (bl_memr16_size (in) < bl_memr16_size (match) ||
      bl_memr16_size (in) == 0 ||
      bl_memr16_size (match) == 0
    ) {
    return false;
  }
  bl_uword with_mask = bl_min (bl_memr16_size (match), bl_memr16_size (mask));
  bl_uword align_dat = 0;
  bl_uword align_cmp = 0;
  bl_uword i         = 0;

#if BL_WORDSIZE == 64
  bl_uword first_run = with_mask & ~(sizeof (bl_uword) - 1);
  while (i < first_run) {
    ((bl_u8*) &align_dat)[0]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[0] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[0]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[1]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[1] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[1]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[2]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[2] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[2]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[3]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[3] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[3]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[4]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[4] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[4]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[5]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[5] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[5]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[6]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[6] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[6]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[7]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[7] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[7]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    if (align_dat != align_cmp) {
      return false;
    }
  }
#elif BL_WORDSIZE == 32
  bl_uword first_run = with_mask & ~(sizeof (bl_uword) - 1);
  while (i < first_run) {
    ((bl_u8*) &align_dat)[0]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[0] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[0]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[1]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[1] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[1]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[2]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[2] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[2]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    ((bl_u8*) &align_dat)[3]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[3] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[3]  = *bl_memr16_at_as (match, i, bl_u8);
    ++i;
    if (align_dat != align_cmp) {
      return false;
    }
  }
#else
#error "implement for the remaining platforms"
#endif
  for (bl_uword j = 0; i < with_mask; ++i, ++j) {
    ((bl_u8*) &align_dat)[j]  = *bl_memr16_at_as (in, i, bl_u8);
    ((bl_u8*) &align_dat)[j] &= *bl_memr16_at_as (mask, i, bl_u8);
    ((bl_u8*) &align_cmp)[j]  = *bl_memr16_at_as (match, i, bl_u8);
  }
  if (align_dat != align_cmp) {
    return false;
  }
  bl_uword with_no_mask = bl_memr16_size (match) - with_mask;
  return with_no_mask == 0 ||
    memcmp (bl_memr16_at (in, i), bl_memr16_at (match, i), with_no_mask) == 0;
}
/*----------------------------------------------------------------------------*/
static bool gsched_fiber_try_peek_input_head_match_mask(
  gsched_fibers_node* fn, bl_memr16 match, bl_memr16 mask
  )
{
  while (true) {
    bl_memr16 dat = ssc_api_try_peek_input_head (fn);
    if (bl_memr16_is_null (dat)) {
      return false;
    }
    if (ssc_api_pattern_match_mask (dat, match, mask)) {
      return true;
    }
    gsched_fiber_drop_input_head (&fn->fiber);
  }
}
/*----------------------------------------------------------------------------*/
static bool gsched_fiber_try_peek_input_head_match(
  gsched_fibers_node* fn, bl_memr16 match
  )
{
  while (true) {
    bl_memr16 dat = ssc_api_try_peek_input_head (fn);
    if (bl_memr16_is_null (dat)) {
      return false;
    }
    if (ssc_api_pattern_match (dat, match)) {
      return true;
    }
    gsched_fiber_drop_input_head (&fn->fiber);
  }
}
/*----------------------------------------------------------------------------*/
static bl_memr16 ssc_api_peek_input_head_match_mask_impl(
  ssc_handle h,
  bl_memr16       match,
  bl_memr16       mask,
  bl_timeoft32      us,
  bool         timed
  )
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;

  if (bl_memr16_is_null (match)) {
    return ssc_api_timed_peek_input_head (h, us);
  }
  bl_memr16 ret;
  bool found;
  if (bl_memr16_is_null (mask)) {
    found = gsched_fiber_try_peek_input_head_match (fn, match);
  }
  else {
    found = gsched_fiber_try_peek_input_head_match_mask (fn, match, mask);
  }
  if (found) {
    fiber_node_forward_progress_limit (gs, fn);
    return ssc_api_try_peek_input_head (h);
  }
  else if (timed && us == 0) {
    return bl_memr16_null();
  }
  fn->fiber.state.id                      = fstate_onqueue;
  fn->fiber.state.params.qread.match      = bl_memr16_beg_as (match, bl_u8);
  fn->fiber.state.params.qread.match_size = bl_memr16_size (match);
  fn->fiber.state.params.qread.mask       = bl_memr16_beg_as (mask, bl_u8);
  fn->fiber.state.params.qread.mask_size  = bl_memr16_size (mask);

  bl_timept32 timeout_deadline;
  if (timed) {
    timeout_deadline = fn->fiber.state.time + bl_usec_to_timept32 (us);
    fiber_node_program_timed (gs, fn, timeout_deadline);
  }
  node_queue_transfer_tail (&gs->sq[q_queue], &gs->sq[q_run], fn);
  fiber_node_yield_to_sched (fn);
  /*will be rescheduled when matching data is available or after timing out*/
  fn->fiber.state.id = fstate_run;
  ret = ssc_api_try_peek_input_head (h);
  if (!bl_memr16_is_null (ret)) { /*no timeout: self remove from the timed queue*/
    fiber_node_cancel_timed (gs, fn, timeout_deadline);
  }
  return ret;
}
/*----------------------------------------------------------------------------*/
bl_memr16 ssc_api_timed_peek_input_head_match_mask(
  ssc_handle h, bl_memr16 match, bl_memr16 mask, bl_timeoft32 us
  )
{
  return ssc_api_peek_input_head_match_mask_impl(
    h, match, mask, us, true
    );
}
/*----------------------------------------------------------------------------*/
bl_memr16 ssc_api_peek_input_head_match_mask(
  ssc_handle h, bl_memr16 match, bl_memr16 mask
  )
{
  return ssc_api_peek_input_head_match_mask_impl(
    h, match, mask, 0, false
    );
}
/*----------------------------------------------------------------------------*/
void ssc_api_drop_input_head (ssc_handle h)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;
  gsched_fiber_drop_input_head (&fn->fiber);
  fiber_node_forward_progress_limit (gs, fn);
}
/*----------------------------------------------------------------------------*/
void ssc_api_drop_all_input(ssc_handle h)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;
  gsched_fiber_drop_all_input (&fn->fiber);
  fiber_node_forward_progress_limit (gs, fn);
}
/*----------------------------------------------------------------------------*/
void ssc_api_produce_error(
  ssc_handle h, bl_err err, char const* static_string
  )
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;

  ssc_output_data dat;
  dat.gid  = fn->fiber.parent->gid;
  dat.type = ssc_type_error;
  dat.data = bl_memr16_rv ((void*) static_string, err.bl);
  dat.time = fn->fiber.state.time;

  bl_err e = ssc_out_q_produce (&gs->global->out_queue, &dat);
  log_error_if(
    e.bl != bl_ok, "unable to produce output data on fiber: %s", bl_strerror (e)
    );
  fiber_node_forward_progress_limit (gs, fn);
}
/*----------------------------------------------------------------------------*/
static void ssc_produce_output_impl (ssc_handle h, bl_memr16 b, bool dyn)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;

  ssc_output_data dat;
  dat.gid  = fn->fiber.parent->gid;
  dat.type = ssc_type_bytes | (dyn ? ssc_type_is_dynamic_mask : 0);
  dat.data = b;
  dat.time = fn->fiber.state.time;

  bl_err e = ssc_out_q_produce (&gs->global->out_queue, &dat);
  log_error_if(
    e.bl != bl_ok, "unable to produce output data on fiber: %s", bl_strerror (e)
    );
  fiber_node_forward_progress_limit (gs, fn);
}
/*----------------------------------------------------------------------------*/
static void ssc_produce_string_impl(
  ssc_handle h, char const* str, bl_uword size_incl_trail_null, bool dyn
  )
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*             gs = fn->fiber.parent;

  if (size_incl_trail_null == 0) {
    size_incl_trail_null = strlen (str) + 1;
  }
  else {
    bl_assert (strlen (str) == size_incl_trail_null - 1);
  }
  ssc_output_data dat;
  dat.gid  = fn->fiber.parent->gid;
  dat.type = ssc_type_string | (dyn ? ssc_type_is_dynamic_mask : 0);
  dat.data = bl_memr16_rv ((void*) str, size_incl_trail_null);
  dat.time = fn->fiber.state.time;

  bl_err e = ssc_out_q_produce (&gs->global->out_queue, &dat);
  log_error_if(
    e.bl != bl_ok, "unable to produce output data on fiber: %s", bl_strerror (e)
    );
  fiber_node_forward_progress_limit (gs, fn);
}
/*----------------------------------------------------------------------------*/
void ssc_api_produce_static_output (ssc_handle h, bl_memr16 b)
{
  ssc_produce_output_impl (h, b, false);
}
/*----------------------------------------------------------------------------*/
void ssc_api_produce_dynamic_output (ssc_handle h, bl_memr16 b)
{
  ssc_produce_output_impl (h, b, true);
}
/*----------------------------------------------------------------------------*/
void ssc_api_produce_static_string(
  ssc_handle h, char const* str, bl_uword size_incl_trail_null
  )
{
  ssc_produce_string_impl (h, str, size_incl_trail_null, false);
}
/*----------------------------------------------------------------------------*/
void ssc_api_produce_dynamic_string(
  ssc_handle h, char const* str, bl_uword size_incl_trail_null
  )
{
  ssc_produce_string_impl (h, str, size_incl_trail_null, true);
}
/*----------------------------------------------------------------------------*/
void ssc_api_delay (ssc_handle h, bl_timeoft32 us)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*            gs  = fn->fiber.parent;
  fn->fiber.state.time  += bl_usec_to_timept32 (us);
  fiber_node_forward_progress_limit (gs, fn);
}
/*----------------------------------------------------------------------------*/
bl_timept32 ssc_api_get_timestamp (ssc_handle h)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  return fn->fiber.state.time;
}
/*----------------------------------------------------------------------------*/
ssc_fiber_run_cfg ssc_api_fiber_get_run_cfg (ssc_handle h)
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  return fn->fiber.cfg.run_cfg;
}
/*----------------------------------------------------------------------------*/
static inline void set_fiber_as_produce_only (gsched_fibers_node* fn)
{

  fn->fiber.cfg.run_cfg.run_flags |= bl_u8_bit (ssc_fiber_produce_only);
}
/*----------------------------------------------------------------------------*/
static bool fiber_run_cfg_is_valid (ssc_fiber_run_cfg const* cfg)
{
  return cfg->max_func_count != 0 &&
         cfg->run_flags <= ((1 << ssc_fiber_flags_biggest) - 1);
}
/*----------------------------------------------------------------------------*/
bl_err ssc_api_fiber_set_run_cfg(
  ssc_handle h, ssc_fiber_run_cfg const* c
  )
{
  gsched_fibers_node* fn = (gsched_fibers_node*) h;
  gsched*            gs  = fn->fiber.parent;
  bl_assert (c);
  /*produce only can't be disabled*/
  bl_u8 oldflags = fn->fiber.cfg.run_cfg.run_flags;
  if (!fiber_run_cfg_is_valid (c) ||
    (fiber_is_produce_only (oldflags) && !fiber_is_produce_only (c->run_flags))
    ) {
    return bl_mkerr (bl_invalid);
  }
  fn->fiber.cfg.run_cfg = *c;
  if (fiber_is_produce_only (c->run_flags)) {
    gsched_fiber_drop_all_input (&fn->fiber);
    ++gs->produce_only_fibers;
  }
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
/* GROUP SCHEDULER */
/*----------------------------------------------------------------------------*/
bl_err gsched_init(
  gsched*                    gs,
  ssc_group_id               id,
  ssc_global*                global,
  ssc_fiber_group_cfg const* fgroup_cfg,
  ssc_fiber_cfgs const*      fiber_cfgs,
  bl_alloc_tbl const*        alloc
  )
{
  bl_assert (gs && global && fgroup_cfg && fiber_cfgs && alloc);
  memset (gs, 0, sizeof *gs);

  gs->gid                 = id;
  gs->global              = global;
  gs->fiber_cfgs          = fiber_cfgs;
  gs->active_fibers       = ssc_fiber_cfgs_size (fiber_cfgs);
  gs->produce_only_fibers = 0;
  gs->vars.now            = bl_timept32_get();
  gs->vars.has_prog       = false;

  gsched_foreach_state_queue (gs, q) {
    bl_tailq_init (q);
  }
  bl_tailq_init (&gs->finished);

  bl_err err = bl_mkok();
  bl_uword size = fibers_get_chunk_size (fiber_cfgs);
  if (size == 0) {
    return bl_mkerr (bl_invalid);
  }
  /*fiber chunk allocation*/
  gs->mem_chunk = bl_alloc (alloc, size);
  if (!gs->mem_chunk) {
    return bl_mkerr (bl_alloc);
  }
#ifndef NDEBUG
  memset (gs->mem_chunk, 0xaa, size);
#endif

  bl_u8* addr                 = gs->mem_chunk;
  gsched_fibers_node* node = nullptr;

  /*fiber initialization*/
  for (bl_uword i = 0; i < ssc_fiber_cfgs_size (fiber_cfgs); ++i) {

    ssc_fiber_cfg*      cfg  = ssc_fiber_cfgs_at (fiber_cfgs, i);
    gsched_fibers_node* next = (gsched_fibers_node*) (addr);
    bl_u8** queue_elems = (bl_u8**) (((bl_u8*) next) + sizeof *next);

    addr = ((bl_u8*) queue_elems) + (cfg->min_queue_size * sizeof *queue_elems);
    addr = (bl_u8*) bl_next_offset_aligned_to_type(
      (bl_uword) addr, gsched_fibers_node
      );
    err = fiber_init (next, gs->vars.now, gs, queue_elems, cfg);
    if (err.bl) {
      goto rollback;
    }
    if (node) {
      bl_tailq_insert_after (&gs->finished, node, next, hook);
    }
    else {
      bl_tailq_insert_head (&gs->finished, next, hook);
    }
    node = next;
  }
  /*MPSC init*/
  err = ssc_in_q_init(
    &gs->queue,bl_round_next_pow2_u (fgroup_cfg->min_queue_size), alloc
    );
  if (err.bl) {
    goto rollback;
  }
  bl_timept32 now = bl_timept32_get();
  /*timed item queue*/
  err = gsched_timed_init(
    &gs->timed,
    now,
    bl_next_pow2_u (ssc_fiber_cfgs_size (fiber_cfgs)),
    alloc
    );
  if (err.bl) {
    goto destroy_q;
  }
  /*future wakes queue*/
  err = gsched_timed_init (&gs->future_wakes, now, 32, alloc);
  if (err.bl) {
    goto destroy_timed;
  }
  return err;

destroy_timed:
  gsched_timed_destroy (&gs->timed, alloc);

destroy_q:
  ssc_in_q_destroy (&gs->queue, alloc);

rollback:
  bl_tailq_foreach (node, &gs->sq[q_blocked], hook) { /*variable reuse*/
    fiber_destroy (&node->fiber);
  }
  bl_tailq_init (&gs->sq[q_blocked]);
  bl_dealloc (alloc, gs->mem_chunk);
  gs->mem_chunk = nullptr;
  return err;
}
/*----------------------------------------------------------------------------*/
void gsched_destroy (gsched* gs, bl_alloc_tbl const* alloc)
{
  gsched_timed_destroy (&gs->future_wakes, alloc);
  gsched_timed_destroy (&gs->timed, alloc);
  ssc_in_q_destroy (&gs->queue, alloc);

  gsched_fibers_node* fn;
  bl_tailq_foreach (fn, &gs->finished, hook) {
    fiber_destroy (&fn->fiber);
  }
  gsched_foreach_state_queue (gs, q) {
    bl_tailq_foreach (fn, q, hook) {
      fiber_destroy (&fn->fiber);
    }
  }
  if (gs->mem_chunk) {
    bl_dealloc (alloc, gs->mem_chunk);
  }
}
/*----------------------------------------------------------------------------*/
static void gsched_fibers_transfer_tail_all(
  gsched_fibers* to, gsched_fibers* from
  )
{
  gsched_fibers_node* n    = bl_tailq_first (from);
  gsched_fibers_node* last = bl_tailq_last (to, gsched_fibers);
  if (!n) {
    return;
  }
  to->tqh_last        = &bl_tailq_last (from, gsched_fibers);
  last->hook.tqe_next = n;
  n->hook.tqe_prev    = &last->hook.tqe_next;
  bl_tailq_init (from);
#if 0
  /*macro substitution of bl_tailq merge*/
  if ((n->hook.tqe_next = last->hook.tqe_next) != nullptr) {
    n->hook.tqe_next->hook.tqe_prev = &n->hook.tqe_next;
  }
  else {
    to->tqh_last = &n->hook.tqe_next;
  }
  last->hook.tqe_next = n;
  n->hook.tqe_prev    = &last->hook.tqe_next;
#endif
}
/*----------------------------------------------------------------------------*/
bl_err gsched_run_setup (gsched* gs)
{
  bl_err err = bl_mkok();
  bl_uword i    = 0;
  bl_uword j;

  for (bl_uword i = 0; i < bl_arr_elems_member (gsched, sq); ++i) {
    if (!bl_tailq_empty (&gs->sq[i])) {
      return bl_mkerr (bl_preconditions);
    }
  }
  gsched_fibers_node* fn, *next;
  next = bl_tailq_first (&gs->finished);
  bl_assert (next);
  while (next) {
    fn   = next;
    next = bl_tailq_next (next, hook);
    ssc_fiber_setup_func f = ssc_fiber_cfgs_at (gs->fiber_cfgs, i)->setup;
    if (f) {
      err = f (fn->fiber.cfg.context, fn->fiber.parent->global->sim_context);
    }
    if (err.bl) {
      goto rollback;
    }
    ++i;
    node_queue_transfer_tail (&gs->sq[q_run], &gs->finished, fn);
  }
  err = gsched_program_schedule (gs);
  if (err.bl) {
    goto rollback;
  }
  return err;

rollback:
  fn = bl_tailq_first (&gs->sq[q_run]);
  for (j = 0; j < i; ++j) {
    fiber_run_teardown (&fn->fiber);
    fn = bl_tailq_next (fn, hook);
  }
  gsched_fibers_transfer_tail_all (&gs->sq[q_run], &gs->finished);
  gsched_fibers_transfer_tail_all (&gs->finished, &gs->sq[q_run]);
  return err;
}
/*----------------------------------------------------------------------------*/
void gsched_run_teardown (gsched* gs)
{
  gsched_fibers_node* fn;
  bl_tailq_foreach (fn, &gs->finished, hook) {
    fiber_run_teardown (&fn->fiber);
  }
  gsched_foreach_state_queue (gs, q) {
    bl_tailq_foreach (fn, q, hook) {
      fiber_run_teardown (&fn->fiber);
    }
  }
}
/*----------------------------------------------------------------------------*/
bl_err gsched_fiber_cfg_validate_correct (ssc_fiber_cfg* cfg)
{
  bl_assert (cfg);
  if (cfg->min_stack_size == 0 ||
      cfg->min_queue_size == 0 ||
      !fiber_run_cfg_is_valid (&cfg->run_cfg)
    ) {
    return bl_mkerr (bl_invalid);
  }
  cfg->min_queue_size =bl_round_next_pow2_u (cfg->min_queue_size);
  return bl_mkok();
}
/*----------------------------------------------------------------------------*/
static inline bl_uword gsched_consume_inputs (gsched* gs, bl_timept32* now)
{
  bl_u8*   input[16];
  bl_uword count = 0;
  bl_static_assert_ns (bl_is_pow2 (bl_arr_elems (input)));
  bl_uword idx;
  /*consume inputs from the outside*/
  do {
    idx = 0;
    if (gs->vars.unhandled_bstream) {
      bl_static_assert_ns (bl_arr_elems (input) > 1);
      input[idx] = gs->vars.unhandled_bstream;
      ++idx;
      gs->vars.unhandled_bstream = nullptr;
    }
    do {
      input[idx] = ssc_in_q_try_consume (&gs->queue);
      if (input[idx]) {
        *in_bstream_refcount (input[idx]) =
          gs->active_fibers - gs->produce_only_fibers;
        ++idx;
      }
      else {
        break;
      }
    }
    while (idx < bl_arr_elems (input));

    if (idx == 0) {
      break;
    }
    /*send data to fibers*/
    gsched_foreach_state_queue (gs, q) { /*iterate all the state queues*/
      gsched_fibers_node* n;
      bl_tailq_foreach (n, q, hook) { /*iterate every fiber in every state queue*/
        if (bl_unlikely(
          fiber_is_produce_only (n->fiber.cfg.run_cfg.run_flags)
          )) {
          continue;
        }
        for (bl_uword i = 0; i < idx; ++i) {
          if (!gsched_fiber_queue_can_insert (&n->fiber.queue)) {
            gsched_fiber_drop_input_head (&n->fiber);
          }
          gsched_fiber_queue_insert_tail (&n->fiber.queue, &input[i]);
        }
      }
    }
    count += idx;
  }
  while (idx == bl_arr_elems (input));
  if (count != 0) {
    *now = *in_bstream_timept32 (input[idx - 1]);
  }
  return count;
}
/*----------------------------------------------------------------------------*/
static inline void cancel_currently_programmed_future_event (gsched* gs)
{
 bl_taskq_post_try_cancel_delayed(
    gs->global->tq, gs->vars.prog_id, gs->vars.prog_timept32
    );
  gs->vars.has_prog = false;
}
/*----------------------------------------------------------------------------*/
static void gsched_loop_from_timed_event(
  bl_err error,bl_taskq_id id, void* context
  );
/*----------------------------------------------------------------------------*/
static inline void gsched_try_schedule_to_nearest_timed_event (gsched* gs)
{
  gsched_timed_entry const* timed = gsched_timed_get_head (&gs->timed);
  gsched_timed_entry const* wake  = gsched_timed_get_head (&gs->future_wakes);
  bl_timept32                    lowest;

  switch ((bl_u_bitv (timed != nullptr, 0) | bl_u_bitv (wake != nullptr, 1))) {
  case 0:
    return;
  case 1:
    lowest = timed->time;
    break;
  case 2:
    lowest = wake->time;
    break;
  case 3:
    lowest = bl_timept32_min (timed->time, wake->time);
    break;
  default:
    return;
  }
  if (gs->vars.has_prog) {
    if (bl_timept32_get_diff (gs->vars.prog_timept32, lowest) > 0) {
      cancel_currently_programmed_future_event (gs);
    }
    else {
      return;
    }
  }
  gs->vars.prog_timept32 = lowest;
  bl_assert_side_effect(
   bl_taskq_post_delayed_abs(
      gs->global->tq,
      &gs->vars.prog_id,
      gs->vars.prog_timept32,
     bl_taskq_task_rv (gsched_loop_from_timed_event, gs)
      ).bl == bl_ok
    );
  gs->vars.has_prog = true;
}
/*----------------------------------------------------------------------------*/
static inline void gsched_process_blocked_on_queue (gsched* gs)
{
  for (gsched_fibers_node* next = bl_tailq_first (&gs->sq[q_queue]); next; ) {
    gsched_fibers_node* fn = next; /*self removal from the run_q is allowed*/
    next                   = bl_tailq_next (next, hook);

    bool  ready = false;
    bl_uword mode  = (bl_uword) (fn->fiber.state.params.qread.match != nullptr) +
                  (bl_uword) (fn->fiber.state.params.qread.mask != nullptr);
    switch (mode) {
    case 0:
      ready = true; /*this function is only called when there are new msgs*/
      break;
    case 1:
      ready = gsched_fiber_try_peek_input_head_match(
        fn,
        bl_memr16_rv(
          (void*) fn->fiber.state.params.qread.match,
          fn->fiber.state.params.qread.match_size
          )
        );
      break;
    case 2:
      ready = gsched_fiber_try_peek_input_head_match_mask(
        fn,
        bl_memr16_rv(
          (void*) fn->fiber.state.params.qread.match,
          fn->fiber.state.params.qread.match_size
          ),
        bl_memr16_rv(
          (void*) fn->fiber.state.params.qread.mask,
          fn->fiber.state.params.qread.mask_size
          )
        );
      break;
    default:
      bl_assert (false);
      break;
    }
    if (ready) {
      node_queue_transfer_tail (&gs->sq[q_run], &gs->sq[q_queue], fn);
      fn->fiber.state.time = gs->vars.now;
    }
  }
}
/*----------------------------------------------------------------------------*/
static void gsched_loop (gsched* gs,bl_taskq_id id, bool from_timed_event)
{
  if (bl_tailq_empty (&gs->sq[q_run]) &&
      bl_tailq_empty (&gs->sq[q_blocked]) &&
      bl_tailq_empty (&gs->sq[q_queue])
    ) {
    return;
  }
  bl_timept32 now;
  bl_uword  new_input_count = gsched_consume_inputs (gs, &now);
  bl_uword  expired_count   = 0;
  if (new_input_count == 0 || from_timed_event) {
    gs->vars.now = bl_timept32_get();
    if (from_timed_event) {
      gs->vars.has_prog = (id == gs->vars.prog_id) ? false : gs->vars.has_prog;
    }
  }
  else {
    gs->vars.now = bl_timept32_max (gs->vars.now, now);
  }
  /*scan for scheduled deferred wake operations*/
  while (true) {
    gsched_timed_entry const* future =
      gsched_timed_get_head_if_expired (&gs->future_wakes, true, gs->vars.now);
    if (!future) {
      break;
    }
    run_wake(
      gs, future->value.wake.id, future->value.wake.count, gs->vars.now
      );
    gsched_timed_drop_head (&gs->future_wakes);
    ++expired_count;
  }
  /*move expired tasks in the timed queue to the run queue*/
  while (true) {
    gsched_timed_entry const* timed =
      gsched_timed_get_head_if_expired (&gs->timed, true, gs->vars.now);
    if (!timed) {
      break;
    }
    bl_uword id = (timed->value.fn->fiber.state.id == fstate_onqueue) ?
      q_queue : q_blocked;
    timed->value.fn->fiber.state.id = fstate_timer_reschedule;
    node_queue_transfer_tail (&gs->sq[q_run], &gs->sq[id], timed->value.fn);
    timed->value.fn->fiber.state.time = gs->vars.now;
    gsched_timed_drop_head (&gs->timed);
    ++expired_count;
  }

  if (expired_count && gs->vars.has_prog) {
    cancel_currently_programmed_future_event (gs);
  }
  if (new_input_count) {
    gsched_process_blocked_on_queue (gs);
  }
  /*process run queue*/
  for (gsched_fibers_node* next = bl_tailq_first (&gs->sq[q_run]); next; ) {
    gsched_fibers_node* n = next; /*self removal from the run_q is allowed*/
    next                  = bl_tailq_next (next, hook);
    bl_assert (bl_timept32_get_diff (gs->vars.now, n->fiber.state.time) >= 0);
    n->fiber.state.time   = gs->vars.now;
    coro_transfer (&gs->global->main_coro_ctx, &n->fiber.coro_ctx);
  }
  /*immediate request another run if there are still tasks in the run queue*/
  if (!bl_tailq_empty (&gs->sq[q_run])) {
    goto reschedule;
  }
  /*signal input producers that this task group needs scheduling after putting
    new input on the queue -> group can't make forward progress*/
  gs->vars.unhandled_bstream = ssc_in_q_try_consume (&gs->queue);
  if (gs->vars.unhandled_bstream) {
    goto reschedule; /*new data, group may make immediate forward progress*/
  }
  ssc_in_q_sig prev_sig;
  bl_err err = ssc_in_q_try_switch_to_idle (&gs->queue, &prev_sig);
  if (err.bl == bl_preconditions && prev_sig == in_q_ok) {
    /* new data, group may make immediate forward progress */
     goto reschedule;
  }
  else {
    /* no immediate forward progress is possible*/
    bl_assert(
      (!err.bl) ||
      (err.bl == bl_preconditions && (prev_sig == in_q_sig_idle)) ||
      (err.bl == bl_preconditions && (prev_sig == in_q_blocked))
      );
    gsched_try_schedule_to_nearest_timed_event (gs);
    return;
  }
reschedule:
  bl_assert_side_effect (gsched_program_schedule (gs).bl == bl_ok);
}
/*----------------------------------------------------------------------------*/
static void gsched_loop_from_timed_event(
  bl_err error,bl_taskq_id id, void* context
  )
{
  if (bl_unlikely (error.bl)) {
    return;
  }
  gsched_loop ((gsched*) context, id, true);
}
/*----------------------------------------------------------------------------*/
static void gsched_loop_regular(
  bl_err error,bl_taskq_id id, void* context
  )
{
  if (bl_unlikely (error.bl)) {
    return;
  }
  gsched_loop ((gsched*) context, id, false);
}
/*----------------------------------------------------------------------------*/
bl_err gsched_program_schedule_priv (gsched* gs,bl_taskq_task_func task)
{
 bl_taskq_id id;
  return bl_taskq_post(gs->global->tq, &id,bl_taskq_task_rv (task, gs));
}
/*----------------------------------------------------------------------------*/
bl_err gsched_program_schedule (gsched* gs)
{
  return gsched_program_schedule_priv (gs, gsched_loop_regular);
}
/*----------------------------------------------------------------------------*/
