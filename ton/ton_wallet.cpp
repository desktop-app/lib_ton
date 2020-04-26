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

constexpr auto kViewersPasswordExpires = 15 * 60 * crl::time(1000);

[[nodiscard]] TLError GenerateFakeIncorrectPasswordError() {
	return tl_error(tl_int32(0), tl_string("KEY_DECRYPT"));
}

[[nodiscard]] int SmcRevision(const TLinitialAccountState &state) {
	return state.match([](const TLDwallet_v3_initialAccountState &) {
		return 2;
	}, [](const TLDrwallet_initialAccountState &) {
		return 1;
	}, [](const auto &) -> int {
		Unexpected("Unknown initial account state.");
	});
}

} // namespace

Wallet::Wallet(const QString &path)
: _external(std::make_unique<External>(path, generateUpdatesCallback()))
, _accountViewers(
	std::make_unique<AccountViewers>(
		this,
		&_external->lib(),
		&_external->db()))
, _list(std::make_unique<WalletList>())
, _viewersPasswordsExpireTimer([=] { checkPasswordsExpiration(); }) {
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

bool Wallet::IsIncorrectPasswordError(const Error &error) {
	return error.details.startsWith(qstr("KEY_DECRYPT"));
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
		if (_switchedToMain) {
			auto copy = settings();
			copy.useTestNetwork = false;
			updateSettings(copy, std::move(done));
		} else {
			_external->lib().request(TLSync()).send();
			InvokeCallback(done);
		}
	};
	_external->open(globalPassword, defaultSettings, std::move(opened));
}

void Wallet::start(Callback<> done) {
	_external->start([=](Result<ConfigInfo> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		_configInfo = *result;
		InvokeCallback(done);
	});
}

QString Wallet::getUsedAddress(const QByteArray &publicKey) const {
	return getUsedAddress(getUsedInitialAccountState(publicKey));
}

TLinitialAccountState Wallet::getUsedInitialAccountState(
		const QByteArray &publicKey) const {
	Expects(_configInfo.has_value());

	const auto i = ranges::find(
		_list->entries,
		publicKey,
		&WalletList::Entry::publicKey);
	Assert(i != end(_list->entries));
	return i->restrictedInitPublicKey.isEmpty()
		? tl_wallet_v3_initialAccountState(
			tl_string(publicKey),
			tl_int64(_configInfo->walletId))
		: tl_rwallet_initialAccountState(
			tl_string(i->restrictedInitPublicKey),
			tl_string(publicKey),
			tl_int64(_configInfo->walletId));
}

QString Wallet::getUsedAddress(const TLinitialAccountState &state) const {
	return RequestSender::Execute(TLGetAccountAddress(
		state,
		tl_int32(SmcRevision(state))
	)).value_or(
		tl_accountAddress(tl_string())
	).match([&](const TLDaccountAddress &data) {
		return tl::utf16(data.vaccount_address());
	});
}

const Settings &Wallet::settings() const {
	return _external->settings();
}

void Wallet::updateSettings(Settings settings, Callback<> done) {
	const auto &was = _external->settings();
	const auto detach = (was.net().blockchainName
		!= settings.net().blockchainName);
	const auto change = (was.useTestNetwork != settings.useTestNetwork);
	const auto finish = [=](Result<ConfigInfo> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		Expects(!_configInfo
			|| (_configInfo->walletId == result->walletId)
			|| detach
			|| change);
		_configInfo = *result;
		InvokeCallback(done);
	};
	if (!change) {
		_external->updateSettings(settings, finish);
		return;
	}
	// First just save the new settings.
	settings.useTestNetwork = was.useTestNetwork;
	_external->updateSettings(settings, [=](Result<ConfigInfo> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		// Then logout and switch the network.
		deleteAllKeys([=](Result<> result) {
			if (!result) {
				InvokeCallback(done, result.error());
				return;
			}
			_external->switchNetwork(finish);
		});
	});
}

