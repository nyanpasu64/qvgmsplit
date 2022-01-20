project('qvgmsplit', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'],
)

cmake = import('cmake')

cmake_opt = cmake.subproject_options()
# This doesn't actually set CMAKE_CXX_STANDARD, making it useless.
cmake_opt.set_override_option('cpp_std', 'c++20')

fmt = dependency('fmt', fallback : ['fmt', 'fmt_dep'])
gsl = dependency('gsl', fallback : ['microsoft-gsl', 'microsoft_gsl_dep'])

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
vgm_player = libvgm_proj.dependency('vgm-player')
vgm_utils = libvgm_proj.dependency('vgm-utils')

executable('qvgmsplit',
  'src/backend.cpp',
  'src/backend.h',
  'src/gui_app.cpp',
  'src/gui_app.h',
  'src/lib/box_array.h',
  'src/lib/copy_move.h',
  'src/lib/defer.h',
  'src/lib/enumerate.h',
  'src/lib/format.cpp',
  'src/lib/format.h',
  'src/lib/gtr.h',
  'src/lib/hv_line.h',
  'src/lib/layout_macros.h',
  'src/lib/release_assert.h',
  'src/lib/trace.h',
  'src/lib/unwrap.h',
  'src/main.cpp',
  'src/mainwindow.cpp',
  'src/mainwindow.h',
  'src/options_dialog.cpp',
  'src/options_dialog.h',
  'src/render_dialog.cpp',
  'src/render_dialog.h',
  'src/settings.cpp',
  'src/settings.h',
  'src/vgm.cpp',
  'src/vgm.h',
  'src/wave_writer.cpp',
  'src/wave_writer.h',
  install : true,
)
