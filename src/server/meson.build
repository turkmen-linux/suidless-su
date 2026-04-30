server_sources = files(
    'main.c',
    'server.c',
    'auth.c',
    'pty.c'
)

executable('sshlike-server',
    server_sources,
    include_directories: inc,
    install: true
)
