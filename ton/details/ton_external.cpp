// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_external.h"

#include "ton/details/ton_request_sender.h"
#include "storage/cache/storage_cache_database.h"
#include "storage/storage_encryption.h"
#include "base/openssl_help.h"

#include <crl/crl_async.h>
#include <crl/crl_on_main.h>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDataStream>

namespace Ton::details {
namespace {

constexpr auto kSaltSize = size_type(32);
constexpr auto kIterations = 100'000;
constexpr auto kWalletListKey = Storage::Cache::Key{ 1ULL, 1ULL };

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

[[nodiscard]] Storage::EncryptionKey DatabaseKey(
		bytes::const_span password,
		bytes::const_span salt) {
	const auto iterations = password.empty() ? 1 : kIterations;
	return Storage::EncryptionKey(
		openssl::Pbkdf2Sha512(password, salt, iterations));
}

[[nodiscard]] QByteArray SerializeList(const WalletList &list) {
	auto result = QByteArray();
	{
		auto stream = QDataStream(&result, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_5_3);
		stream << qint32(list.entries.size());
		for (const auto &[key, secret] : list.entries) {
			stream << key << secret;
		}
	}
	return result;
}

[[nodiscard]] WalletList DeserializeList(QByteArray value) {
	if (value.isEmpty()) {
		return {};
	}

	auto result = WalletList();
	auto stream = QDataStream(&value, QIODevice::ReadOnly);
	stream.setVersion(QDataStream::Qt_5_3);
	auto count = qint32();
	stream >> count;
	if (count <= 0 || stream.status() != QDataStream::Ok) {
		return {};
	}
	result.entries.resize(count);
	for (auto &[key, secret] : result.entries) {
		stream >> key >> secret;
	}
	if (stream.status() != QDataStream::Ok || !stream.atEnd()) {
		return {};
	}
	return result;
}

} // namespace

std::optional<Error> ErrorFromStorage(const Storage::Cache::Error &error) {
	using Type = Storage::Cache::Error::Type;
	if (error.type == Type::IO || error.type == Type::LockFailed) {
		return Error{ Error::Type::IO, error.path };
	} else if (error.type == Type::WrongKey) {
		return Error{ Error::Type::WrongPassword };
	}
	return std::nullopt;
}

External::External(const QString &path)
: _basePath(path.endsWith('/') ? path : (path + '/'))
, _db(DatabasePath(_basePath), DatabaseSettings()) {
	Expects(!path.isEmpty());
}

void External::open(
		const QByteArray &globalPassword,
		Callback<WalletList> done) {
	Expects(_state == State::Initial);

	_state = State::Opening;
	if (const auto result = loadSalt(); !result) {
		_state = State::Initial;
		InvokeCallback(done, result.error());
		return;
	}
	openDatabase(globalPassword, [=](Result<WalletList> result) {
		if (!result) {
			_state = State::Initial;
			InvokeCallback(done, result);
			return;
		}
		const auto list = std::move(*result);
		startLibrary([=](Result<> result) {
			if (!result) {
				_state = State::Initial;
				InvokeCallback(done, result.error());
				return;
			}
			_state = State::Opened;
			InvokeCallback(done, list);
		});
	});
}

RequestSender &External::lib() {
	return _lib;
}

Fn<void(WalletList, Callback<>)> External::saveListMethod() {
	return [=](WalletList list, Callback<> done) {
		const auto weak = base::make_weak(this);
		auto saved = [=](Storage::Cache::Error error) {
			crl::on_main(weak, [=] {
				if (const auto bad = ErrorFromStorage(error)) {
					InvokeCallback(done, *bad);
				} else {
					InvokeCallback(done);
				}
			});
		};
		if (list.entries.empty()) {
			_db.remove(kWalletListKey, std::move(saved));
		} else {
			_db.put(kWalletListKey, SerializeList(list), std::move(saved));
		}
	};
}

Result<> External::loadSalt() {
	const auto path = SaltPath(_basePath);
	const auto failed = [&] {
		return Error{ Error::Type::IO, path };
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
				return Error{ Error::Type::IO, path };
			}
			const auto salt = file.readAll();
			if (salt.size() != kSaltSize) {
				return Error{ Error::Type::IO, path };
			}
			_salt = bytes::make_vector(salt);
			return {};
		} else if (!QFile::remove(path)) {
			return failed();
		}
	}
	return writeNewSalt();
}

Result<> External::writeNewSalt() {
	auto db = QDir(DatabasePath(_basePath));
	if (db.exists() && !db.removeRecursively()) {
		return Error{ Error::Type::IO, db.path() };
	}
	auto lib = QDir(LibraryStoragePath(_basePath));
	if (lib.exists() && !lib.removeRecursively()) {
		return Error{ Error::Type::IO, lib.path() };
	}

	if (!QDir().mkpath(_basePath)) {
		return Error{ Error::Type::IO, _basePath };
	}
	const auto path = SaltPath(_basePath);
	const auto failed = [&] {
		return Error{ Error::Type::IO, path };
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

void External::openDatabase(
		const QByteArray &globalPassword,
		Callback<WalletList> done) {
	Expects(_salt.size() == kSaltSize);

	const auto weak = base::make_weak(this);
	auto key = DatabaseKey(bytes::make_span(globalPassword), _salt);
	_db.open(std::move(key), [=](Storage::Cache::Error error) {
		crl::on_main(weak, [=] {
			if (const auto bad = ErrorFromStorage(error)) {
				InvokeCallback(done, *bad);
			} else {
				_db.get(kWalletListKey, [=](QByteArray &&value) {
					crl::on_main(weak, [=] {
						InvokeCallback(done, DeserializeList(value));
					});
				});
			}
		});
	});
}

void External::startLibrary(Callback<> done) {
	const auto path = LibraryStoragePath(_basePath);
	if (!QDir().mkpath(path)) {
		InvokeCallback(done, Error{ Error::Type::IO, path });
		return;
	}

	_lib.request(TLInit(
		make_options(
			nullptr,
			make_keyStoreTypeDirectory(tl::make_string(path)))
	)).done([=] {
		InvokeCallback(done);
	}).fail([=](const TLError &error) {
		_db.close();
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

} // namespace Ton::details
