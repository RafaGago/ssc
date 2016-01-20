#ifndef __SSC_IN_BYTES_H__
#define __SSC_IN_BYTES_H__

#include <bl/base/platform.h>
#include <bl/base/integer.h>
#include <bl/base/time.h>
#include <bl/base/allocator.h>

/*----------------------------------------------------------------------------*/
enum in_bstream_header_e {
  /*max align (from malloc)*/
  in_bstream_tstamp_bytes       = sizeof (tstamp), 
  /*at least tstamp alignment (32 or 64)*/
  in_bstream_payload_size_bytes = sizeof (u16),
  /*at least u16 alignment*/ 
  in_bstream_refcount_bytes     = sizeof (u8), 

  in_bstream_tstamp_offset = 0,

  in_bstream_payload_size_offset = 
    in_bstream_tstamp_offset + in_bstream_tstamp_bytes,

  in_bstream_refcount_offset = 
    in_bstream_payload_size_offset + in_bstream_payload_size_bytes,

  in_bstream_payload_offset = 
    in_bstream_refcount_offset + in_bstream_refcount_bytes,

  in_bstream_overhead = in_bstream_payload_offset,
};
/*----------------------------------------------------------------------------*/
static inline tstamp* in_bstream_tstamp (u8* in_bstream)
{
  return (tstamp*) (in_bstream + in_bstream_tstamp_offset);
}
/*----------------------------------------------------------------------------*/
static inline u16* in_bstream_payload_size (u8* in_bstream)
{
  return (u16*) (in_bstream + in_bstream_payload_size_offset);
}
/*----------------------------------------------------------------------------*/
static inline u8* in_bstream_refcount (u8* in_bstream)
{
  return (u8*) (in_bstream + in_bstream_refcount_offset);
}
/*----------------------------------------------------------------------------*/
static inline uword in_bstream_total_size (uword payload)
{
  return payload + in_bstream_overhead;
}
/*----------------------------------------------------------------------------*/
static inline u8* in_bstream_payload (u8* in_bstream)
{
  return in_bstream + in_bstream_payload_offset;
}
/*----------------------------------------------------------------------------*/
static inline u8* in_bstream_from_payload (u8* in_bstream_payload)
{
  return in_bstream_payload - in_bstream_payload_offset;
}
/*----------------------------------------------------------------------------*/
static inline u8* in_bstream_alloc (uword size, alloc_tbl const* alloc)
{
  u8* ret;
  ret = bl_alloc (alloc, in_bstream_total_size (size));
  if (ret) {
    *in_bstream_tstamp (ret) = 0xdeadbeef;
  }
  return ret;
}
/*----------------------------------------------------------------------------*/
static inline bool in_bstream_pattern_validate (u8* in_bstream)
{
  return *in_bstream_tstamp (in_bstream) == 0xdeadbeef;
}
/*----------------------------------------------------------------------------*/
static inline void in_bstream_dealloc (u8* in_bstream, alloc_tbl const* alloc)
{
  bl_dealloc (alloc, in_bstream);
}
/*----------------------------------------------------------------------------*/


#endif /* __SSC_IN_BYTES_H__ */

