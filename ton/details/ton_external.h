// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/details/ton_request_sender.h"
#include "ton/ton_result.h"
#include "storage/storage_databases.h"
#include "base/bytes.h"
#include "base/weak_ptr.h"

namespace Storage {
namespace Cache {
class Database;
} // namespace Cache
} // namespace Storage

namespace Ton {
struct Config;
} // namespace Ton

namespace Ton::details {

class RequestSender;
struct WalletList;

class External final : public base::has_weak_ptr {
public:
	explicit External(const QString &path);

	void open(
		const QByteArray &globalPassword,
		const Config &config,
		Callback<WalletList> done);
	void setConfig(const Config &config, Callback<> done);

	[[nodiscard]] RequestSender &lib();
	[[nodiscard]] Storage::Cache::Database &db();

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
	Storage::DatabasePointer _db;

	State _state = State::Initial;
	bytes::vector _salt;

};

} // namespace Ton::details
