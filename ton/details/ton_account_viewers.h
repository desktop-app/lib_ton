// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_state.h"
#include "ton/ton_result.h"
#include "ton/details/ton_local_time_syncer.h"
#include "base/weak_ptr.h"
#include "base/timer.h"

namespace Storage::Cache {
class Database;
} // namespace Storage::Cache

namespace Ton {
class Wallet;
class AccountViewer;
struct PendingTransaction;
} // namespace Ton

namespace Ton::details {

class RequestSender;

class AccountViewers final : public base::has_weak_ptr {
public:
	AccountViewers(
		not_null<Wallet*> owner,
		not_null<RequestSender*> lib,
		not_null<Storage::Cache::Database*> db);
	~AccountViewers();

	[[nodiscard]] std::unique_ptr<AccountViewer> createAccountViewer(
		const QByteArray &publicKey,
		const QString &address);
	void addPendingTransaction(const PendingTransaction &pending);

	[[nodiscard]] rpl::producer<BlockchainTime> blockchainTime() const;

private:
	struct Viewers {
		QByteArray publicKey;
		rpl::variable<WalletState> state;
		rpl::variable<crl::time> lastGoodRefresh = 0;
		rpl::variable<bool> refreshing = false;
		crl::time lastRefreshFinished = 0;
		crl::time nextRefresh = 0;
		Callback<> refreshed;
		std::vector<not_null<AccountViewer*>> list;
		rpl::lifetime lifetime;
	};
	enum class RefreshSource {
		Database,
		Remote,
		Pending,
	};

	void refreshFromDatabase(const QString &address, Viewers &viewers);
	void refreshAccount(const QString &address, Viewers &viewers);
	void checkPendingForSameState(
		const QString &address,
		Viewers &viewers,
		const AccountState &state);
	void checkNextRefresh();
	Viewers *findRefreshingViewers(const QString &address);
	void finishRefreshing(Viewers &viewers, Result<> result = {});
	template <typename Data>
	bool reportError(Viewers &viewers, Result<Data> result);
	void saveNewState(
		Viewers &viewers,
		WalletState &&state,
		RefreshSource source);

	const not_null<Wallet*> _owner;
	const not_null<RequestSender*> _lib;
	const not_null<Storage::Cache::Database*> _db;

	base::flat_map<QString, Viewers> _map;

	base::Timer _refreshTimer;

	rpl::event_stream<BlockchainTime> _blockchainTime;

};

} // namespace Ton::details
