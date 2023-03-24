#  This file is part of DirectFB.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

project('DirectFB2-tslib', 'c',
        version: '2.0.0')

directfb_dep = dependency('directfb-internal')

moduledir = directfb_dep.get_variable(pkgconfig: 'moduledir')

tslib_dep = dependency('tslib')

pkgconfig = import('pkgconfig')

library('directfb_tslib',
        'tslib.c',
        c_args: '-DMAX_TSLIB_SLOTS=' + get_option('max-slots').to_string(),
        dependencies: [directfb_dep, tslib_dep],
        install: true,
        install_dir: join_paths(moduledir, 'inputdrivers'))

pkgconfig.generate(filebase: 'directfb-inputdriver-tslib',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-inputdriver-tslib',
                   description: 'tslib input driver',
                   requires_private: 'tslib',
                   libraries_private: ['-L${moduledir}/inputdrivers',
                                       '-Wl,--whole-archive -ldirectfb_tslib -Wl,--no-whole-archive'])

if get_option('default_library') != 'shared'
  meson.add_install_script('dfb-update-pkgconfig', skip_if_destdir: true)
endif
