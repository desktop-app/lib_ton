// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_key_destroyer.h"

#include "ton/details/ton_request_sender.h"
#include "ton/details/ton_external.h"
#include "ton/details/ton_storage.h"

namespace Ton::details {

KeyDestroyer::KeyDestroyer(
		not_null<details::RequestSender*> lib,
		not_null<Storage::Cache::Database*> db,
		const details::WalletList &existing,
		index_type index,
		Callback<> done) {
	Expects(index >= 0 && index < existing.entries.size());

	const auto &entry = existing.entries[index];
	auto removeFromDatabase = crl::guard(this, [=](Result<>) {
		auto copy = existing;
		copy.entries.erase(begin(copy.entries) + index);
		SaveWalletList(db, copy, crl::guard(this, done));
	});
	DeletePublicKey(
		lib,
		entry.publicKey,
		entry.secret,
		std::move(removeFromDatabase));
}

KeyDestroyer::KeyDestroyer(
		not_null<RequestSender*> lib,
		not_null<Storage::Cache::Database*> db,
		Callback<> done) {
	const auto removeFromDatabase = crl::guard(this, [=](const auto&) {
		SaveWalletList(db, {}, crl::guard(this, done));
	});
	lib->request(TLDeleteAllKeys(
	)).done(
		removeFromDatabase
	).fail(
		removeFromDatabase
	).send();
}

} // namespace Ton::details
