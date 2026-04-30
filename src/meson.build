sources = files(
    'main.c',
    'server/server.c',
    'server/auth.c',
    'server/pty.c',
    'server/spawn.c',
    'client/client.c'
)

executable('sshlike',
    sources,
    include_directories: inc,
    c_args: ['-fvisibility=hidden'],
    install: true
)