void Wallet::checkConfig(const QByteArray &config, Callback<> done) {
	// We want to check only validity of config,
	// not validity in one specific blockchain_name.
	// So we pass an empty blockchain name.
	_external->lib().request(TLoptions_ValidateConfig(
		tl_config(
			tl_string(config),
			tl_string(QString()),
			tl_from(false),
			tl_from(false))
	)).done([=] {
		InvokeCallback(done);
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void Wallet::sync() {
	_external->lib().request(TLSync()).send();
}

rpl::producer<Update> Wallet::updates() const {
	return _updates.events();
}

std::vector<QByteArray> Wallet::publicKeys() const {
	return _list->entries | ranges::view::transform(
		&WalletList::Entry::publicKey
	) | ranges::to_vector;
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

void Wallet::queryRestrictedInitPublicKey(Callback<QByteArray> done) {
	Expects(_keyCreator != nullptr);
	Expects(_configInfo.has_value());

	_keyCreator->queryRestrictedInitPublicKey(
		getUsedAddress(
			tl_rwallet_initialAccountState(
				tl_string(_configInfo->restrictedInitPublicKey),
				tl_string(_keyCreator->key()),
				tl_int64(_configInfo->walletId))),
		_configInfo->restrictedInitPublicKey,
		std::move(done));
}

void Wallet::saveKey(
		const QByteArray &password,
		const QByteArray &restrictedInitPublicKey,
		Callback<QByteArray> done) {
	Expects(_keyCreator != nullptr);

	auto saved = [=](Result<WalletList::Entry> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		const auto destroyed = base::take(_keyCreator);
		_list->entries.push_back(*result);
		InvokeCallback(done, result->publicKey);
	};
	_keyCreator->save(
		password,
		*_list,
		restrictedInitPublicKey,
		settings().useTestNetwork,
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

TLinputKey Wallet::prepareInputKey(
		const QByteArray &publicKey,
		const QByteArray &password) const {
	const auto i = ranges::find(
		_list->entries,
		publicKey,
		&WalletList::Entry::publicKey);
	Assert(i != end(_list->entries));

	return tl_inputKeyRegular(
		tl_key(tl_string(publicKey), TLsecureBytes{ i->secret }),
		TLsecureBytes{ password });
}

void Wallet::setWalletList(const WalletList &list) {
	Expects(_list->entries.empty());

	*_list = list;
}

void Wallet::deleteKey(
		const QByteArray &publicKey,
		Callback<> done) {
	Expects(_keyCreator == nullptr);
	Expects(_keyDestroyer == nullptr);
	Expects(_passwordChanger == nullptr);
	Expects(ranges::contains(
		_list->entries,
		publicKey,
		&WalletList::Entry::publicKey));

	auto list = *_list;
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
		_list->entries.erase(begin(_list->entries) + index);
		_viewersPasswords.erase(publicKey);
		_viewersPasswordsWaiters.erase(publicKey);
		InvokeCallback(done, result);
	};
	_keyDestroyer = std::make_unique<KeyDestroyer>(
		&_external->lib(),
		&_external->db(),
		std::move(list),
		index,
		settings().useTestNetwork,
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
		_list->entries.clear();
		_viewersPasswords.clear();
		_viewersPasswordsWaiters.clear();
		InvokeCallback(done, result);
	};
	_keyDestroyer = std::make_unique<KeyDestroyer>(
		&_external->lib(),
		&_external->db(),
		settings().useTestNetwork,
		std::move(removed));
}

void Wallet::changePassword(
		const QByteArray &oldPassword,
		const QByteArray &newPassword,
		Callback<> done) {
	Expects(_keyCreator == nullptr);
	Expects(_keyDestroyer == nullptr);
	Expects(_passwordChanger == nullptr);
	Expects(!_list->entries.empty());

	auto changed = [=](Result<std::vector<QByteArray>> result) {
		const auto destroyed = base::take(_passwordChanger);
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		Assert(result->size() == _list->entries.size());
		for (auto i = 0, count = int(result->size()); i != count; ++i) {
			_list->entries[i].secret = (*result)[i];
		}
		for (auto &[publicKey, password] : _viewersPasswords) {
			updateViewersPassword(publicKey, newPassword);
		}
		InvokeCallback(done);
	};
	_passwordChanger = std::make_unique<PasswordChanger>(
		&_external->lib(),
		&_external->db(),
		oldPassword,
		newPassword,
		*_list,
		settings().useTestNetwork,
		std::move(changed));
}

void Wallet::checkSendGrams(
		const QByteArray &publicKey,
		const TransactionToSend &transaction,
		Callback<TransactionCheckResult> done) {
	Expects(transaction.amount >= 0);

	const auto initial = getUsedInitialAccountState(publicKey);
	const auto sender = getUsedAddress(initial);
	Assert(!sender.isEmpty());

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
	_external->lib().request(TLCreateQuery(
		tl_inputKeyFake(),
		tl_accountAddress(tl_string(sender)),
		tl_int32(transaction.timeout),
		tl_actionMsg(
			tl_vector(1, tl_msg_message(
				tl_accountAddress(tl_string(transaction.recipient)),
				tl_string(),
				tl_int64(transaction.amount),
				(transaction.sendUnencryptedText
					? tl_msg_dataText
					: tl_msg_dataDecryptedText)(
						tl_string(transaction.comment)))),
			tl_from(transaction.allowSendToUninited)),
		initial
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

	const auto initial = getUsedInitialAccountState(publicKey);
	const auto sender = getUsedAddress(initial);
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

	_external->lib().request(TLCreateQuery(
		prepareInputKey(publicKey, password),
		tl_accountAddress(tl_string(sender)),
		tl_int32(transaction.timeout),
		tl_actionMsg(
			tl_vector(1, tl_msg_message(
				tl_accountAddress(tl_string(transaction.recipient)),
				tl_string(),
				tl_int64(transaction.amount),
				(transaction.sendUnencryptedText
					? tl_msg_dataText
					: tl_msg_dataDecryptedText)(
						tl_string(transaction.comment)))),
			tl_from(transaction.allowSendToUninited)),
		initial
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
	_external->lib().request(TLGetAccountState(
		tl_accountAddress(tl_string(address))
	)).done([=](const TLFullAccountState &result) {
		InvokeCallback(done, Parse(result));
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void Wallet::requestTransactions(
		const QByteArray &publicKey,
		const QString &address,
		const TransactionId &lastId,
		Callback<TransactionsSlice> done) {
	_external->lib().request(TLraw_GetTransactions(
		tl_inputKeyFake(),
		tl_accountAddress(tl_string(address)),
		tl_internal_transactionId(tl_int64(lastId.lt), tl_bytes(lastId.hash))
	)).done([=](const TLraw_Transactions &result) {
		InvokeCallback(done, Parse(result));
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void Wallet::trySilentDecrypt(
		const QByteArray &publicKey,
		std::vector<Transaction> &&list,
		Callback<std::vector<Transaction>> done) {
	const auto encrypted = CollectEncryptedTexts(list);
	if (encrypted.empty() || !_viewersPasswords.contains(publicKey)) {
		InvokeCallback(done, std::move(list));
		return;
	}
	const auto shared = std::make_shared<std::vector<Transaction>>(
		std::move(list));
	const auto password = _viewersPasswords[publicKey];
	const auto generation = password.generation;
	_external->lib().request(TLmsg_Decrypt(
		prepareInputKey(publicKey, password.bytes),
		MsgDataArrayFromEncrypted(encrypted)
	)).done([=](const TLmsg_DataDecryptedArray &result) {
		InvokeCallback(
			done,
			AddDecryptedTexts(
				std::move(*shared),
				encrypted,
				MsgDataArrayToDecrypted(result)));
	}).fail([=](const TLError &error) {
		InvokeCallback(done, std::move(*shared));
	}).send();
}

void Wallet::decrypt(
		const QByteArray &publicKey,
		std::vector<Transaction> &&list,
		Callback<std::vector<Transaction>> done) {
	const auto encrypted = CollectEncryptedTexts(list);
	if (encrypted.empty()) {
		InvokeCallback(done, std::move(list));
		return;
	}
	const auto shared = std::make_shared<std::vector<Transaction>>(
		std::move(list));
	const auto password = _viewersPasswords[publicKey];
	const auto generation = password.generation;
	const auto fail = [=](const TLError &error) {
		handleInputKeyError(publicKey, generation, error, [=](
				Result<> result) {
			if (result) {
				decrypt(publicKey, std::move(*shared), done);
			} else {
				InvokeCallback(done, result.error());
			}
		});
	};
	if (password.bytes.isEmpty()) {
		fail(GenerateFakeIncorrectPasswordError());
		return;
	}
	_external->lib().request(TLmsg_Decrypt(
		prepareInputKey(publicKey, password.bytes),
		MsgDataArrayFromEncrypted(encrypted)
	)).done([=](const TLmsg_DataDecryptedArray &result) {
		notifyPasswordGood(publicKey, generation);
		InvokeCallback(
			done,
			AddDecryptedTexts(
				std::move(*shared),
				encrypted,
				MsgDataArrayToDecrypted(result)));
	}).fail(fail).send();
}

void Wallet::handleInputKeyError(
		const QByteArray &publicKey,
		int generation,
		const TLerror &error,
		Callback<> done) {
	const auto parsed = ErrorFromLib(error);
	if (IsIncorrectPasswordError(parsed)
		&& ranges::contains(
			_list->entries,
			publicKey,
			&WalletList::Entry::publicKey)) {
		if (_viewersPasswords.contains(publicKey)
			&& _viewersPasswords[publicKey].generation == generation) {
			_viewersPasswords[publicKey].expires = 0;
			_viewersPasswordsWaiters[publicKey].emplace_back(done);
			_updates.fire({ DecryptPasswordNeeded{
				publicKey,
				generation
			} });
		} else {
			InvokeCallback(done);
		}
	} else {
		notifyPasswordGood(publicKey, generation);
		InvokeCallback(done, parsed);
	}
}

void Wallet::notifyPasswordGood(
		const QByteArray &publicKey,
		int generation) {
	if (_viewersPasswords.contains(publicKey)
		&& !_viewersPasswords[publicKey].expires) {
		const auto expires = crl::now() + kViewersPasswordExpires;
		_viewersPasswords[publicKey].expires = expires;
		if (!_viewersPasswordsExpireTimer.isActive()) {
			_viewersPasswordsExpireTimer.callOnce(
				kViewersPasswordExpires);
		}
	}
	_updates.fire({ DecryptPasswordGood{ generation } });
}

std::unique_ptr<AccountViewer> Wallet::createAccountViewer(
		const QByteArray &publicKey,
		const QString &address) {
	return _accountViewers->createAccountViewer(publicKey, address);
}

void Wallet::updateViewersPassword(
		const QByteArray &publicKey,
		const QByteArray &password) {
	if (password.isEmpty()) {
		_viewersPasswords.remove(publicKey);
		_viewersPasswordsWaiters.remove(publicKey);
		return;
	}
	auto &data = _viewersPasswords[publicKey];
	data.bytes = password;
	++data.generation;
	if (const auto list = _viewersPasswordsWaiters.take(publicKey)) {
		for (const auto &callback : *list) {
			InvokeCallback(callback);
		}
	}
}

void Wallet::checkPasswordsExpiration() {
	const auto now = crl::now();
	auto next = crl::time(0);
	for (auto i = _viewersPasswords.begin(); i != _viewersPasswords.end();) {
		const auto expires = i->second.expires;
		if (!expires) {
			++i;
		} else if (expires <= now) {
			_viewersPasswordsWaiters.remove(i->first);
			i = _viewersPasswords.erase(i);
		} else {
			if (!next || next > expires) {
				next = expires;
			}
			++i;
		}
	}
	if (next) {
		_viewersPasswordsExpireTimer.callOnce(next - now);
	}
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
		} else if (const auto upgrade = base::get_if<ConfigUpgrade>(
				&update.data)) {
			if (*upgrade == ConfigUpgrade::TestnetToMainnet) {
				_switchedToMain = true;
			}
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
