// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/details/ton_request_sender.h"
#include "ton/ton_result.h"
#include "storage/cache/storage_cache_database.h"
#include "base/bytes.h"
#include "base/weak_ptr.h"

namespace Ton {
struct Config;
} // namespace Ton

namespace Ton::details {

struct WalletList {
	struct Entry {
		QByteArray publicKey;
		QByteArray secret;
	};
	std::vector<Entry> entries;
};

[[nodiscard]] std::optional<Error> ErrorFromStorage(
	const Storage::Cache::Error &error);

void DeleteKeyFromLibrary(
	not_null<RequestSender*> lib,
	const QByteArray &publicKey,
	const QByteArray &secret,
	Callback<> done);

class External final : public base::has_weak_ptr {
public:
	explicit External(const QString &path);

	void open(
		const QByteArray &globalPassword,
		const Config &config,
		Callback<WalletList> done);
	void setConfig(const Config &config, Callback<> done);

	[[nodiscard]] RequestSender &lib();
	[[nodiscard]] Fn<void(WalletList, Callback<>)> saveListMethod();

private:
	enum class State {
		Initial,
		Opening,
		Opened,
	};

	[[nodiscard]] Result<> loadSalt();
	[[nodiscard]] Result<> writeNewSalt();
	void openDatabase(
		const QByteArray &globalPassword,
		Callback<WalletList> done);
	void startLibrary(const Config &config, Callback<> done);

	const QString _basePath;
	RequestSender _lib;
	Storage::Cache::Database _db;

	State _state = State::Initial;
	bytes::vector _salt;

};

} // namespace Ton::details
