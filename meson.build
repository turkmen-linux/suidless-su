project('suidless-su', 'c', version: '1.0')

if get_option('debug')
    add_project_arguments('-DDEBUG', language: 'c')
endif

inc = include_directories('include')

subdir('src')
subdir('data')
