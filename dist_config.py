import os

package = 'libt3window'
excludesrc = '/(Makefile|TODO.*|SciTE.*|run\.sh|test\.c)$'
auxsources= [ 'src/window_api.h', 'src/window_errors.h', 'src/window_shared.c' ]
extrabuilddirs = [ 'doc' ]
auxfiles = [ 'doc/API' ]

versioninfo = '0:0:0'


def get_replacements(mkdist):
	return [
		{
			'tag': '<VERSION>',
			'replacement': mkdist.version
		},
		{
			'tag': '^#define T3_WINDOW_VERSION .*',
			'replacement': '#define T3_WINDOW_VERSION ' + mkdist.get_version_bin(),
			'files': [ 'src/terminal.h' ],
			'regex': True
		},
		{
			'tag': '<OBJECTS>',
			'replacement': " ".join(mkdist.sources_to_objects(mkdist.sources, '\.c$', '.lo')),
			'files': [ 'Makefile.in' ]
		},
		{
			'tag': '<VERSIONINFO>',
			'replacement': versioninfo,
			'files': [ 'Makefile.in' ]
		},
		{
			'tag': '<LIBVERSION>',
			'replacement': versioninfo.split(':', 2)[0],
			'files': [ 'Makefile.in' ]
		}
	]

def finalize(mkdist):
	os.symlink('.', os.path.join(mkdist.topdir, 'src', 't3window'))
