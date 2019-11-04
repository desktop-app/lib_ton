// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/ton_wallet.h"

#include "ton/details/ton_account_viewers.h"
#include "ton/details/ton_request_sender.h"
#include "ton/details/ton_key_creator.h"
#include "ton/details/ton_key_destroyer.h"
#include "ton/details/ton_password_changer.h"
#include "ton/details/ton_external.h"
#include "ton/details/ton_parse_state.h"
#include "ton/details/ton_web_loader.h"
#include "ton/ton_settings.h"
#include "ton/ton_state.h"
#include "ton/ton_account_viewer.h"
#include "storage/cache/storage_cache_database.h"
#include "storage/storage_encryption.h"
#include "base/openssl_help.h"

#include <crl/crl_async.h>
#include <crl/crl_on_main.h>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace Ton {
namespace {

using namespace details;

} // namespace

Wallet::Wallet(const QString &path)
: _external(std::make_unique<External>(path, generateUpdatesCallback()))
, _accountViewers(
	std::make_unique<AccountViewers>(
		this,
		&_external->lib(),
		&_external->db())) {
	crl::async([] {
		// Init random, because it is slow.
		static_cast<void>(openssl::RandomValue<uint8>());
	});
	_accountViewers->blockchainTime(
	) | rpl::start_with_next([=](BlockchainTime time) {
		checkLocalTime(time);
	}, _lifetime);
}

Wallet::~Wallet() = default;

void Wallet::EnableLogging(bool enabled, const QString &basePath) {
	External::EnableLogging(enabled, basePath);
}

void Wallet::LogMessage(const QString &message) {
	return External::LogMessage(message);
}

bool Wallet::CheckAddress(const QString &address) {
	return RequestSender::Execute(TLUnpackAccountAddress(
		tl_string(address)
	)) ? true : false;
}

base::flat_set<QString> Wallet::GetValidWords() {
	const auto result = RequestSender::Execute(TLGetBip39Hints(
		tl_string()));
	Assert(result);

	return result->match([&](const TLDbip39Hints &data) {
		auto &&words = ranges::view::all(
			data.vwords().v
		) | ranges::view::transform([](const TLstring &word) {
			return QString::fromUtf8(word.v);
		});
		return base::flat_set<QString>{ words.begin(), words.end() };
	});
}

Result<> Wallet::CheckConfig(const QByteArray &config) {
	const auto walletId = External::WalletId(config);
	return walletId ? Result<>() : walletId.error();
}

void Wallet::open(
		const QByteArray &globalPassword,
		const Settings &defaultSettings,
		Callback<> done) {
	auto opened = [=](Result<WalletList> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		setWalletList(*result);
		_external->lib().request(TLSync()).send();
		InvokeCallback(done);
	};
	_external->open(globalPassword, defaultSettings, std::move(opened));
}

void Wallet::start(Callback<> done) {
	_external->start([=](Result<int64> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		_walletId = *result;
		InvokeCallback(done);
	});
}

QString Wallet::getAddress(const QByteArray &publicKey) const {
	Expects(_walletId.has_value());

	return RequestSender::Execute(TLwallet_v3_GetAccountAddress(
		tl_wallet_v3_initialAccountState(
			tl_string(publicKey),
			tl_int53(*_walletId))
	)).value_or(
		tl_accountAddress(tl_string())
	).match([&](const TLDaccountAddress &data) {
		return tl::utf16(data.vaccount_address());
	});
}

const Settings &Wallet::settings() const {
	return _external->settings();
}

void Wallet::updateSettings(const Settings &settings, Callback<> done) {
	const auto name = _external->settings().blockchainName;
	const auto detach = (name != settings.blockchainName);
	_external->updateSettings(settings, [=](Result<int64> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		Expects(!_walletId || *_walletId == *result || detach);
		_walletId = *result;
		InvokeCallback(done);
	});
}

rpl::producer<Update> Wallet::updates() const {
	return _updates.events();
}

const std::vector<QByteArray> &Wallet::publicKeys() const {
	return _publicKeys;
}

void Wallet::createKey(Callback<std::vector<QString>> done) {
	Expects(_keyCreator == nullptr);
	Expects(_keyDestroyer == nullptr);
	Expects(_passwordChanger == nullptr);

	auto created = [=](Result<std::vector<QString>> result) {
		const auto destroyed = result
			? std::unique_ptr<KeyCreator>()
			: base::take(_keyCreator);
		InvokeCallback(done, result);
	};
	_keyCreator = std::make_unique<KeyCreator>(
		&_external->lib(),
		&_external->db(),
		std::move(created));
}

void Wallet::importKey(const std::vector<QString> &words, Callback<> done) {
	Expects(_keyCreator == nullptr);
	Expects(_keyDestroyer == nullptr);
	Expects(_passwordChanger == nullptr);

	auto created = [=](Result<> result) {
		const auto destroyed = result
			? std::unique_ptr<KeyCreator>()
			: base::take(_keyCreator);
		InvokeCallback(done, result);
	};
	_keyCreator = std::make_unique<KeyCreator>(
		&_external->lib(),
		&_external->db(),
		words,
		std::move(created));
}

