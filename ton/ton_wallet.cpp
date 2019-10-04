// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/ton_wallet.h"

#include "ton/ton_request_sender.h"
#include "storage/cache/storage_cache_database.h"
#include "storage/storage_encryption.h"
#include "base/openssl_help.h"

#include <crl/crl_async.h>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace Ton {
namespace {

constexpr auto kSaltSize = size_type(32);
constexpr auto kIterations = 100'000;

[[nodiscard]] QString SubPath(const QString &basePath, const QString &name) {
	Expects(basePath.endsWith('/'));

	return basePath + name;
}

[[nodiscard]] QString LibraryStoragePath(const QString &basePath) {
	return SubPath(basePath, "lib");
}

[[nodiscard]] QString DatabasePath(const QString &basePath) {
	return SubPath(basePath, "data");
}

[[nodiscard]] QString SaltPath(const QString &basePath) {
	return SubPath(basePath, "salt");
}

[[nodiscard]] Storage::Cache::Database::Settings DatabaseSettings() {
	auto result = Storage::Cache::Database::Settings();
	result.totalSizeLimit = 0;
	result.totalTimeLimit = 0;
	result.trackEstimatedTime = false;
	return result;
}

[[nodiscard]] std::unique_ptr<Storage::Cache::Database> MakeDatabase(
		const QString &basePath) {
	return std::make_unique<Storage::Cache::Database>(
		DatabasePath(basePath),
		DatabaseSettings());
}

[[nodiscard]] bytes::vector DatabaseKey(
		bytes::const_span password,
		bytes::const_span salt) {
	const auto iterations = password.empty() ? 1 : kIterations;
	return openssl::Pbkdf2Sha512(password, salt, iterations);
}

} // namespace

Wallet::Wallet(const QString &path)
: _basePath(path.endsWith('/') ? path : (path + '/'))
, _lib(std::make_unique<RequestSender>())
, _db(MakeDatabase(_basePath)) {
	Expects(!path.isEmpty());

	crl::async([] {
		// Init random, because it is slow.
		static_cast<void>(openssl::RandomValue<uint8>());
	});
}

Wallet::~Wallet() = default;

Wallet::OpenError Wallet::open(const QByteArray &globalPassword) {
	Expects(!_opened);

	if (const auto error = loadSalt()) {
		return error;
	}
	if (const auto error = openDatabase(globalPassword)) {
		return error;
	}
	if (const auto error = startLibrary()) {
		return error;
	}
	_opened = true;
	return {};
}

const std::vector<QByteArray> &Wallet::publicKeys() const {
	Expects(_opened);

	return _publicKeys;
}

Wallet::OpenError Wallet::loadSalt() {
	const auto path = SaltPath(_basePath);
	const auto failed = [&] {
		return OpenError{ OpenError::Type::IO, path };
	};
	auto info = QFileInfo(path);
	if (info.isSymLink()) {
		if (!QFile::remove(path)) {
			return failed();
		}
	} else if (info.isDir()) {
		if (!QDir(path).removeRecursively()) {
			return failed();
		}
	} else if (info.exists()) {
		if (info.size() == kSaltSize) {
			auto file = QFile(path);
			if (!file.open(QIODevice::ReadOnly)) {
				return { OpenError::Type::IO, path };
			}
			const auto salt = file.readAll();
			if (salt.size() != kSaltSize) {
				return { OpenError::Type::IO, path };
			}
			_salt = bytes::make_vector(salt);
			return {};
		} else if (!QFile::remove(path)) {
			return failed();
		}
	}
	return writeNewSalt();
}

Wallet::OpenError Wallet::writeNewSalt() {
	auto db = QDir(DatabasePath(_basePath));
	if (db.exists() && !db.removeRecursively()) {
		return { OpenError::Type::IO, db.path() };
	}
	auto lib = QDir(LibraryStoragePath(_basePath));
	if (lib.exists() && !lib.removeRecursively()) {
		return { OpenError::Type::IO, lib.path() };
	}

	if (!QDir().mkpath(_basePath)) {
		return { OpenError::Type::IO, _basePath };
	}
	const auto path = SaltPath(_basePath);
	const auto failed = [&] {
		return OpenError{ OpenError::Type::IO, path };
	};
	_salt = bytes::vector(kSaltSize);
	bytes::set_random(_salt);
	auto file = QFile(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return failed();
	}
	const auto salt = reinterpret_cast<const char*>(_salt.data());
	if (file.write(salt, kSaltSize) != kSaltSize) {
		return failed();
	}
	return {};
}

Wallet::OpenError Wallet::openDatabase(const QByteArray &globalPassword) {
	Expects(_salt.size() == kSaltSize);

	auto semaphore = crl::semaphore();
	auto key = DatabaseKey(bytes::make_span(globalPassword), _salt);
	auto dbKey = Storage::EncryptionKey(base::duplicate(key));
	auto dbError = Storage::Cache::Error();
	_db->open(std::move(dbKey), [&](Storage::Cache::Error error) {
		dbError = error;
		semaphore.release();
	});
	semaphore.acquire();

	using Type = Storage::Cache::Error::Type;
	if (dbError.type == Type::IO || dbError.type == Type::LockFailed) {
		return { OpenError::Type::IO, dbError.path };
	} else if (dbError.type == Type::WrongKey) {
		return { OpenError::Type::WrongPassword };
	}
	_globalKey = std::move(key);
	return {};
}

Wallet::OpenError Wallet::startLibrary() {
	const auto path = LibraryStoragePath(_basePath);
	if (!QDir().mkpath(path)) {
		return { OpenError::Type::IO, path };
	}

	auto semaphore = crl::semaphore();
	auto tlError = std::optional<TLError>();
	_lib->request(TLInit(
		make_options(
			nullptr,
			make_keyStoreTypeDirectory(tl::make_string(path)))
	)).done_async([&] {
		semaphore.release();
	}).fail_async([&](const TLError &error) {
		tlError = error;
		semaphore.release();
	}).send();
	semaphore.acquire();

	if (tlError) {
		_db->close();
		return tlError->match([&](const TLDerror &data) -> OpenError {
			return { OpenError::Type::IO, tl::utf16(data.vmessage()) };
		});
	}
	return {};
}

} // namespace Ton
