// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_tl_core_conversion.h"

namespace Ton::details {

TLstring tl_from(std::string &&value) {
	return tl::make_string(value);
}

std::string tl_to(const TLstring &value) {
	return std::string(value.v.data(), value.v.size());
}

TLsecureString tl_from(td::SecureString &&value) {
	return TLsecureString{ QByteArray(value.data(), value.size()) };
}

td::SecureString tl_to(const TLsecureString &value) {
	return td::SecureString{ value.v.data(), size_t(value.v.size()) };
}

TLint32 tl_from(std::int32_t value) {
	return tl::make_int(value);
}

std::int32_t tl_to(const TLint32 &value) {
	return value.v;
}

TLint64 tl_from(std::int64_t value) {
	return tl::make_long(value);
}

std::int64_t tl_to(const TLint64 &value) {
	return value.v;
}

TLbool tl_from(bool value) {
	return value ? make_boolTrue() : make_boolFalse();
}

bool tl_to(const TLbool &value) {
	return (value.type() == id_boolTrue);
}

} // namespace Ton::details
