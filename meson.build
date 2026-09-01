project(
  'pacu',
  'c',
  version: '1.0.0',
  license: 'AGPL-3.0-or-later',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'buildtype=debugoptimized',
    'b_ndebug=if-release'
  ]
)

cc = meson.get_compiler('c')

add_project_arguments(
  '-D_GNU_SOURCE',
  '-D_FILE_OFFSET_BITS=64',
  language: 'c'
)

# Dependencies
libalpm_dep = dependency('libalpm', required: true)
libcurl_dep = dependency('libcurl', required: true)

inc_dir = include_directories('include')

src_files = files(
  'src/main.c',
  'src/util/terminal.c',
  'src/util/fs_utils.c',
  'src/util/config_parser.c',
  'src/util/editor.c',
  'src/util/alpm_helper.c',
  'src/util/log_parser.c',
  'src/util/net_utils.c',
  'src/commands/cmd_edit_sources.c',
  'src/commands/cmd_repo.c',
  'src/commands/cmd_hold.c',
  'src/commands/cmd_mark.c',
  'src/commands/cmd_autoremove.c',
  'src/commands/cmd_cache.c',
  'src/commands/cmd_check_updates.c',
  'src/commands/cmd_why.c',
  'src/commands/cmd_deptree.c',
  'src/commands/cmd_search_file.c',
  'src/commands/cmd_history.c',
  'src/commands/cmd_top_sizes.c',
  'src/commands/cmd_doctor.c',
  'src/commands/cmd_verify.c',
  'src/commands/cmd_diff.c',
  'src/commands/cmd_rank_mirrors.c',
  'src/commands/cmd_keys.c',
  'src/commands/cmd_news.c'
)

# Primary binary: pacu
executable(
  'pacu',
  src_files,
  include_directories: inc_dir,
  dependencies: [libalpm_dep, libcurl_dep],
  install: true
)

# Symlinks installed alongside pacu
install_symlink('pu', pointing_to: 'pacu', install_dir: get_option('bindir'))
install_symlink('pacman-utils', pointing_to: 'pacu', install_dir: get_option('bindir'))
install_symlink('pacedit', pointing_to: 'pacu', install_dir: get_option('bindir'))
install_symlink('pacrepo', pointing_to: 'pacu', install_dir: get_option('bindir'))
install_symlink('pacclean', pointing_to: 'pacu', install_dir: get_option('bindir'))
install_symlink('pacdoctor', pointing_to: 'pacu', install_dir: get_option('bindir'))
install_symlink('pacwhy', pointing_to: 'pacu', install_dir: get_option('bindir'))
install_symlink('pacorphans', pointing_to: 'pacu', install_dir: get_option('bindir'))

# Shell completions
install_data(
  'completions/bash/pacman-utils',
  rename: 'pacu',
  install_dir: get_option('datadir') / 'bash-completion' / 'completions'
)

install_data(
  'completions/zsh/_pacman-utils',
  rename: '_pacu',
  install_dir: get_option('datadir') / 'zsh' / 'site-functions'
)

# Man pages
if get_option('man')
  install_man('man/pacu.8', 'man/pu.8')
endif

# Unit tests
test_util_sources = files(
  'src/util/terminal.c',
  'src/util/fs_utils.c',
  'src/util/config_parser.c',
  'src/util/log_parser.c'
)

test_config_exe = executable(
  'test_config_parser',
  ['tests/test_config_parser.c', test_util_sources],
  include_directories: inc_dir
)
test('config_parser_test', test_config_exe)

test_fs_exe = executable(
  'test_fs_utils',
  ['tests/test_fs_utils.c', test_util_sources],
  include_directories: inc_dir
)
test('fs_utils_test', test_fs_exe)

test_log_exe = executable(
  'test_log_parser',
  ['tests/test_log_parser.c', test_util_sources],
  include_directories: inc_dir
)
test('log_parser_test', test_log_exe)
