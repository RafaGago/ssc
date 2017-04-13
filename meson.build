# See https://github.com/anholt/libepoxy/blob/master/meson.build
# TODO: version, tests, valgrind, visibility, config generation

project(
    'ssc',
    'c',
    version : '0.0.1',
    license : 'BSD-3',
    default_options : [
        'c_std=gnu99',
#        'warning_level=1',
        'default_library=static'
        ]
    )

run_command(
    'git', 'submodule', 'update', '--init', '--recursive'
    )

base_library  = subproject('base_library')

base_lib      = base_library.get_variable ('base_lib')
nonblock_lib  = base_library.get_variable ('nonblock_lib')
taskqueue_lib = base_library.get_variable ('taskqueue_lib')

base_library_includedirs = base_library.get_variable ('includedirs')
base_library_testincludedirs = base_library.get_variable ('testincludedirs')

cmocka_lib = base_library.get_variable ('cmockalib_abspath')

includedirs = [
    include_directories ('include'),
    include_directories ('src'),
    include_directories ('gitmodules/libcoro'),
]
includedirs += base_library_includedirs

testincludedirs = includedirs
testincludedirs += include_directories ('test/src')
testincludedirs += base_library_testincludedirs

pkg_mod = import ('pkgconfig')
install_subdir ('include/ssc', install_dir : 'include')

cflags = []
lib_cflags = []
test_cflags = []
test_link_args = [ cmocka_lib ]

if get_option ('sharedlib')
    libtype = 'shared_library'
else
    libtype = 'static_library'
endif

cc = meson.get_compiler ('c')
if cc.get_id() == 'gcc' or cc.get_id() == 'clang'
    cflags += ['-fvisibility=hidden', '-fno-stack-protector']
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
    include_directories : includedirs,
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
        include_directories : testincludedirs,
        link_with           : ssc_lib,
        link_args           : test_link_args,
        dependencies        : threads
    ))
