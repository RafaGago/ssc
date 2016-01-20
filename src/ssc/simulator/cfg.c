#include <ssc/simulator/cfg.h>

/*----------------------------------------------------------------------------*/
void ssc_cfg_init (ssc_cfg* cfg)
{
  cfg->min_out_queue_size = 1024;
}
/*----------------------------------------------------------------------------*/
void ssc_fiber_group_cfg_init (ssc_fiber_group_cfg* cfg)
{
  cfg->min_queue_size         = 128;
  cfg->max_look_ahead_time_us = 15000;
}
/*----------------------------------------------------------------------------*/
