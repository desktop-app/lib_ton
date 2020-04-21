// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_result.h"

namespace Storage::Cache {
class Database;
struct Error;
} // namespace Storage::Cache

namespace Ton {
struct TransactionId;
struct AccountState;
struct Message;
struct Transaction;
struct TransactionsSlice;
struct PendingTransaction;
struct WalletState;
struct Settings;
} // namespace Ton

namespace Ton::details {

class RequestSender;

struct WalletList {
	struct Entry {
		QByteArray publicKey;
		QByteArray secret;
		QByteArray restrictedInitPublicKey;
	};
	std::vector<Entry> entries;
};

[[nodiscard]] std::optional<Error> ErrorFromStorage(
	const Storage::Cache::Error &error);

void DeletePublicKey(
	not_null<RequestSender*> lib,
	const QByteArray &publicKey,
	const QByteArray &secret,
	Callback<> done);

void SaveWalletList(
	not_null<Storage::Cache::Database*> db,
	const WalletList &list,
	Callback<> done);
void LoadWalletList(
	not_null<Storage::Cache::Database*> db,
	Fn<void(WalletList&&)> done);

void SaveWalletState(
	not_null<Storage::Cache::Database*> db,
	const WalletState &state,
	Callback<> done);
void LoadWalletState(
	not_null<Storage::Cache::Database*> db,
	const QString &address,
	Fn<void(WalletState&&)> done);

void SaveSettings(
	not_null<Storage::Cache::Database*> db,
	const Settings &settings,
	Callback<> done);
void LoadSettings(
	not_null<Storage::Cache::Database*> db,
	Fn<void(Settings&&)> done);

} // namespace Ton::details
