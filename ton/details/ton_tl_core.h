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

using TLint32 = tl::int_type;
using TLint53 = tl::int64_type;
using TLint64 = tl::int64_type;
using TLdouble = tl::double_type;
using TLstring = tl::string_type;
using TLbytes = tl::bytes_type;
template <typename T>
using TLvector = tl::vector_type<T>;

inline TLint32 tl_int32(int32 value) {
	return tl::make_int(value);
}
inline TLint53 tl_int53(int64 value) {
	return tl::make_int64(value);
}
inline TLint64 tl_int64(int64 value) {
	return tl::make_int64(value);
}
inline TLdouble tl_double(float64 value) {
	return tl::make_double(value);
}
inline TLstring tl_string(const std::string &v) {
	return tl::make_string(v);
}
inline TLstring tl_string(const QString &v) {
	return tl::make_string(v);
}
inline TLbytes tl_string(const QByteArray &v) {
	return tl::make_string(v);
}
inline TLstring tl_string(const char *v) {
	return tl::make_string(v);
}
inline TLstring tl_string() {
	return tl::make_string();
}
inline TLbytes tl_bytes(const QByteArray &v) {
	return tl::make_bytes(v);
}
inline TLbytes tl_bytes(QByteArray &&v) {
	return tl::make_bytes(std::move(v));
}
inline TLbytes tl_bytes() {
	return tl::make_bytes();
}
inline TLbytes tl_bytes(bytes::const_span buffer) {
	return tl::make_bytes(buffer);
}
inline TLbytes tl_bytes(const bytes::vector &buffer) {
	return tl::make_bytes(buffer);
}
template <typename T>
inline TLvector<T> tl_vector(uint32 count) {
	return tl::make_vector<T>(count);
}
template <typename T>
inline TLvector<T> tl_vector(uint32 count, const T &value) {
	return tl::make_vector<T>(count, value);
}
template <typename T>
inline TLvector<T> tl_vector(const QVector<T> &v) {
	return tl::make_vector<T>(v);
}
template <typename T>
inline TLvector<T> tl_vector(QVector<T> &&v) {
	return tl::make_vector<T>(std::move(v));
}
template <typename T>
inline TLvector<T> tl_vector() {
	return tl::make_vector<T>();
}

class TLsecureString {
public:
	QByteArray v;

};
using TLsecureBytes = TLsecureString;

} // namespace Ton::details

namespace tl {

template <typename Accumulator>
struct Writer;

template <typename Prime>
struct Reader;

template <>
struct Writer<QByteArray> final {
	static void PutBytes(QByteArray &to, const void *bytes, uint32 count) {
		constexpr auto kPrime = sizeof(uint32);
		const auto primes = (count / kPrime) + (count % kPrime ? 1 : 0);
		const auto size = to.size();
		to.resize(size + (primes * kPrime));
		memcpy(to.data() + size, bytes, count);
	}
	static void Put(QByteArray &to, uint32 value) {
		PutBytes(to, &value, sizeof(value));
	}
};

template <>
struct Reader<char> final {
	[[nodiscard]] static bool HasBytes(
			uint32 count,
			const char *from,
			const char *end) {
		constexpr auto kPrime = sizeof(uint32);
		const auto primes = (count / kPrime) + (count % kPrime ? 1 : 0);
		return (end - from) >= primes * kPrime;
	}
	static void GetBytes(
			void *bytes,
			uint32 count,
			const char *&from,
			const char *end) {
		Expects(HasBytes(count, from, end));

		constexpr auto kPrime = sizeof(uint32);
		const auto primes = (count / kPrime) + (count % kPrime ? 1 : 0);
		memcpy(bytes, from, count);
		from += primes * kPrime;
	}
	[[nodiscard]] static bool Has(
			uint32 primes,
			const char *from,
			const char *end) {
		return HasBytes(primes * sizeof(uint32), from, end);
	}
	[[nodiscard]] static uint32 Get(const char *&from, const char *end) {
		auto result = uint32();
		GetBytes(&result, sizeof(result), from, end);
		return result;
	}
};

} // namespace tl
