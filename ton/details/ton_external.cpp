// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_external.h"

#include "ton/details/ton_request_sender.h"
#include "ton/details/ton_storage.h"
#include "ton/details/ton_parse_state.h"
#include "ton/ton_settings.h"
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
constexpr auto kMaxTonLibLogSize = 50 * 1024 * 1024;
constexpr auto kErrorsTillSetConfig = 3;
constexpr auto kDebugVerbosity = 10;

std::atomic<bool> LoggingEnabled = false;

[[nodiscard]] QString SubPath(const QString &basePath, const QString &name) {
	Expects(basePath.endsWith('/'));

	return basePath + name;
}

[[nodiscard]] QString LibraryStoragePath(const QString &basePath) {
	return SubPath(basePath, "lib");
}

[[nodiscard]] QString DatabasePath(const QString &basePath) {
	return SubPath(basePath, "db");
}

[[nodiscard]] QString SaltPath(const QString &basePath) {
	return SubPath(basePath, "salt");
}

[[nodiscard]] QString TonLibLogPath(const QString &basePath) {
	return SubPath(basePath, "tonlib_log.txt");
}

[[nodiscard]] Storage::Cache::Database::Settings DatabaseSettings() {
	auto result = Storage::Cache::Database::Settings();
	result.totalSizeLimit = 0;
	result.totalTimeLimit = 0;
	result.trackEstimatedTime = false;
	return result;
}

[[nodiscard]] Storage::DatabasePointer MakeDatabase(
		const QString &basePath) {
	static Storage::Databases All;
	return All.get(DatabasePath(basePath), DatabaseSettings());
}

[[nodiscard]] Storage::EncryptionKey DatabaseKey(
		bytes::const_span password,
		bytes::const_span salt) {
	const auto iterations = password.empty() ? 1 : kIterations;
	return Storage::EncryptionKey(
		openssl::Pbkdf2Sha512(password, salt, iterations));
}

} // namespace

External::External(const QString &path, Fn<void(Update)> &&updateCallback)
: _basePath(path.endsWith('/') ? path : (path + '/'))
, _updateCallback(std::move(updateCallback))
, _lib(generateUpdateCallback())
, _db(MakeDatabase(_basePath)) {
	Expects(!path.isEmpty());
}

Fn<void(const TLUpdate &)> External::generateUpdateCallback() const {
	if (!_updateCallback) {
		return nullptr;
	}
	const auto weak = base::make_weak(this);
	return [=](const TLUpdate &update) {
		crl::on_main(weak, [=, update = Parse(update)]() mutable {
			_updateCallback(std::move(update));
		});
	};
}

void External::open(
		const QByteArray &globalPassword,
		const Settings &defaultSettings,
		Callback<WalletList> done) {
	Expects(_state == State::Initial);

	_state = State::Opening;
	if (const auto result = loadSalt(); !result) {
		_state = State::Initial;
		InvokeCallback(done, result.error());
		return;
	}
	_settings = defaultSettings;
	openDatabase(globalPassword, [=](Result<Settings> result) {
		if (!result) {
			_state = State::Initial;
			InvokeCallback(done, result.error());
			return;
		}
		if (!result->test.config.isEmpty()) {
			applyLocalSettings(*result);
		}
		const auto future = std::make_shared<std::optional<WalletList>>();
		const auto loadedWallets = crl::guard(this, [=](WalletList &&list) {
			if (_state == State::Opened) {
				InvokeCallback(done, std::move(list));
			} else {
				*future = std::move(list);
			}
		});
		LoadWalletList(_db.get(), _settings.useTestNetwork, loadedWallets);
		startLibrary([=](Result<> result) {
			if (!result) {
				_state = State::Initial;
				InvokeCallback(done, result.error());
				return;
			}
			_state = State::Opened;
			if (*future) {
				if (_configUpgrade != ConfigUpgrade::None) {
					updateSettings(_settings, nullptr);
					if (_updateCallback) {
						_updateCallback({ _configUpgrade });
					}
				}
				InvokeCallback(done, std::move(**future));
			}
		});
	});
}

