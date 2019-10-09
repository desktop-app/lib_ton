// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "auto/tl/tonlib_api.h"
#include "ton/details/ton_tl_core.h"
#include "ton_tl.h"

namespace Ton::details {

TLstring tl_from(std::string &&value);
std::string tl_to(const TLstring &value);
TLsecureString tl_from(td::SecureString &&value);
td::SecureString tl_to(const TLsecureString &value);
TLint32 tl_from(std::int32_t value);
std::int32_t tl_to(const TLint32 &value);
TLint64 tl_from(std::int64_t value);
std::int64_t tl_to(const TLint64 &value);
TLbool tl_from(bool value);
bool tl_to(const TLbool &value);

template <typename T>
auto tl_from(std::vector<T> &&value) {
	using U = decltype(tl_from(std::declval<T>()));
	auto result = QVector<U>();
	result.reserve(value.size());
	for (auto &element : value) {
		result.push_back(tl_from(std::move(element)));
	}
	return tl_vector<U>(std::move(result));
}

template <typename T>
auto tl_to(const TLvector<T> &value) {
	using U = decltype(tl_to(std::declval<T>()));
	auto result = std::vector<U>();
	result.reserve(value.v.size());
	for (const auto &element : value.v) {
		result.push_back(tl_to(element));
	}
	return result;
}

} // namespace Ton::details
