# See https://github.com/anholt/libepoxy/blob/master/meson.build
# TODO: version, tests, valgrind, visibility, config generation

project(
    'ssc',
    'c',
    version : '0.0.1',
    license : 'BSD-3',
    default_options : [
        'c_std=gnu99',
        'cpp_std=gnu++11', # Just to be able to compile base_library C++ tests
#        'warning_level=1',
        'default_library=static'
        ]
    )

base_library = subproject ('base_library')

base_lib          = base_library.get_variable ('base_lib')
nonblock_lib      = base_library.get_variable ('nonblock_lib')
taskqueue_lib     = base_library.get_variable ('taskqueue_lib')
libcmocka_path    = base_library.get_variable ('libcmocka_path')
build_helpers_dir = base_library.get_variable ('build_helpers_dir')

include_dirs = [
    include_directories ('include'),
    include_directories ('src'),
    include_directories ('gitmodules/libcoro'),
]
include_dirs += base_library.get_variable ('include_dirs')

test_include_dirs  = include_dirs
test_include_dirs += [ include_directories ('test/src') ]
test_include_dirs +=  base_library.get_variable ('test_include_dirs')

pkg_mod = import ('pkgconfig')
install_subdir ('include/ssc', install_dir : 'include')

# "BL_USE_CLOCK_MONOTONIC_RAW" is needed, otherwise weird timing bugs happen.
cflags = base_library.get_variable ('cflags')
lib_cflags = []
test_cflags = []
test_link_args = [ libcmocka_path ]

if get_option ('sharedlib')
    libtype = 'shared_library'
    cflags += [ '-DSSC_SIM_SHAREDLIB', '-DSSC_SIM_SHAREDLIB_COMPILATION' ]
else
    cflags += [ '-DSSC_PRIVATE_SYMS', '-DSSC_SIM_PRIVATE_SYMS' ]
    libtype = 'static_library'
endif

cc = meson.get_compiler ('c')
if cc.get_id() == 'gcc' or cc.get_id() == 'clang'
    if get_option ('pic_statlibs') and libtype == 'static_library'
        lib_cflags += ['-fPIC']
    endif
endif

host_system = host_machine.system()
if host_system == 'windows'
    test_link_args += ['-lwinmm.lib'] # Untested
endif

threads = dependency ('threads')

#- base ------------------------------------------------------------------------

ssc_srcs = [
    'src/ssc/simulator/out_data_memory.c',
    'src/ssc/simulator/simulation.c',
    'src/ssc/simulator/out_queue.c',
    'src/ssc/simulator/cfg.c',
    'src/ssc/simulator/in_queue.c',
    'src/ssc/simulator/simulator.c',
    'src/ssc/simulator/group_scheduler.c',
    'gitmodules/libcoro/coro.c'
]
ssc_test_srcs = [
    'test/src/ssc/simulation_environment.c',
    'test/src/ssc/ahead_of_time_test.c',
    'test/src/ssc/two_fiber_test.c',
    'test/src/ssc/tests_main.c',
    'test/src/ssc/basic_test.c',
]
ssc_lib = build_target(
    'ssc',
    ssc_srcs,
    target_type         : libtype,
    include_directories : include_dirs,
    link_with           : [ base_lib, nonblock_lib, taskqueue_lib ],
    c_args              : cflags + lib_cflags,
    install             : true
    )
pkg_mod.generate(
    libraries   : ssc_lib,
    version     : '0.0.1',
    name        : 'libssc',
    filebase    : 'ssc',
    description : 'simulator task scheduler'
    )
test(
    'ssc',
    executable(
        'ssc-test',
        ssc_test_srcs,
        include_directories : test_include_dirs,
        link_with           : ssc_lib,
        link_args           : test_link_args,
        dependencies        : threads
    ))

executable(
    'ssc-example',
    [
        'example/src/ssc/ssc_example.c',
        'example/src/ssc/ssc_example_simulation.c',
    ],
    include_directories : include_dirs,
    link_with           : ssc_lib,
    c_args              : cflags + lib_cflags,
    link_args           : test_link_args,
    dependencies        : threads
)



