#ifndef __SSC_CFG_H__
#define __SSC_CFG_H__

#include <bl/base/platform.h>
#include <bl/base/integer.h>
#include <bl/base/dynarray.h>

#include <ssc/types.h>
/*----------------------------------------------------------------------------*/
typedef struct ssc_cfg {
  uword min_out_queue_size;
}
ssc_cfg;
/*----------------------------------------------------------------------------*/
typedef struct ssc_fiber_group_cfg {
  uword min_queue_size;
}
ssc_fiber_group_cfg;
/*----------------------------------------------------------------------------*/
extern void ssc_cfg_init (ssc_cfg* cfg);
/*----------------------------------------------------------------------------*/
extern void ssc_fiber_group_cfg_init (ssc_fiber_group_cfg* cfg);
/*----------------------------------------------------------------------------*/
define_dynarray_types (ssc_fiber_cfgs, ssc_fiber_cfg)
declare_dynarray_funcs (ssc_fiber_cfgs, ssc_fiber_cfg)

#endif /* __SSC_CFG_H__ */

