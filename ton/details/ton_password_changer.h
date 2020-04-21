// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/weak_ptr.h"
#include "ton/ton_result.h"
#include "ton/details/ton_external.h"
#include "ton/details/ton_storage.h"

namespace Storage::Cache {
class Database;
} // namespace Storage::Cache

namespace Ton {
struct Error;
} // namespace Ton

namespace Ton::details {

class RequestSender;
struct WalletList;

class PasswordChanger final : public base::has_weak_ptr {
public:
	PasswordChanger(
		not_null<RequestSender*> lib,
		not_null<Storage::Cache::Database*> db,
		const QByteArray &oldPassword,
		const QByteArray &newPassword,
		WalletList existing,
		Callback<std::vector<QByteArray>> done);

private:
	void changeNext();
	void savedNext(const QByteArray &newSecret);
	void rollback(Error error);
	void rollforward();

	const not_null<RequestSender*> _lib;
	const not_null<Storage::Cache::Database*> _db;
	const QByteArray _oldPassword;
	const QByteArray _newPassword;
	const Callback<std::vector<QByteArray>> _done;
	WalletList _list;
	std::vector<QByteArray> _newSecrets;

};

} // namespace Ton::details
