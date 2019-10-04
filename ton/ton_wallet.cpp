// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/ton_wallet.h"

#include "ton/details/ton_request_sender.h"
#include "ton/details/ton_key_creator.h"
#include "ton/details/ton_key_destroyer.h"
#include "ton/details/ton_external.h"
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
: _external(std::make_unique<External>(path)) {
	crl::async([] {
		// Init random, because it is slow.
		static_cast<void>(openssl::RandomValue<uint8>());
	});
}

Wallet::~Wallet() = default;

void Wallet::open(
		const QByteArray &globalPassword,
		Callback<> done) {
	_external->open(globalPassword, [=](Result<WalletList> result) {
		if (!result) {
			InvokeCallback(done, result.error());
			return;
		}
		setWalletList(*result);
		InvokeCallback(done);
	});
}

const std::vector<QByteArray> &Wallet::publicKeys() const {
	return _publicKeys;
}

void Wallet::createKey(Fn<void(Result<std::vector<QString>>)> done) {
	Expects(_keyCreator == nullptr);
	Expects(_keyDestroyer == nullptr);

	auto created = [=](Result<std::vector<QString>> result) {
		const auto destroyed = result
			? std::unique_ptr<KeyCreator>()
			: base::take(_keyCreator);
		InvokeCallback(done, result);
	};
	_keyCreator = std::make_unique<KeyCreator>(
		&_external->lib(),
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
		_external->saveListMethod(),
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
	Expects(ranges::find(_publicKeys, publicKey) != end(_publicKeys));

	auto list = collectWalletList();
	const auto index = ranges::find(
		list.entries,
		publicKey,
		&WalletList::Entry::publicKey
	) - begin(list.entries);

	auto removed = [=](Result<> result) {
		if (!result) {
			InvokeCallback(done, result);
			return;
		}
		const auto destroyed = base::take(_keyDestroyer);
		_publicKeys.erase(begin(_publicKeys) + index);
		_secrets.erase(begin(_secrets) + index);
		InvokeCallback(done, result);
	};
	_keyDestroyer = std::make_unique<KeyDestroyer>(
		&_external->lib(),
		std::move(list),
		index,
		_external->saveListMethod(),
		std::move(removed));
}

} // namespace Ton
