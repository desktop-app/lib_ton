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
#include "ton/ton_config.h"
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
: _external(std::make_unique<External>(path))
, _accountViewers(
	std::make_unique<AccountViewers>(
		this,
		&_external->lib(),
		&_external->db())) {
	crl::async([] {
		// Init random, because it is slow.
		static_cast<void>(openssl::RandomValue<uint8>());
	});
}

Wallet::~Wallet() = default;

QString Wallet::GetAddress(const QByteArray &publicKey) {
	return RequestSender::Execute(TLwallet_GetAccountAddress(
		tl_wallet_initialAccountState(tl_string(publicKey))
	)).value_or(
		tl_accountAddress(tl_string())
	).match([&](const TLDaccountAddress &data) {
		return tl::utf16(data.vaccount_address());
	});
}

void Wallet::open(
		const QByteArray &globalPassword,
		const Config &config,
		Callback<> done) {
	_external->open(globalPassword, config, [=](Result<WalletList> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		setWalletList(*result);
		InvokeCallback(done);
	});
}

void Wallet::setConfig(const Config &config, Callback<> done) {
	_external->setConfig(config, std::move(done));
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

void Wallet::saveKey(
		const QByteArray &password,
		Callback<QByteArray> done) {
	Expects(_keyCreator != nullptr);

	auto saved = [=](Result<details::WalletList::Entry> result) {
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

details::WalletList Wallet::collectWalletList() const {
	Expects(_publicKeys.size() == _secrets.size());

	auto result = details::WalletList();
	for (auto i = 0, count = int(_secrets.size()); i != count; ++i) {
		result.entries.push_back({ _publicKeys[i], _secrets[i] });
	}
	return result;
}

void Wallet::setWalletList(const details::WalletList &list) {
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

void Wallet::sendGrams(
		const QByteArray &publicKey,
		const QByteArray &password,
		const TransactionToSend &transaction,
		Callback<PendingTransaction> done) {
	Expects(transaction.amount > 0);

	const auto sender = GetAddress(publicKey);
	Assert(!sender.isEmpty());

	const auto index = ranges::find(_publicKeys, publicKey)
		- begin(_publicKeys);
	Assert(index < _secrets.size());

	_external->lib().request(TLgeneric_SendGrams(
		tl_inputKey(
			tl_key(tl_string(publicKey), TLsecureBytes{ _secrets[index] }),
			TLsecureBytes{ password }),
		tl_accountAddress(tl_string(sender)),
		tl_accountAddress(tl_string(transaction.recipient)),
		tl_int64(transaction.amount),
		tl_int32(transaction.timeout),
		tl_from(transaction.allowSendToUninited),
		tl_string(transaction.comment)
	)).done([=](const TLSendGramsResult &result) {
		InvokeCallback(done, Parse(result, sender, transaction));
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
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
		tl_internal_transactionId(tl_int64(lastId.id), tl_bytes(lastId.hash))
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

} // namespace Ton
