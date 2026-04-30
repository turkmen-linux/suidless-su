sources = files(
    'main.c',
    'server/server.c',
    'server/auth.c',
    'server/pty.c',
    'client/client.c'
)

executable('sshlike',
    sources,
    include_directories: inc,
    install: true
)
