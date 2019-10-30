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
      '../gyp/helpers/modules/pch.gypi',
      '../gyp/helpers/modules/openssl.gypi',
    ],
    'variables': {
      'src_loc': '.',
      'ton_loc': '<(libs_loc)/ton',
      'ton_libs': [
        '-ltonlib',
        '-ltl_tonlib_api',
        '-lsmc-envelope',
        '-llite-client-common',
        '-lton_block',
        '-ladnllite',
        '-ltl-lite-utils',
        '-lkeys',
        '-ltl-utils',
        '-ltl_api',
        '-ltl_lite_api',
        '-ltdnet',
        '-ltdactor',
        '-lton_crypto',
        '-ltdutils',
        '-lcrc32c',
      ],
      'pch_source': '<(src_loc)/ton/details/ton_pch.cpp',
      'pch_header': '<(src_loc)/ton/details/ton_pch.h',
    },
    'dependencies': [
      '<(submodules_loc)/lib_base/lib_base.gyp:lib_base',
      '<(submodules_loc)/lib_tl/lib_tl.gyp:lib_tl',
      '<(submodules_loc)/lib_storage/lib_storage.gyp:lib_storage',
    ],
    'export_dependent_settings': [
      '<(submodules_loc)/lib_base/lib_base.gyp:lib_base',
      '<(submodules_loc)/lib_storage/lib_storage.gyp:lib_storage',
    ],
    'sources': [
      '<(src_loc)/ton/details/ton_account_viewers.cpp',
      '<(src_loc)/ton/details/ton_account_viewers.h',
      '<(src_loc)/ton/details/ton_external.cpp',
      '<(src_loc)/ton/details/ton_external.h',
      '<(src_loc)/ton/details/ton_client.cpp',
      '<(src_loc)/ton/details/ton_client.h',
      '<(src_loc)/ton/details/ton_key_creator.cpp',
      '<(src_loc)/ton/details/ton_key_creator.h',
      '<(src_loc)/ton/details/ton_key_destroyer.cpp',
      '<(src_loc)/ton/details/ton_key_destroyer.h',
      '<(src_loc)/ton/details/ton_parse_state.cpp',
      '<(src_loc)/ton/details/ton_parse_state.h',
      '<(src_loc)/ton/details/ton_password_changer.cpp',
      '<(src_loc)/ton/details/ton_password_changer.h',
      '<(src_loc)/ton/details/ton_request_sender.cpp',
      '<(src_loc)/ton/details/ton_request_sender.h',
      '<(src_loc)/ton/details/ton_storage.cpp',
      '<(src_loc)/ton/details/ton_storage.h',
      '<(src_loc)/ton/details/ton_tl_core.h',
      '<(src_loc)/ton/details/ton_tl_core_conversion.cpp',
      '<(src_loc)/ton/details/ton_tl_core_conversion.h',
      '<(src_loc)/ton/ton_account_viewer.cpp',
      '<(src_loc)/ton/ton_account_viewer.h',
      '<(src_loc)/ton/ton_result.h',
      '<(src_loc)/ton/ton_settings.h',
      '<(src_loc)/ton/ton_state.cpp',
      '<(src_loc)/ton/ton_state.h',
      '<(src_loc)/ton/ton_utility.cpp',
      '<(src_loc)/ton/ton_utility.h',
      '<(src_loc)/ton/ton_wallet.cpp',
      '<(src_loc)/ton/ton_wallet.h',
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
      '<(SHARED_INTERMEDIATE_DIR)',
      '<(third_party_loc)/expected/include',
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
        '<(third_party_loc)/expected/include',
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
      }], [ 'build_linux', {
        'libraries': [
          '<(linux_lib_ssl)',
          '<(linux_lib_crypto)',
        ],
      }]],
    },
    'actions': [{
      'action_name': 'codegen_tl',
      'inputs': [
        '<(src_loc)/ton/details/ton_tl_generate.py',
        '<(submodules_loc)/lib_tl/tl/generate_tl.py',
        '<(libs_loc)/ton/tl/generate/scheme/tonlib_api.tl',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/ton_tl.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/ton_tl.h',
        '<(SHARED_INTERMEDIATE_DIR)/ton_tl-conversion.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/ton_tl-conversion.h',
      ],
      'action': [
        'python', '<(src_loc)/ton/details/ton_tl_generate.py',
        '-o', '<(SHARED_INTERMEDIATE_DIR)/ton_tl',
        '<(libs_loc)/ton/tl/generate/scheme/tonlib_api.tl',
      ],
      'message': 'codegen_tl-ing tonlib_api.tl..',
      'process_outputs_as_sources': 1,
    }, {
      'action_name': 'codegen_storage_tl',
      'inputs': [
        '<(src_loc)/ton/details/ton_storage_tl_generate.py',
        '<(submodules_loc)/lib_tl/tl/generate_tl.py',
        '<(src_loc)/ton/details/ton_storage.tl',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/ton_storage_tl.cpp',
        '<(SHARED_INTERMEDIATE_DIR)/ton_storage_tl.h',
      ],
      'action': [
        'python', '<(src_loc)/ton/details/ton_storage_tl_generate.py',
        '-o', '<(SHARED_INTERMEDIATE_DIR)/ton_storage_tl',
        '<(src_loc)/ton/details/ton_storage.tl',
      ],
      'message': 'codegen_tl-ing ton_storage.tl..',
      'process_outputs_as_sources': 1,
    }],
  }],
}
