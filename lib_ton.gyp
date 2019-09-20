# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

{
  'includes': [
    '../gyp/helpers/common/common.gypi',
  ],
  'targets': [{
    'target_name': 'lib_ton',
    'includes': [
      '../gyp/helpers/common/library.gypi',
      '../gyp/helpers/modules/qt.gypi',
      '../gyp/helpers/modules/openssl.gypi',
    ],
    'variables': {
      'src_loc': '.',
      'ton_loc': '<(libs_loc)/ton',
      'ton_libs': [
        '-ltdutils',
        '-ltdactor',
        '-ladnllite',
        '-ltl_api',
        '-lton_crypto',
        '-lton_block',
        '-ltl_tonlib_api',
        '-ltonlib',
        '-ltdnet',
        '-lkeys',
        '-llite-client-common',
        '-ltl-utils',
        '-ltl-lite-utils',
        '-ltl_lite_api',
        '-lcrc32c',
      ],
    },
    'dependencies': [
      '<(submodules_loc)/lib_base/lib_base.gyp:lib_base',
    ],
    'export_dependent_settings': [
      '<(submodules_loc)/lib_base/lib_base.gyp:lib_base',
    ],
    'sources': [
      '<(src_loc)/ton/ton_utility.cpp',
      '<(src_loc)/ton/ton_utility.h',
    ],
    'include_dirs': [
      '<(src_loc)',
      '<(ton_loc)',
      '<(ton_loc)/crypto',
      '<(ton_loc)/tdactor',
      '<(ton_loc)/tdutils',
      '<(ton_loc)/tonlib',
      '<(ton_loc)/tl',
      '<(ton_loc)/tl/generate',
    ],
    'configurations': {
      'Debug': {
        'include_dirs': [
          '<(ton_loc)/build-debug/tdutils',
        ],
      },
      'Release': {
        'include_dirs': [
          '<(ton_loc)/build/tdutils',
        ],
      },
    },
    'direct_dependent_settings': {
      'include_dirs': [
        '<(src_loc)',
      ],
      'conditions': [[ 'build_win', {
        'link_settings': {
          'libraries': [
            '-lNormaliz',
          ],
        },
        'configurations': {
          'Debug': {
            'library_dirs': [
              '<(libs_loc)/ton/build-debug/tdutils/Debug',
              '<(libs_loc)/ton/build-debug/tdactor/Debug',
              '<(libs_loc)/ton/build-debug/adnl/Debug',
              '<(libs_loc)/ton/build-debug/tl/Debug',
              '<(libs_loc)/ton/build-debug/crypto/Debug',
              '<(libs_loc)/ton/build-debug/tl/Debug',
              '<(libs_loc)/ton/build-debug/tonlib/Debug',
              '<(libs_loc)/ton/build-debug/tdnet/Debug',
              '<(libs_loc)/ton/build-debug/keys/Debug',
              '<(libs_loc)/ton/build-debug/lite-client/Debug',
              '<(libs_loc)/ton/build-debug/tl-utils/Debug',
              '<(libs_loc)/ton/build-debug/third-party/crc32c/Debug',
            ],
          },
          'Release': {
            'library_dirs': [
              '<(libs_loc)/ton/build/tdutils/Release',
              '<(libs_loc)/ton/build/tdactor/Release',
              '<(libs_loc)/ton/build/adnl/Release',
              '<(libs_loc)/ton/build/tl/Release',
              '<(libs_loc)/ton/build/crypto/Release',
              '<(libs_loc)/ton/build/tl/Release',
              '<(libs_loc)/ton/build/tonlib/Release',
              '<(libs_loc)/ton/build/tdnet/Release',
              '<(libs_loc)/ton/build/keys/Release',
              '<(libs_loc)/ton/build/lite-client/Release',
              '<(libs_loc)/ton/build/tl-utils/Release',
              '<(libs_loc)/ton/build/third-party/crc32c/Release',
            ],
          },
        },
      }, {
        'configurations': {
          'Debug': {
            'library_dirs': [
              '<(libs_loc)/ton/build-debug/tdutils',
              '<(libs_loc)/ton/build-debug/tdactor',
              '<(libs_loc)/ton/build-debug/adnl',
              '<(libs_loc)/ton/build-debug/tl',
              '<(libs_loc)/ton/build-debug/crypto',
              '<(libs_loc)/ton/build-debug/tonlib',
              '<(libs_loc)/ton/build-debug/tdnet',
              '<(libs_loc)/ton/build-debug/keys',
              '<(libs_loc)/ton/build-debug/lite-client',
              '<(libs_loc)/ton/build-debug/tl-utils',
              '<(libs_loc)/ton/build-debug/third-party/crc32c',
            ],
          },
          'Release': {
            'library_dirs': [
              '<(libs_loc)/ton/build/tdutils',
              '<(libs_loc)/ton/build/tdactor',
              '<(libs_loc)/ton/build/adnl',
              '<(libs_loc)/ton/build/tl',
              '<(libs_loc)/ton/build/crypto',
              '<(libs_loc)/ton/build/tonlib',
              '<(libs_loc)/ton/build/tdnet',
              '<(libs_loc)/ton/build/keys',
              '<(libs_loc)/ton/build/lite-client',
              '<(libs_loc)/ton/build/tl-utils',
              '<(libs_loc)/ton/build/third-party/crc32c',
            ],
          },
        },
      }], [ 'build_mac', {
        'xcode_settings': {
          'OTHER_LDFLAGS': [
            '<@(ton_libs)',
          ],
        },
      }, {
        'libraries': [
          '<@(ton_libs)',
        ],
      }]],
    },
  }],
}
