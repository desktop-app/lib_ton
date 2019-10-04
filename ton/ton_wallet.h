// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/bytes.h"

namespace Storage {
namespace Cache {
class Database;
} // namespace Cache
} // namespace Storage

namespace Ton {

class RequestSender;

class Wallet final {
public:
	explicit Wallet(const QString &path);
	~Wallet();

	struct OpenError {
		enum class Type {
			None,
			IO,
			WrongPassword,
		};
		Type type = Type::None;
		QString path;

		explicit operator bool() const {
			return (type != Type::None);
		}
	};
	[[nodiscard]] OpenError open(const QByteArray &globalPassword);
	const std::vector<QByteArray> &publicKeys() const;

private:
	[[nodiscard]] OpenError loadSalt();
	[[nodiscard]] OpenError writeNewSalt();
	[[nodiscard]] OpenError openDatabase(const QByteArray &globalPassword);
	[[nodiscard]] OpenError startLibrary();

	const QString _basePath;
	const std::unique_ptr<RequestSender> _lib;
	const std::unique_ptr<Storage::Cache::Database> _db;

	bool _opened = false;
	bytes::vector _salt;
	bytes::vector _globalKey;
	std::vector<QByteArray> _publicKeys;

};

} // namespace Ton
