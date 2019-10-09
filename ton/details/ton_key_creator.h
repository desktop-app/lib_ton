// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/details/ton_external.h"
#include "ton/details/ton_storage.h"

namespace Storage {
namespace Cache {
class Database;
} // namespace Cache
} // namespace Storage

namespace Ton::details {

class RequestSender;

class KeyCreator final : public base::has_weak_ptr {
public:
	KeyCreator(
		not_null<RequestSender*> lib,
		not_null<Storage::Cache::Database*> db,
		Fn<void(Result<std::vector<QString>>)> done);

	void save(
		const QByteArray &password,
		const WalletList &existing,
		Callback<WalletList::Entry> done);

private:
	enum class State {
		Creating,
		Created,
		ChangingPassword,
		Saving,
	};

	void exportWords(Fn<void(Result<std::vector<QString>>)> done);
	void changePassword(const QByteArray &password, Callback<> done);
	void saveToDatabase(
		WalletList existing,
		Callback<WalletList::Entry> done);

	const not_null<RequestSender*> _lib;
	const not_null<Storage::Cache::Database*> _db;

	State _state = State::Creating;
	QByteArray _key;
	QByteArray _secret;
	QByteArray _password;

};

} // namespace Ton::details
