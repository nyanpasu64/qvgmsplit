project('qvgmsplit', 'cpp', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'],
)

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Widgets'])

cmake = import('cmake')

cmake_opt = cmake.subproject_options()
# This doesn't actually set CMAKE_CXX_STANDARD, making it useless.
cmake_opt.set_override_option('cpp_std', 'c++20')

fmt = dependency('fmt', fallback : ['fmt', 'fmt_dep'])

# Arch installs both the GNU Scientific Library and Guidelines Support Library to
# /usr/include. So if GNU Scientific Library is installed, dependency('gsl') can succeed
# when /usr/include/gsl is present, but result in build errors since
# /usr/include/gsl/span is missing. microsoft-gsl and microsoft_gsl don't work, and both
# unconditionally fallback to the Internet.
gsl = dependency('microsoft-gsl', fallback : ['microsoft-gsl', 'microsoft_gsl_dep'])

# CMake subprojects wrongly think they're top-level projects. See
# https://github.com/mesonbuild/meson/issues/7102 for details.
stx_proj = cmake.subproject('STX', options: cmake_opt)
stx = stx_proj.dependency('stx')

libvgm_opt = cmake_opt
libvgm_opt.add_cmake_defines({
  'BUILD_TESTS': false,
  'BUILD_PLAYER': false,
  'BUILD_VGM2WAV': false,
  'USE_SANITIZERS': false,
})
libvgm_proj = cmake.subproject('libvgm', options: libvgm_opt)
vgm_audio = libvgm_proj.dependency('vgm-audio')
vgm_emu = libvgm_proj.dependency('vgm-emu')
vgm_player = libvgm_proj.dependency('vgm-player')
vgm_utils = libvgm_proj.dependency('vgm-utils')

inc = include_directories(
  'src'
)

moc_files = qt5.compile_moc(
  headers : files(
    'src/gui_app.h',
    'src/mainwindow.h',
    'src/options_dialog.h',
    'src/render_dialog.h',
  ),
  include_directories : inc,
  dependencies : qt5_dep,
)

executable('qvgmsplit',
  'src/backend.cpp',
  'src/gui_app.cpp',
  'src/lib/format.cpp',
  'src/main.cpp',
  'src/mainwindow.cpp',
  'src/options_dialog.cpp',
  'src/render_dialog.cpp',
  'src/settings.cpp',
  'src/vgm.cpp',
  'src/wave_writer.cpp',
  moc_files,
  dependencies : [
    qt5_dep,
    vgm_audio, vgm_emu, vgm_player, vgm_utils,
    fmt, gsl, stx,
  ],
  include_directories: inc,
  implicit_include_directories : true,
  install : true,
)
