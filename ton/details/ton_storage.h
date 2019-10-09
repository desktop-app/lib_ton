// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_result.h"

namespace Storage {
namespace Cache {
class Database;
struct Error;
} // namespace Cache
} // namespace Storage

namespace Ton::details {

class RequestSender;

struct WalletList {
	struct Entry {
		QByteArray publicKey;
		QByteArray secret;
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

[[nodiscard]] QByteArray Pack(const WalletList &data);
[[nodiscard]] WalletList Unpack(const QByteArray &data);

} // namespace Ton::details
