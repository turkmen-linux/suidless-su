sources = files(
    'main.c',
    'server/server.c',
    'server/pty.c',
    'server/spawn.c',
    'server/auth.c',
    'client/client.c',
    'client/su.c',
    'client/sudo.c'
)
deps = []
if get_option('pam')
    sources += ['server/pam.c']
    deps += [dependency('pam', required:true)]
    add_project_arguments('-DPAM', language : 'c')
endif

executable('suidless-su',
    sources,
    dependencies: deps,
    include_directories: inc,
    c_args: [
        '-fvisibility=hidden',
        '-D_POSIX_C_SOURCE=200809L',
        '-D_XOPEN_SOURCE=700',
        '-D_GNU_SOURCE',
        '-Wall',
        '-Wextra',
        '-Werror'
    ],
    link_args: ['-lcrypt'],
    install_dir: get_option('prefix') / get_option('bindir'),
    install: true
)
foreach tool : ['sudo', 'su']
    install_symlink(
        tool, 
        install_dir: get_option('prefix') / get_option('bindir'),
        pointing_to: 'suidless-su'
    )
    run_command(
        ['ln', '-s', './src/suidless-su', meson.current_build_dir() + '/../'+tool],
        check: false
    )
endforeach
