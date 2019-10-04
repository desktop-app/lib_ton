// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "tl/tl_basic_types.h"
#include "base/match_method.h"

namespace Ton::details {

using TLdouble = tl::double_type;
using TLstring = tl::string_type;
using TLint32 = tl::int_type;
using TLint53 = tl::long_type;
using TLint64 = tl::long_type;
template <typename T>
using TLvector = tl::vector_type<T>;
using TLbytes = tl::bytes_type;

class TLsecureString {
public:
	QByteArray v;

};
using TLsecureBytes = TLsecureString;

} // namespace Ton::details