void External::applyLocalSettings(const Settings &localSettings) {
	if (localSettings.version < 3
		&& localSettings.useTestNetwork
		&& _settings.version == 3
		&& !_settings.useTestNetwork) {
		_configUpgrade = ConfigUpgrade::TestnetToMainnet;
		_settings.test = localSettings.test;
		_settings.useTestNetwork = true;
	} else if (localSettings.version == 0
		&& _settings.version == 2
		&& _settings.test.blockchainName == "testnet2") {
		_configUpgrade = ConfigUpgrade::TestnetToTestnet2;
	} else {
		_settings = localSettings;
	}
}

const Settings &External::settings() const {
	Expects(_state == State::Opened);

	return _settings;
}

void External::updateSettings(
		const Settings &settings,
		Callback<int64> done) {
	Expects(_settings.useTestNetwork == settings.useTestNetwork);

	const auto &was = _settings.net();
	const auto &now = settings.net();
	const auto clear = (was.blockchainName != now.blockchainName);
	_settings = settings;
	const auto config = now.config;
	_lib.request(TLoptions_SetConfig(
		tl_config(
			tl_string(now.config),
			tl_string(now.blockchainName),
			tl_from(_settings.useNetworkCallbacks),
			tl_from(clear))
	)).done([=](const TLoptions_ConfigInfo &result) {
		const auto walletId = WalletId(result);
		const auto saved = [=](Result<> result) {
			if (!result) {
				InvokeCallback(done, result.error());
			}
			InvokeCallback(done, walletId);
		};
		SaveSettings(_db.get(), settings, crl::guard(this, saved));
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void External::switchNetwork(Callback<int64> done) {
	_settings.useTestNetwork = !_settings.useTestNetwork;

	updateSettings(_settings, std::move(done));
}

void External::resetNetwork() {
	Expects(_state == State::Opened);

	updateSettings(_settings, nullptr);
}

RequestSender &External::lib() {
	return _lib;
}

Storage::Cache::Database &External::db() {
	return *_db;
}

void External::EnableLogging(bool enabled, const QString &basePath) {
	LoggingEnabled = enabled;
	if (enabled) {
		RequestSender::Execute(TLSetLogStream(
			tl_logStreamFile(
				tl_string(TonLibLogPath(basePath)),
				tl_int53(kMaxTonLibLogSize))));
		RequestSender::Execute(TLSetLogVerbosityLevel(
			tl_int32(kDebugVerbosity)));
	} else {
		RequestSender::Execute(TLSetLogStream(tl_logStreamEmpty()));
	}
}

void External::LogMessage(const QString &message) {
	if (!LoggingEnabled) {
		return;
	}
	RequestSender::Execute(TLAddLogMessage(
		tl_int32(kDebugVerbosity),
		tl_string(message)));
}

int64 External::WalletId(const TLoptions_ConfigInfo &data) {
	return data.match([](const TLDoptions_configInfo &data) {
		return data.vdefault_wallet_id().v;
	});
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
		Callback<Settings> done) {
	Expects(_salt.size() == kSaltSize);

	const auto weak = base::make_weak(this);
	auto key = DatabaseKey(bytes::make_span(globalPassword), _salt);
	_db->open(std::move(key), [=](Storage::Cache::Error error) {
		crl::on_main(weak, [=] {
			if (const auto bad = ErrorFromStorage(error)) {
				InvokeCallback(done, *bad);
			} else {
				const auto loaded = [=](Settings &&result) {
					InvokeCallback(done, std::move(result));
				};
				LoadSettings(_db.get(), crl::guard(weak, loaded));
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
		tl_options(
			nullptr,
			tl_keyStoreTypeDirectory(tl_string(path)))
	)).done([=] {
		InvokeCallback(done);
	}).fail([=](const TLError &error) {
		_db->close();
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void External::start(Callback<int64> done) {
	_lib.request(TLoptions_SetConfig(
		tl_config(
			tl_string(_settings.net().config),
			tl_string(_settings.net().blockchainName),
			tl_from(_settings.useNetworkCallbacks),
			tl_from(false))
	)).done([=](const TLoptions_ConfigInfo &result) {
		_lib.resendingOnError(
		) | rpl::start_with_next([=] {
			if (++_failedRequestsSinceSetConfig >= kErrorsTillSetConfig) {
				_failedRequestsSinceSetConfig = 0;
				resetNetwork();
			}
		}, _lifetime);

		InvokeCallback(done, WalletId(result));
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

} // namespace Ton::details
