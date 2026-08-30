project(
    'battle_royale_sim',
    'cpp',
    version: '0.1.0',
    meson_version: '>=1.4.0',
    default_options: [
        'cpp_std=c++20',
        'buildtype=debugoptimized',
        'b_vscrt=mt',
    ],
)

python = find_program('python3', 'python', required: true)
cpp = meson.get_compiler('cpp')
godot_cpp_source_dir = meson.current_source_dir() / 'godot-cpp'
godot_cpp_generated_dir = 'godotcpp-generated'
godot_cpp_generated_root = meson.current_build_dir() / godot_cpp_generated_dir
godot_cpp_target = get_option('godotcpp-target')
godot_cpp_precision = get_option('godotcpp-precision')
godot_cpp_profile = get_option('godotcpp-build-profile')
godot_cpp_api_version = get_option('godotcpp-api-version')

if godot_cpp_api_version == '4.7'
    godot_cpp_api_filename = 'extension_api.json'
else
    godot_cpp_api_filename = 'extension_api-' + godot_cpp_api_version.replace(
        '.',
        '-',
    ) + '.json'
endif

godot_cpp_api_file = files('godot-cpp/gdextension/' + godot_cpp_api_filename)
godot_cpp_interface_file = files(
    'godot-cpp/gdextension/gdextension_interface.json',
)
godot_cpp_generator_file = files('godot-cpp/binding_generator.py')
godot_cpp_profile_script = files('godot-cpp/build_profile.py')
godot_cpp_profile_script_input = configure_file(
    input: godot_cpp_profile_script,
    output: 'godotcpp-build-profile.py',
    copy: true,
)
godot_cpp_profile_file = files(godot_cpp_profile)
godot_cpp_profile_input = configure_file(
    input: godot_cpp_profile_file,
    output: 'godotcpp-build-profile.json',
    copy: true,
)
godot_cpp_api_input = configure_file(
    input: godot_cpp_api_file,
    output: 'godotcpp-extension-api.json',
    copy: true,
)
godot_cpp_trimmed_api = meson.current_build_dir() / 'godotcpp-trimmed-api.json'
run_command(
    python,
    godot_cpp_profile_script_input,
    godot_cpp_profile_input,
    godot_cpp_api_input,
    godot_cpp_trimmed_api,
    check: true,
)

file_list_script = 'import sys; sys.path.insert(0, sys.argv[1]); from binding_generator import print_file_list; print_file_list(api_filepath=sys.argv[2], output_dir=sys.argv[3], headers=True, sources=True)'
godot_cpp_file_list_result = run_command(
    python,
    '-c',
    file_list_script,
    godot_cpp_source_dir,
    godot_cpp_trimmed_api,
    godot_cpp_generated_dir,
    check: true,
)
godot_cpp_generated_files = godot_cpp_file_list_result.stdout().strip().split(
    ';',
)
if godot_cpp_generated_files.length() == 0 or godot_cpp_generated_files[0] == ''
    error('godot-cpp binding file list generation returned no files')
endif
godot_cpp_generated_outputs = []
foreach generated_file : godot_cpp_generated_files
    relative_generated_file = generated_file.replace(
        godot_cpp_generated_dir + '/',
        '',
    )
    godot_cpp_generated_outputs += [
        'godotcpp_' + relative_generated_file.replace('/', '__'),
    ]
endforeach

if get_option('godotcpp-template-get-node')
    binding_bool = 'true'
else
    binding_bool = 'false'
endif
godot_cpp_bindings = custom_target(
    'godotcpp-bindings',
    input: [
        godot_cpp_profile_input,
        godot_cpp_api_input,
        godot_cpp_interface_file[0],
        godot_cpp_generator_file[0],
        godot_cpp_profile_script_input,
    ],
    output: godot_cpp_generated_outputs,
    command: [
        python,
        meson.current_source_dir() / '..' / 'scripts' / 'generate_godot_cpp_bindings.py',
        godot_cpp_source_dir,
        godot_cpp_trimmed_api,
        godot_cpp_interface_file[0].full_path(),
        binding_bool,
        '64',
        godot_cpp_precision,
        godot_cpp_generated_root,
        '@OUTDIR@',
    ],
    depend_files: [
        godot_cpp_generator_file[0],
        godot_cpp_profile_script[0],
        meson.current_source_dir() / '..' / 'scripts' / 'generate_godot_cpp_bindings.py',
    ],
    build_by_default: true,
)
godot_cpp_generated_headers = []
generated_index = 0
foreach generated_file : godot_cpp_generated_files
    if not generated_file.endswith('.cpp')
        godot_cpp_generated_headers += [godot_cpp_bindings[generated_index]]
    endif
    generated_index += 1
endforeach

godot_cpp_sources = files(
    'godot-cpp/src/classes/editor_plugin_registration.cpp',
    'godot-cpp/src/classes/low_level.cpp',
    'godot-cpp/src/classes/wrapped.cpp',
    'godot-cpp/src/core/class_db.cpp',
    'godot-cpp/src/core/error_macros.cpp',
    'godot-cpp/src/core/memory.cpp',
    'godot-cpp/src/core/method_bind.cpp',
    'godot-cpp/src/core/object.cpp',
    'godot-cpp/src/core/print_string.cpp',
    'godot-cpp/src/godot.cpp',
    'godot-cpp/src/variant/aabb.cpp',
    'godot-cpp/src/variant/basis.cpp',
    'godot-cpp/src/variant/callable_custom.cpp',
    'godot-cpp/src/variant/callable_method_pointer.cpp',
    'godot-cpp/src/variant/char_string.cpp',
    'godot-cpp/src/variant/color.cpp',
    'godot-cpp/src/variant/packed_arrays.cpp',
    'godot-cpp/src/variant/plane.cpp',
    'godot-cpp/src/variant/projection.cpp',
    'godot-cpp/src/variant/quaternion.cpp',
    'godot-cpp/src/variant/rect2.cpp',
    'godot-cpp/src/variant/rect2i.cpp',
    'godot-cpp/src/variant/transform2d.cpp',
    'godot-cpp/src/variant/transform3d.cpp',
    'godot-cpp/src/variant/variant.cpp',
    'godot-cpp/src/variant/variant_internal.cpp',
    'godot-cpp/src/variant/vector2.cpp',
    'godot-cpp/src/variant/vector2i.cpp',
    'godot-cpp/src/variant/vector3.cpp',
    'godot-cpp/src/variant/vector3i.cpp',
    'godot-cpp/src/variant/vector4.cpp',
    'godot-cpp/src/variant/vector4i.cpp',
)
godot_cpp_sources += godot_cpp_bindings

