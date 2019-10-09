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
    'construct': 'make_',
  },
  'types': {
    'prime': 'char',
    'typeId': 'uint32',
    'buffer': 'QByteArray',
  },
  'sections': [
    'read-write',
  ],

  'builtin': [
    'string',
    'int32',
    'int64',
    'bytes',
  ],
  'builtinTemplates': [
    'vector',
  ],
  'builtinInclude': 'ton/details/ton_tl_core.h',

})
