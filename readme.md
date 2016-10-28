Simulator Scheduler (ssc)
=========================

A co-operative real-time single threaded task scheduler suited as a
bare-bones core for i/o simulators.

Special credit
==============

This project is part of an unrealeased and unfinished project at my former
employer [Diadrom AB](http://diadrom.se/) which they kindly accepted
to release as open source with a BSD license.

Description
============

A co-operative real-time (with lookahead) single threaded task scheduler
suited as a bare-bones core for i/o simulators.

It was born at my former employer from the need to provide a simulated
environment for the communications of some high-level programs to avoid the
need for hardware and to give them a predictable environment. These programs
were interfacing with C libraries.

It is divided in two parts: the simulator (scheduler + queuing) and the
simulation.

The "user" is to write simulations by defining some C functions that are to be
run as concurrent processes, such processes listen and reply to messages.

A simple C API is provided for the simulation processes: message IO +
filtering, sleeping + yielding, time functions and semaphores.

From the simulator side there is mostly an API to send and receive messages
to/from the simulation and to execute the simulation loop from a thread.

The project is decoupled from the protocols it is simulating, it just sends
bytestreams without a given me meaning, it is the task of the application to
encode/decode a protocol and to parse them back and forth.

The messages sent from the simulator (from the outside) to the simulator are
broadcasted: each process/fiber on the fiber group receives it. So broadcasted,
point to point and master-slave protocols can be simulated just by filtering
messages.

There is an example program that static links the simulator and simulation in
the [example/src folder](https://github.com/RafaGago/ssc/tree/master/example/src/ssc).

Features
========

-Can be built as a static or shared library.

-Can load simulations as shared libraries or be statically linked with them.

-The simulation API is thought from scratch to be used from a script language.
[LUA bindings exist](https://github.com/RafaGago/ssc_lua).

-All the processes on the simulation run from the same thread, so there is no
 need for locking and the memory visibility is guaranteed.

-The send/receive functions of the simulator are thread-safe.

-The cooperative scheduler can do soft context switching, and hence each
 simulation process has its own stack. The size is configurable.

-The calls inside a fiber take virtually zero processing time, time just
 advances when the user calls "ssc_delay". Long blocking processes inside
 a fiber without calling "ssc_delay" are a bad idea.

-Consequence of the above, the fiber time has lookahead: the scheduler can
 keep simulating ahead of real-time if  "it detects" that the given process
 isn't blocked waiting for further messages. How far it goes is configurable.

Known quirks
==============

Select the simulation process/fiber stack size wisely. Otherwise they will
show as segfaults. This is done on the "ssc_add_fiber" function through the
cfg parameter.

The group input queue (simulator to simulation) is reference counted, so 
fibers that don't read/consume the input queue periodically block the 
input queue resource deallocation. A produce-only fiber should either mark
itself as produce-only through the "ssc_set_fiber_as_produce_only" call or
periodically call "ssc_drop_all_input".

Every fiber has its own time (which can be above real time), so if you are
modifying global data from many fibers time coherency is lost, one fiber
can see modifications done in "the future" from another fiber. The
lookahead feature can be disabled through the "ssc_set_fiber_as_real_time"
call.

Current status
==============

Very basic testing done.

Build (Linux)
=============

Be aware that cmocka requires CMake.

> git submodule update --init --recursive
>
> cd dependencies
>
> ./prepare_dependencies.sh
>
> cd ../build/premake
>
> premake5 gmake
>
> make -C ../linux config=release verbose=yes

Build (Windows)
===============

Hopefully with Windows bash the steps above will work. the premake5 command
should be run to generate Visual Studio solutions. (e.g.: premake5 vs2015).
