# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

import glob, re, binascii, os, sys

sys.dont_write_bytecode = True
scriptPath = os.path.dirname(os.path.realpath(__file__))
sys.path.append(scriptPath + '/../../../lib_tl/tl')
from generate_tl import generate

generate({
  'namespaces': {
    'global': 'Ton::details',
  },
  'prefixes': {
    'type': 'TL',
    'data': 'TLD',
    'id': 'id',
    'construct': 'tl_',
  },
  'types': {
    'typeId': 'uint32',
  },
  'sections': [
  ],

  'skip': [
    'double ? = Double;',
    'string ? = String;',
    'object ? = Object;',
    'function ? = Function;',

    'int32 = Int32;',
    'int53 = Int53;',
    'int64 = Int64;',
    'bytes = Bytes;',
    'secureString = SecureString;',
    'secureBytes = SecureBytes;',

    'vector {t:Type} # [ t ] = Vector t;',
    'withBlock id:ton.blockIdExt function:Function = Object;',
  ],
  'builtin': [
    'double',
    'string',
    'int32',
    'int53',
    'int64',
    'bytes',
    'secureString',
    'secureBytes',
  ],
  'builtinTemplates': [
    'vector',
  ],
  'builtinInclude': 'ton/details/ton_tl_core.h',
  'nullable': [
    'config',
  ],

  'conversion': {
    'include': 'auto/tl/tonlib_api.h',
    'namespace': 'ton::tonlib_api',
    'builtinAdditional': [
      'bool',
    ],
    'builtinInclude': 'ton/details/ton_tl_core_conversion.h',
  },

})
