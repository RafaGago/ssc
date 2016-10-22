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

It is divided on two parts the simulator (scheduler + queuing) and the
simulation.

The "user" is to write simulations by defining some C functions that are to be
run as concurrent processes, such processes listen and may act to messages
broadcasted to the simulator.

A simple C API is provided for the simulation processes to listen for messages,
write messages back to the simulator and for sleeping (e.g. to simulate a long
running process. This API communicates the running processes with the task
scheduler (simulator).

From the simulator side there is mostly an API to send and receive messages
to/from the simulation.

Those messages are byte streams without a given meaning, it is the task
of the application that uses the library and the simulation code to give meaning
and to parse them back and forth.

There is an example program that static links the simulator and simulation in
the example/src folder.

Features
========

-Can be built as a static or shared library.
-Can load simulations as shared libraries or be statically linked with them.
-The simulation API is thought from scratch to be used from a script language.
 LUA bindings exist.
-All the processes on the simulation run from the same thread, so there is no
 need for locking and the memory visibility is guaranteed.
-The send/receive functions of the simulator are thread-safe.
-The cooperative scheduler can do soft context switching, and hence each
 simulation process has its own stack.
-It has lookahead, the scheduler can keep simulating if it detects that the
 given process isn't blocked waiting for further messages.

Known quirks
==============

Select the simulation process/fiber stack size wisely. Otherwise they will
show as segfaults. This is done on the "ssc_add_fiber" function through the
cfg parameter.

Current status
==============

Very basic testing done.

Build (Linux)
=============

Be aware that cmocka used on base_library requires CMake.

git submodule init && git submodule update
cd dependencies
./prepare_dependencies.sh
cd ../build/premake
premake5 gmake
make -C ../linux config=release verbose=yes

Build (Windows)
===============

Hopefully with Windows bash the steps above will work. the premake5 command
should be run to generate Visual Studio solutions. (e.g.: premake5 vs2015).
