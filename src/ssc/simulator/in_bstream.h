#ifndef __SSC_IN_BYTES_H__
#define __SSC_IN_BYTES_H__

#include <bl/base/platform.h>
#include <bl/base/integer.h>
#include <bl/base/time.h>
#include <bl/base/allocator.h>

/*----------------------------------------------------------------------------*/
enum in_bstream_header_e {
  /*max align (from malloc)*/
  in_bstream_timept32_bytes       = sizeof (bl_timept32),
  /*at least bl_timept32 alignment (32 or 64)*/
  in_bstream_payload_size_bytes = sizeof (bl_u16),
  /*at least bl_u16 alignment*/
  in_bstream_refcount_bytes     = sizeof (bl_u8),

  in_bstream_timept32_offset = 0,

  in_bstream_payload_size_offset =
    in_bstream_timept32_offset + in_bstream_timept32_bytes,

  in_bstream_refcount_offset =
    in_bstream_payload_size_offset + in_bstream_payload_size_bytes,

  in_bstream_payload_offset =
    in_bstream_refcount_offset + in_bstream_refcount_bytes,

  in_bstream_overhead = in_bstream_payload_offset,
};
/*----------------------------------------------------------------------------*/
static inline bl_timept32* in_bstream_timept32 (bl_u8* in_bstream)
{
  return (bl_timept32*) (in_bstream + in_bstream_timept32_offset);
}
/*----------------------------------------------------------------------------*/
static inline bl_u16* in_bstream_payload_size (bl_u8* in_bstream)
{
  return (bl_u16*) (in_bstream + in_bstream_payload_size_offset);
}
/*----------------------------------------------------------------------------*/
static inline bl_u8* in_bstream_refcount (bl_u8* in_bstream)
{
  return (bl_u8*) (in_bstream + in_bstream_refcount_offset);
}
/*----------------------------------------------------------------------------*/
static inline bl_uword in_bstream_total_size (bl_uword payload)
{
  return payload + in_bstream_overhead;
}
/*----------------------------------------------------------------------------*/
static inline bl_u8* in_bstream_payload (bl_u8* in_bstream)
{
  return in_bstream + in_bstream_payload_offset;
}
/*----------------------------------------------------------------------------*/
static inline bl_u8* in_bstream_from_payload (bl_u8* in_bstream_payload)
{
  return in_bstream_payload - in_bstream_payload_offset;
}
/*----------------------------------------------------------------------------*/
static inline bl_u8* in_bstream_alloc (bl_uword size, bl_alloc_tbl const* alloc)
{
  bl_u8* ret;
  ret = bl_alloc (alloc, in_bstream_total_size (size));
  if (ret) {
    *in_bstream_timept32 (ret) = 0xdeadbeef;
  }
  return ret;
}
/*----------------------------------------------------------------------------*/
static inline bool in_bstream_pattern_validate (bl_u8* in_bstream)
{
  return *in_bstream_timept32 (in_bstream) == 0xdeadbeef;
}
/*----------------------------------------------------------------------------*/
static inline void in_bstream_dealloc(
  bl_u8* in_bstream, bl_alloc_tbl const* alloc
  )
{
  bl_dealloc (alloc, in_bstream);
}
/*----------------------------------------------------------------------------*/


#endif /* __SSC_IN_BYTES_H__ */

