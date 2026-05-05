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
    install_dir: '/etc/init.d'	
  )
endif