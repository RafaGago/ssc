#ifndef __SSC_SIMULATOR_H__
#define __SSC_SIMULATOR_H__

#include <ssc/simulator/libexport.h>
#include <ssc/types.h>

#include <bl/base/platform.h>
#include <bl/base/integer.h>
#include <bl/base/error.h>

typedef struct ssc ssc;
/*==============================================================================
 Startup / Shutdown
==============================================================================*/
/*------------------------------------------------------------------------------
ssc_create: Creates a new simulator instance-

    "simlib_path" can be a nullptr if simulator and simulation are compiled
    together.

    "simlib_passed_data" is a free pointer which will be received on the simu-
    lation initialization functions.
------------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_create(
    ssc**       instance_out,
    char const* simlib_path,
    void*       simlib_passed_data
    );
/*------------------------------------------------------------------------------
  ssc_destroy: Destroys a simulator instance. You may need to call
    "ssc_run_teardown(...)"  before.
------------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_destroy (ssc* sim);
/*==============================================================================
  Simulator thread functions. To be run on a worker thread.
==============================================================================*/
/*------------------------------------------------------------------------------
  ssc_run_setup: Runs the simulation setup.
------------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_run_setup (ssc* sim);
/*------------------------------------------------------------------------------
  ssc_run_setup: Runs the simulation cleanup.
------------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_run_teardown (ssc* sim);
/*------------------------------------------------------------------------------
  ssc_run_some: Executes some iterations of the simulation or blocks until the
  timeout if there is nothing to do.
------------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_run_some (ssc* sim, u32 usec_timeout);
/*------------------------------------------------------------------------------
  ssc_run_some: Executes some iterations of the simulation. It returns
  immediately if there is nothing to do.
------------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_try_run_some (ssc* sim);
/*==============================================================================
 Data exchange funcs (Thread safe)
 =============================================================================*/
/* ssc_block: Blocks any new input to the ssc instance, any call to write will
    return "bl_locked" */
/*----------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  void ssc_block (ssc* sim);
/*----------------------------------------------------------------------------*/
/* ssc_alloc_write_bytestream: allocates memory to use with "ssc_send". Don't
   assume any alignment. */
/*----------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  u8* ssc_alloc_write_bytestream (ssc* sim, uword capacity);
/*----------------------------------------------------------------------------*/
/* ssc_write: Sends a message to a fiber group.

  "bytestream" is to be allocated by calling the "ssc_alloc_write_bytestream"
  function before (which will embbed bookeeping data). Don't use it with raw
  chunks.

  "bytestream"'s memory can be considered freed after returning from this func-
  tion (even with an error code). Don't access the pointer after this call. */
/*----------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_write (ssc* sim, ssc_group_id g, u8* bytestream, u16 size);
/*----------------------------------------------------------------------------*/
/* ssc_read: Reads messages from the simulation.

  "d" is an array of structs ssc_output_data.

  "d_capacity" is the size of the array d.

  "d_consumed" contains the number of "ssc_output_data" structs retrieved
    on "d" when the function returns.

  Each of the messages needs to be deallocated by "ssc_dealloc_read_data(...)"*/
/*----------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_read(
    ssc*             sim,
    uword*           d_consumed,
    ssc_output_data* d,
    uword            d_capacity,
    u32              timeout_us
    );
/*----------------------------------------------------------------------------*/
/* ssc_dealloc_read_data: Deallocates __one__ message retrieved by ssc_read.

   If you retrieved a bulk of them in one ssc_read call you need to deallocate
   them individually anyways.*/
/*----------------------------------------------------------------------------*/
extern SSC_SIM_EXPORT
  bl_err ssc_dealloc_read_data (ssc* sim, ssc_output_data* read_data);
/*----------------------------------------------------------------------------*/
#endif /* __SSC_SIMULATION_H__ */

