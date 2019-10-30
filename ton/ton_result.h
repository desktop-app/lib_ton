// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <tl/expected.hpp>

namespace Ton {

struct Error {
	enum class Type {
		IO,
		WrongPassword,
		TonLib,
		Web,
	};

	Error(Type type, const QString &details = {});

	template <typename T>
	operator tl::expected<T, Error>() const & {
		return tl::make_unexpected(*this);
	}

	template <typename T>
	operator tl::expected<T, Error>() && {
		return tl::make_unexpected(std::move(*this));
	}

	Type type;
	QString details;
};

inline Error::Error(Type type, const QString &details)
: type(type)
, details(details) {
}

template <typename T = void>
using Result = tl::expected<T, Error>;

template <typename T = void>
using Callback = Fn<void(Result<T>)>;

namespace details {

template <typename T, typename Argument>
void InvokeCallback(const Callback<T> &callback, Argument &&argument) {
	if (const auto onstack = callback) {
		onstack(std::forward<Argument>(argument));
	}
}

template <typename T>
void InvokeCallback(const Callback<T> &callback) {
	if (const auto onstack = callback) {
		onstack(Result<T>());
	}
}

} // namespace details
} // namespace Ton
