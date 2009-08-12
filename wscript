

VERSION='0.1'
APPNAME='soliloque-server'
srcdir = '.'
blddir = 'output'
SOURCES='main_serv.c server.c channel.c player.c array.c connection_packet.c crc.c packet_tools.c acknowledge_packet.c control_packet.c toolbox.c audio_packet.c ban.c server_stat.c configuration.c database.c registration.c server_privileges.c player_stat.c log.c'

flags_dbg1= ['-Wall', '-Werror']
flags_dbg2= ['-W', '-Wno-unused-parameter', '-Wstrict-prototypes', '-Wmissing-prototypes', '-Wpointer-arith'].append(flags_dbg1)
flags_dbg3= ['-Wreturn-type', '-Wcast-qual', '-Wswitch', '-Wshadow', '-Wcast-align'].append(flags_dbg2)

def set_options(opt):
  opt.add_option('--with-openssl', type='string', help='Define the location of openssl libraries.', dest='openssl')
  pass

def configure(conf):
  import Options
  conf.check_tool('gcc')
  # default environment
  conf.setenv('default')
  conf.check_cfg(atleast_pkgconfig_version='0.0.0')
  conf.check_cfg(package='libconfig', args='--cflags --libs', uselib_store='LIBCONFIG', mandatory=True)
  conf.check_cc(lib='dbi', uselib_store='LIBDBI', mandatory=True)
  conf.check_cc(lib='pthread', uselib_store='PTHREAD', mandatory=True)
  # Check for OpenSSL library and support for SHA256
  if (Options.options.openssl):
    conf.check_cc(lib='crypto', cppflags='-I'+Options.options.openssl+'/include',
		    ldflags='-L'+Options.options.openssl+'/lib', uselib_store='OPENSSL', mandatory=True)
  else:
    conf.check_cc(lib='crypto', uselib_store='OPENSSL', mandatory=True)
  conf.check(define_name='HAVE_SHA256', uselib='OPENSSL', function_name='SHA256', header_name='openssl/sha.h', mandatory=True)
  # Check for strndup (not present on OSX)
  conf.check(cflags='-D_GNU_SOURCE', define_name='HAVE_STRNDUP', function_name='strndup', header_name='string.h', errmsg='internal')
  conf.write_config_header('config.h')

def build(bld):
  sol_serv = bld.new_task_gen()
  sol_serv.features = "cc cprogram"
  sol_serv.source = SOURCES
  sol_serv.target = APPNAME
  sol_serv.includes = '.'
  sol_serv.install_path = '${PREFIX}/bin'
  sol_serv.defines = ['_GNU_SOURCE', '_BSD_SOURCE']
  sol_serv.uselib = 'LIBCONFIG PTHREAD LIBDBI OPENSSL'
  sol_serv.ccflags = ['-O2', '-ggdb', '-ansi'].append(flags_dbg2)