void Wallet::saveKey(
		const QByteArray &password,
		Callback<QByteArray> done) {
	Expects(_keyCreator != nullptr);

	auto saved = [=](Result<WalletList::Entry> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		const auto destroyed = base::take(_keyCreator);
		_publicKeys.push_back(result->publicKey);
		_secrets.push_back(result->secret);
		InvokeCallback(done, result->publicKey);
	};
	_keyCreator->save(
		password,
		collectWalletList(),
		std::move(saved));
}

void Wallet::exportKey(
		const QByteArray &publicKey,
		const QByteArray &password,
		Callback<std::vector<QString>> done) {
	_external->lib().request(TLExportKey(
		prepareInputKey(publicKey, password)
	)).done([=](const TLExportedKey &result) {
		InvokeCallback(done, Parse(result));
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

WalletList Wallet::collectWalletList() const {
	Expects(_publicKeys.size() == _secrets.size());

	auto result = WalletList();
	for (auto i = 0, count = int(_secrets.size()); i != count; ++i) {
		result.entries.push_back({ _publicKeys[i], _secrets[i] });
	}
	return result;
}

TLinputKey Wallet::prepareInputKey(
		const QByteArray &publicKey,
		const QByteArray &password) const {
	const auto index = ranges::find(_publicKeys, publicKey)
		- begin(_publicKeys);
	Assert(index < _secrets.size());

	return tl_inputKeyRegular(
		tl_key(tl_string(publicKey), TLsecureBytes{ _secrets[index] }),
		TLsecureBytes{ password });
}

void Wallet::setWalletList(const WalletList &list) {
	Expects(_publicKeys.empty());
	Expects(_secrets.empty());

	if (list.entries.empty()) {
		return;
	}
	_publicKeys.reserve(list.entries.size());
	_secrets.reserve(list.entries.size());
	for (const auto &[publicKey, secret] : list.entries) {
		_publicKeys.push_back(publicKey);
		_secrets.push_back(secret);
	}
}

void Wallet::deleteKey(
		const QByteArray &publicKey,
		Callback<> done) {
	Expects(_keyCreator == nullptr);
	Expects(_keyDestroyer == nullptr);
	Expects(_passwordChanger == nullptr);
	Expects(ranges::find(_publicKeys, publicKey) != end(_publicKeys));

	auto list = collectWalletList();
	const auto index = ranges::find(
		list.entries,
		publicKey,
		&WalletList::Entry::publicKey
	) - begin(list.entries);

	auto removed = [=](Result<> result) {
		const auto destroyed = base::take(_keyDestroyer);
		if (!result) {
			InvokeCallback(done, result);
			return;
		}
		_publicKeys.erase(begin(_publicKeys) + index);
		_secrets.erase(begin(_secrets) + index);
		InvokeCallback(done, result);
	};
	_keyDestroyer = std::make_unique<KeyDestroyer>(
		&_external->lib(),
		&_external->db(),
		std::move(list),
		index,
		std::move(removed));
}

void Wallet::deleteAllKeys(Callback<> done) {
	Expects(_keyCreator == nullptr);
	Expects(_keyDestroyer == nullptr);
	Expects(_passwordChanger == nullptr);

	auto removed = [=](Result<> result) {
		const auto destroyed = base::take(_keyDestroyer);
		if (!result) {
			InvokeCallback(done, result);
			return;
		}
		_publicKeys.clear();
		_secrets.clear();
		InvokeCallback(done, result);
	};
	_keyDestroyer = std::make_unique<KeyDestroyer>(
		&_external->lib(),
		&_external->db(),
		std::move(removed));
}

void Wallet::changePassword(
		const QByteArray &oldPassword,
		const QByteArray &newPassword,
		Callback<> done) {
	Expects(_keyCreator == nullptr);
	Expects(_keyDestroyer == nullptr);
	Expects(_passwordChanger == nullptr);
	Expects(!_publicKeys.empty());

	auto changed = [=](Result<std::vector<QByteArray>> result) {
		const auto destroyed = base::take(_passwordChanger);
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		_secrets = std::move(*result);
		InvokeCallback(done);
	};
	_passwordChanger = std::make_unique<PasswordChanger>(
		&_external->lib(),
		&_external->db(),
		oldPassword,
		newPassword,
		collectWalletList(),
		std::move(changed));
}

void Wallet::checkSendGrams(
		const QByteArray &publicKey,
		const TransactionToSend &transaction,
		Callback<TransactionCheckResult> done) {
	Expects(transaction.amount >= 0);

	const auto sender = getAddress(publicKey);
	Assert(!sender.isEmpty());

	const auto index = ranges::find(_publicKeys, publicKey)
		- begin(_publicKeys);
	Assert(index < _secrets.size());

	const auto check = [=](int64 id) {
		_external->lib().request(TLquery_EstimateFees(
			tl_int53(id),
			tl_boolTrue()
		)).done([=](const TLquery_Fees &result) {
			_external->lib().request(TLquery_Forget(
				tl_int53(id)
			)).send();
			InvokeCallback(done, Parse(result));
		}).fail([=](const TLError &error) {
			InvokeCallback(done, ErrorFromLib(error));
		}).send();
	};
	_external->lib().request(TLgeneric_CreateSendGramsQuery(
		tl_inputKeyFake(),
		tl_accountAddress(tl_string(sender)),
		tl_accountAddress(tl_string(transaction.recipient)),
		tl_int64(transaction.amount),
		tl_int32(transaction.timeout),
		tl_from(transaction.allowSendToUninited),
		tl_string(transaction.comment)
	)).done([=](const TLquery_Info &result) {
		result.match([&](const TLDquery_info &data) {
			check(data.vid().v);
		});
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void Wallet::sendGrams(
		const QByteArray &publicKey,
		const QByteArray &password,
		const TransactionToSend &transaction,
		Callback<PendingTransaction> ready,
		Callback<> done) {
	Expects(transaction.amount >= 0);

	const auto sender = getAddress(publicKey);
	Assert(!sender.isEmpty());

	const auto send = [=](int64 id) {
		_external->lib().request(TLquery_Send(
			tl_int53(id)
		)).done([=] {
			InvokeCallback(done);
		}).fail([=](const TLError &error) {
			InvokeCallback(done, ErrorFromLib(error));
		}).send();
	};

	_external->lib().request(TLgeneric_CreateSendGramsQuery(
		prepareInputKey(publicKey, password),
		tl_accountAddress(tl_string(sender)),
		tl_accountAddress(tl_string(transaction.recipient)),
		tl_int64(transaction.amount),
		tl_int32(transaction.timeout),
		tl_from(transaction.allowSendToUninited),
		tl_string(transaction.comment)
	)).done([=](const TLquery_Info &result) {
		result.match([&](const TLDquery_info &data) {
			const auto weak = base::make_weak(this);
			auto pending = Parse(result, sender, transaction);
			_accountViewers->addPendingTransaction(pending);
			if (!weak) {
				return;
			}
			InvokeCallback(ready, std::move(pending));
			if (!weak) {
				return;
			}
			send(data.vid().v);
		});
	}).fail([=](const TLError &error) {
		InvokeCallback(ready, ErrorFromLib(error));
	}).send();
}

void Wallet::requestState(
		const QString &address,
		Callback<AccountState> done) {
	_external->lib().request(TLgeneric_GetAccountState(
		tl_accountAddress(tl_string(address))
	)).done([=](const TLgeneric_AccountState &result) {
		const auto finish = [&](auto &&value) {
			InvokeCallback(done, std::move(value));
		};
		result.match([&](const TLDgeneric_accountStateTestGiver &data) {
			finish(Error{ Error::Type::TonLib, "BAD_ADDRESS_TEST_GIVER" });
		}, [&](const TLDgeneric_accountStateTestWallet &data) {
			finish(Error{ Error::Type::TonLib, "BAD_ADDRESS_TEST_WALLET" });
		}, [&](const TLDgeneric_accountStateRaw &data) {
			finish(Error{ Error::Type::TonLib, "BAD_ADDRESS_RAW" });
		}, [&](const TLDgeneric_accountStateWallet &data) {
			finish(Error{ Error::Type::TonLib, "BAD_ADDRESS_SIMPLE" });
		}, [&](const auto &data) {
			finish(Parse(data.vaccount_state()));
		});
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void Wallet::requestTransactions(
		const QString &address,
		const TransactionId &lastId,
		Callback<TransactionsSlice> done) {
	_external->lib().request(TLraw_GetTransactions(
		tl_accountAddress(tl_string(address)),
		tl_internal_transactionId(tl_int64(lastId.lt), tl_bytes(lastId.hash))
	)).done([=](const TLraw_Transactions &result) {
		InvokeCallback(done, Parse(result));
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

std::unique_ptr<AccountViewer> Wallet::createAccountViewer(
		const QString &address) {
	return _accountViewers->createAccountViewer(address);
}

void Wallet::loadWebResource(const QString &url, Callback<QByteArray> done) {
	if (!_webLoader) {
		_webLoader = std::make_unique<WebLoader>([=] {
			_webLoader = nullptr;
		});
	}
	_webLoader->load(url, std::move(done));
}

Fn<void(Update)> Wallet::generateUpdatesCallback() {
	return [=](Update update) {
		if (const auto sync = base::get_if<SyncState>(&update.data)) {
			if (*sync == _lastSyncStateUpdate) {
				return;
			}
			_lastSyncStateUpdate = *sync;
		}
		_updates.fire(std::move(update));
	};
}

void Wallet::checkLocalTime(BlockchainTime time) {
	if (_localTimeSyncer) {
		_localTimeSyncer->updateBlockchainTime(time);
		return;
	} else if (LocalTimeSyncer::IsLocalTimeBad(time)) {
		_localTimeSyncer = std::make_unique<LocalTimeSyncer>(
			time,
			&_external->lib(),
			[=] { _localTimeSyncer = nullptr; });
	}
}

} // namespace Ton
