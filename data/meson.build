cdata = configuration_data({
	'prefix' : get_option('prefix'),
	'bindir' : get_option('bindir'),
})

if get_option('service') == 'openrc'
  configure_file(
    input : 'su.openrc.in',
    output : 'su',
    configuration : cdata,
    install: true,
    install_dir: '/etc/init.d',
    install_mode: 'rwxr-xr-x'
  )
endif

if get_option('service') == 'sysvinit'
  configure_file(
    input : 'su.sysvinit.in',
    output : 'su',
    configuration : cdata,
    install: true,
    install_dir: '/etc/init.d',
    install_mode: 'rwxr-xr-x'
  )
endif


if get_option('pam')
    install_data(
        'suidless-su.pam',
        install_dir: get_option('pam-config-dir'),
        install_mode: 'rw-r--r--',
        rename: 'suidless-su'
    )
endif
