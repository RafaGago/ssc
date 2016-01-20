#ifndef __SSC_TYPES_H__
#define __SSC_TYPES_H__

#include <bl/base/error.h>
#include <bl/base/assert.h>
#include <bl/base/platform.h>
#include <bl/base/integer.h>
#include <bl/base/time.h>
#include <bl/base/memory_range.h>
/*----------------------------------------------------------------------------*/
/* GENERAL */
/*----------------------------------------------------------------------------*/
typedef uword_d4 ssc_group_id;
enum { ssc_null_id = utype_max (ssc_group_id) };
/*----------------------------------------------------------------------------*/
/* SIMULATOR */
/*----------------------------------------------------------------------------*/
enum ssc_out_type_e {
  ssc_type_bytes           = 1,
  ssc_type_string          = 2,
  ssc_type_error           = 3,
  ssc_type_is_dynamic_mask = 4,

  ssc_type_static_bytes   = ssc_type_bytes,
  ssc_type_dynamic_bytes  = ssc_type_bytes | ssc_type_is_dynamic_mask,
  ssc_type_static_string  = ssc_type_string,
  ssc_type_dynamic_string = ssc_type_string | ssc_type_is_dynamic_mask,
};
typedef u8 ssc_out_type;
/*----------------------------------------------------------------------------*/
typedef struct ssc_output_data {
  memr16       data;
  tstamp       time;
  ssc_group_id gid;
  ssc_out_type type;
}
ssc_output_data;
/*----------------------------------------------------------------------------*/
static inline bool ssc_output_is_dynamic (ssc_output_data const* d)
{
  return (d->type & ssc_type_is_dynamic_mask) != 0;
}
/*----------------------------------------------------------------------------*/
static inline bool ssc_output_is_bytes (ssc_output_data const* d)
{
  return (d->type & ~ssc_type_is_dynamic_mask) == ssc_type_bytes;
}
/*----------------------------------------------------------------------------*/
static inline bool ssc_output_is_string (ssc_output_data const* d)
{
  return (d->type & ~ssc_type_is_dynamic_mask) == ssc_type_string;
}
/*----------------------------------------------------------------------------*/
static inline bool ssc_output_is_error (ssc_output_data const* d)
{
  return (d->type & ~ssc_type_is_dynamic_mask) == ssc_type_error;
}
/*----------------------------------------------------------------------------*/
static inline void ssc_output_read_as_error(
  ssc_output_data const* d, bl_err* err, char const** str
  )
{
  *err = (bl_err) memr16_size (d->data);
  *str = memr16_beg_as (d->data, char const);
}
/*----------------------------------------------------------------------------*/
static inline char const* ssc_output_read_as_string(
  ssc_output_data const* d, u16* strlength
  )
{
  bl_assert (memr16_size (d->data));
  *strlength = memr16_size (d->data) - 1;
  return memr16_beg_as (d->data, char const);
}
/*----------------------------------------------------------------------------*/
static inline memr16 ssc_output_read_as_bytes (ssc_output_data const* d)
{
  return d->data;
}
/*----------------------------------------------------------------------------*/
/* SIMULATION */
/*----------------------------------------------------------------------------*/
typedef void* ssc_handle;
/*----------------------------------------------------------------------------*/
typedef bl_err (*ssc_fiber_setup_func)(
  void* fiber_context, void* sim_context
  );
typedef void (*ssc_fiber_teardown_func)(
  void* fiber_context, void* sim_context
  );
typedef void (*ssc_fiber_func)(
  ssc_handle ssc, void* fiber_context, void* sim_context
  );

typedef struct ssc_fiber_cfg {
  ssc_group_id            id;
  ssc_fiber_func          fiber;
  ssc_fiber_setup_func    setup;
  ssc_fiber_teardown_func teardown;
  void*                   fiber_context;
  uword                   min_stack_size; /*bytes*/
  uword                   min_queue_size; /*messages*/
  uword                   max_func_count; /*max recursion on a slice*/
}
ssc_fiber_cfg;
/*----------------------------------------------------------------------------*/
static inline ssc_fiber_cfg ssc_fiber_cfg_rv(
  ssc_group_id            id,
  ssc_fiber_func          fiber,
  ssc_fiber_setup_func    setup,
  ssc_fiber_teardown_func teardown,
  void*                   fiber_context
  )
{
  ssc_fiber_cfg ret;
  ret.id             = id;
  ret.fiber          = fiber;
  ret.setup          = setup;
  ret.teardown       = teardown;
  ret.fiber_context  = fiber_context;
  ret.min_stack_size = 8 * 1024;
  ret.min_queue_size = 128;
  ret.max_func_count = 50;
  return ret;
}
/*----------------------------------------------------------------------------*/

#endif /* __SSC_SIMULATION_TYPES_H__ */

