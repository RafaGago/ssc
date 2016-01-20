/* 
  THIS FILE CONTAINS DEFINITIONS (NOT DECLARATIONS) OF LIBRARY CODE TO BE 
  INCLUDED BY EACH SIMULATION IN JUST ONE TRANSLATION UNIT (AKA .c/.cpp file)

  This file defines some helper functions that are simple compositions or
  frequent usage patterns of the main api functions.

  It also defines a the function table for the simulator if the simulation is
  compiled as a shared library.

  The header and the functions are small and simple enough that in my judgement
  they didn't justify the hassle of distributing them as a static lib.
*/

#include <ssc/simulation/simulator_func_table.h>

#ifdef SSC_SHAREDLIB
ssc_simulator_ftable ssc_sim_tbl;
/*----------------------------------------------------------------------------*/
void SSC_EXPORT ssc_manual_link_to_simulator(
  ssc_simulator_ftable const* t
  )
{
  bl_assert_always (t);
  bl_assert_always (t->add_fiber);
  bl_assert_always (t->yield);
  bl_assert_always (t->wake);
  bl_assert_always (t->wait);
  bl_assert_always (t->peek_input_head);
  bl_assert_always (t->timed_peek_input_head);
  bl_assert_always (t->try_peek_input_head);
  bl_assert_always (t->drop_input_head);
  bl_assert_always (t->delay);
  bl_assert_always (t->get_timestamp);
  bl_assert_always (t->produce_static_output);
  bl_assert_always (t->produce_dynamic_output);
  bl_assert_always (t->produce_error);
  bl_assert_always (t->produce_static_string);
  bl_assert_always (t->produce_dynamic_string);
  bl_assert_always (t->consume_input_head_match_mask);
  bl_assert_always (t->timed_consume_input_head_match_mask);
  ssc_sim_tbl = *t;
}
#endif /*SSC_SHAREDLIB*/
