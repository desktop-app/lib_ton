// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_storage.h"

#include "ton/details/ton_request_sender.h"
#include "storage/cache/storage_cache_database.h"
#include "ton_storage_tl.h"

namespace Ton::details {
namespace {

constexpr auto kWalletListKey = Storage::Cache::Key{ 1ULL, 1ULL };

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

void DeletePublicKey(
		not_null<RequestSender*> lib,
		const QByteArray &publicKey,
		const QByteArray &secret,
		Callback<> done) {
	lib->request(TLDeleteKey(
		tl_key(tl_string(publicKey), TLsecureBytes{ secret })
	)).done([=] {
		InvokeCallback(done);
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void SaveWalletList(
		not_null<Storage::Cache::Database*> db,
		const WalletList &list,
		Callback<> done) {
	auto saved = [=](Storage::Cache::Error error) {
		crl::on_main([=] {
			if (const auto bad = ErrorFromStorage(error)) {
				InvokeCallback(done, *bad);
			} else {
				InvokeCallback(done);
			}
		});
	};
	if (list.entries.empty()) {
		db->remove(kWalletListKey, std::move(saved));
	} else {
		db->put(kWalletListKey, Pack(list), std::move(saved));
	}
}

void LoadWalletList(
		not_null<Storage::Cache::Database*> db,
		Fn<void(WalletList&&)> done) {
	Expects(done != nullptr);

	db->get(kWalletListKey, [=](QByteArray value) {
		crl::on_main([=] {
			done(Unpack(value));
		});
	});
}

QByteArray Pack(const WalletList &data) {
	auto entries = QVector<TLstorage_WalletEntry>();
	entries.reserve(data.entries.size());
	for (const auto &entry : data.entries) {
		entries.push_back(make_storage_walletEntry(
			tl_string(entry.publicKey),
			tl_bytes(entry.secret)));
	}
	const auto packed = TLstorage_WalletList(make_storage_walletList(
		tl_vector<TLstorage_WalletEntry>(std::move(entries))));

	auto result = QByteArray();
	result.reserve(tl::count_length(packed));
	packed.write(result);
	return result;
}

WalletList Unpack(const QByteArray &data) {
	auto list = TLstorage_WalletList();
	auto from = data.data();
	const auto till = from + data.size();
	if (!list.read(from, till)) {
		return {};
	}

	auto result = WalletList();
	list.match([&](const TLDstorage_walletList &data) {
		result.entries.reserve(data.ventries().v.size());
		for (const auto &entry : data.ventries().v) {
			entry.match([&](const TLDstorage_walletEntry &data) {
				result.entries.push_back({
					data.vpublicKey().v,
					data.vsecret().v });
			});
		}
	});
	return result;
}

} // namespace Ton::details