godot_cpp_public_args = ['-DGDEXTENSION', '-DTHREADS_ENABLED']
godot_cpp_public_args += ['-DDISABLE_DEPRECATED']
if godot_cpp_target != 'template_release'
    godot_cpp_public_args += ['-DDEBUG_ENABLED']
endif
if godot_cpp_precision == 'double'
    godot_cpp_public_args += ['-DREAL_T_IS_DOUBLE']
endif

godot_cpp_private_args = []
if cpp.get_argument_syntax() == 'msvc'
    generated_include_arg = '/I' + (godot_cpp_generated_root / 'gen/include')
    godot_cpp_public_args += [generated_include_arg]
    godot_cpp_private_args += [generated_include_arg, '/W4', '/utf-8']
    godot_cpp_private_args += ['/D_HAS_EXCEPTIONS=0']
else
    generated_include_arg = '-I' + (godot_cpp_generated_root / 'gen/include')
    godot_cpp_public_args += [generated_include_arg]
    godot_cpp_private_args += [generated_include_arg]
    if host_machine.system() != 'windows'
        godot_cpp_public_args += ['-DUNIX_ENABLED']
    endif
    godot_cpp_private_args += [
        '-Wall',
        '-Wextra',
        '-Wctor-dtor-privacy',
        '-Wno-unused-parameter',
        '-Wnon-virtual-dtor',
        '-Wwrite-strings',
        '-Wimplicit-fallthrough',
        '-Wno-ordered-compare-function-pointers',
        '-fvisibility=hidden',
    ]
    if cpp.has_argument('-fno-exceptions')
        godot_cpp_private_args += ['-fno-exceptions']
    endif
endif

if host_machine.system() == 'windows'
    godot_cpp_public_args += [
        '-DWINDOWS_ENABLED',
        '-DTYPED_METHOD_BIND',
        '-DNOMINMAX',
    ]
elif host_machine.system() == 'linux'
    godot_cpp_public_args += ['-DLINUX_ENABLED']
elif host_machine.system() == 'darwin'
    godot_cpp_public_args += ['-DMACOS_ENABLED']
endif
godot_cpp_private_args += godot_cpp_public_args

godot_cpp_include_dirs = include_directories(
    'godot-cpp/include',
)
godot_cpp = static_library(
    'godot-cpp',
    godot_cpp_sources,
    include_directories: godot_cpp_include_dirs,
    cpp_args: godot_cpp_private_args,
    pic: true,
    build_by_default: true,
)
godot_cpp_dependency = declare_dependency(
    link_with: godot_cpp,
    include_directories: godot_cpp_include_dirs,
    compile_args: godot_cpp_public_args,
    sources: godot_cpp_generated_headers,
)

sim_sources = files(
    'register_types.cpp',
    'sim_server.cpp',
    'sim/world.cpp',
    'sim/world_input.cpp',
    'sim/world_spawn.cpp',
    'sim/stats_config.cpp',
    'sim/nav_grid.cpp',
    'sim/snapshot_builder.cpp',
    'sim/snapshot_bindings.cpp',
    'sim/heroes/hero_registry.cpp',
    'sim/skills/skill_registry.cpp',
)
sim_include_dirs = include_directories('.', 'sim', 'third_party')

sim_link_args = []
if host_machine.system() == 'linux' and get_option('godotcpp-static-cpp')
    if cpp.has_link_argument('-static-libgcc')
        sim_link_args += ['-static-libgcc']
    endif
    if cpp.has_link_argument('-static-libstdc++')
        sim_link_args += ['-static-libstdc++']
    endif
elif host_machine.system() == 'windows' and cpp.get_argument_syntax() != 'msvc' and get_option(
    'godotcpp-static-cpp',
)
    foreach link_arg : ['-static', '-static-libgcc', '-static-libstdc++']
        if cpp.has_link_argument(link_arg)
            sim_link_args += [link_arg]
        endif
    endforeach
endif

addons_dir = meson.current_source_dir() / '..' / 'addons' / 'battle_royale_sim'
if host_machine.system() == 'windows'
    library_name_prefix = ''
else
    library_name_prefix = 'lib'
endif
shared_library(
    'battle_royale_sim',
    sim_sources,
    dependencies: godot_cpp_dependency,
    include_directories: sim_include_dirs,
    link_args: sim_link_args,
    name_prefix: library_name_prefix,
    install: true,
    install_dir: addons_dir,
    build_by_default: true,
)

sim_test_sources = files(
    'tests/sim_tests.cpp',
    'sim/stats_config.cpp',
    'sim/heroes/hero_registry.cpp',
    'sim/skills/skill_registry.cpp',
)
sim_tests = executable(
    'sim_tests',
    sim_test_sources,
    include_directories: sim_include_dirs,
    build_by_default: false,
)
test('sim_tests', sim_tests)
