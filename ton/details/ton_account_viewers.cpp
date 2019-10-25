// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_account_viewers.h"

#include "ton/ton_result.h"
#include "ton/ton_wallet.h"
#include "ton/ton_account_viewer.h"
#include "ton/details/ton_storage.h"
#include "storage/cache/storage_cache_database.h"

namespace Ton::details {
namespace {

constexpr auto kRefreshWithPendingTimeout = 6 * crl::time(1000);

std::vector<PendingTransaction> ComputePendingTransactions(
		std::vector<PendingTransaction> list,
		const AccountState &state,
		const TransactionsSlice &last) {
	const auto processed = [&](const PendingTransaction &transaction) {
		return (transaction.sentUntilSyncTime < state.syncTime)
			|| (ranges::find(last.list, transaction.fake) != end(last.list));
	};
	list.erase(ranges::remove_if(list, processed), end(list));
	return list;
}

} // namespace

AccountViewers::AccountViewers(
	not_null<Wallet*> owner,
	not_null<RequestSender*> lib,
	not_null<Storage::Cache::Database*> db)
: _owner(owner)
, _lib(lib)
, _db(db)
, _refreshTimer([=] { checkNextRefresh(); }) {
}

AccountViewers::~AccountViewers() {
	for (const auto &[address, viewers] : _map) {
		Assert(viewers.list.empty());
	}
}

AccountViewers::Viewers *AccountViewers::findRefreshingViewers(
		const QString &address) {
	const auto i = _map.find(address);
	Assert(i != end(_map));
	if (i->second.list.empty()) {
		_map.erase(i);
		return nullptr;
	}
	return &i->second;
}

void AccountViewers::finishRefreshing(Viewers &viewers, Result<> result) {
	viewers.lastRefresh = crl::now();
	viewers.refreshing = false;
	InvokeCallback(base::take(viewers.refreshed), result);
}

template <typename Data>
bool AccountViewers::reportError(Viewers &viewers, Result<Data> result) {
	if (result.has_value()) {
		return false;
	}
	const auto weak = base::make_weak(this);
	finishRefreshing(viewers, result.error());
	if (!weak) {
		return true;
	}
	checkNextRefresh();
	return true;
}

void AccountViewers::saveNewState(
		Viewers &viewers,
		WalletState &&state,
		RefreshSource source) {
	const auto weak = base::make_weak(this);
	if (source != RefreshSource::Pending) {
		finishRefreshing(viewers);
	}
	if (!weak) {
		return;
	}
	if (viewers.state.current() != state) {
		if (source != RefreshSource::Database) {
			SaveWalletState(_db, state, nullptr);
		}
		viewers.state = std::move(state);
		if (!weak) {
			return;
		}
	}
	if (source == RefreshSource::Database) {
		refreshAccount(state.address, viewers);
	} else {
		checkNextRefresh();
	}
}

void AccountViewers::checkPendingForSameState(
		const QString &address,
		Viewers &viewers,
		const AccountState &state) {
	auto pending = ComputePendingTransactions(
		viewers.state.current().pendingTransactions,
		state,
		TransactionsSlice());
	if (viewers.state.current().pendingTransactions != pending) {
		// Some pending transactions were discarded by the sync time.
		saveNewState(viewers, WalletState{
			address,
			state,
			viewers.state.current().lastTransactions,
			std::move(pending)
		}, RefreshSource::Remote);
	} else {
		finishRefreshing(viewers);
		checkNextRefresh();
	}
}

void AccountViewers::refreshAccount(
		const QString &address,
		Viewers &viewers) {
	viewers.refreshing = true;
	_owner->requestState(address, [=](Result<AccountState> result) {
		const auto viewers = findRefreshingViewers(address);
		if (!viewers || reportError(*viewers, result)) {
			return;
		}
		const auto &state = *result;
		if (state == viewers->state.current().account) {
			checkPendingForSameState(address, *viewers, state);
			return;
		}
		const auto received = [=](Result<TransactionsSlice> result) {
			const auto viewers = findRefreshingViewers(address);
			if (!viewers || reportError(*viewers, result)) {
				return;
			}
			auto pending = ComputePendingTransactions(
				viewers->state.current().pendingTransactions,
				state,
				*result);
			saveNewState(*viewers, WalletState{
				address,
				state,
				std::move(*result),
				std::move(pending)
			}, RefreshSource::Remote);
		};
		_owner->requestTransactions(
			address,
			state.lastTransactionId,
			received);
	});
}

void AccountViewers::checkNextRefresh() {
	constexpr auto kNoRefresh = std::numeric_limits<crl::time>::max();
	auto minWait = kNoRefresh;
	const auto now = crl::now();
	for (auto &[address, viewers] : _map) {
		if (viewers.refreshing.current()) {
			continue;
		}
		Assert(viewers.lastRefresh.current() > 0);
		Assert(!viewers.list.empty());
		const auto min = (*ranges::min_element(
			viewers.list,
			ranges::less(),
			&AccountViewer::refreshEach))->refreshEach();
		const auto use = viewers.state.current().pendingTransactions.empty()
			? min
			: std::min(min, kRefreshWithPendingTimeout);
		const auto next
			= viewers.nextRefresh
			= viewers.lastRefresh.current() + use;
		const auto in = next - now;
		if (in <= 0) {
			refreshAccount(address, viewers);
			continue;
		}
		if (minWait > in) {
			minWait = in;
		}
	}
	if (minWait != kNoRefresh) {
		_refreshTimer.callOnce(minWait);
	}
}

void AccountViewers::refreshFromDatabase(
		const QString &address,
		Viewers &viewers) {
	viewers.refreshing = true;
	auto loaded = [=](Result<WalletState> result) {
		const auto viewers = findRefreshingViewers(address);
		if (!viewers) {
			return;
		}
		saveNewState(
			*viewers,
			result.value_or(WalletState{ address }),
			RefreshSource::Database);
	};
	LoadWalletState(_db, address, crl::guard(this, loaded));
}

std::unique_ptr<AccountViewer> AccountViewers::createAccountViewer(
		const QString &address) {
	const auto i = _map.emplace(
		address,
		Viewers{ WalletState{ address } }
	).first;

	auto &viewers = i->second;
	auto state = rpl::combine(
		viewers.state.value(),
		viewers.lastRefresh.value(),
		viewers.refreshing.value()
	) | rpl::map([](WalletState &&state, crl::time last, bool refreshing) {
		return WalletViewerState{ std::move(state), last, refreshing };
	});
	auto result = std::make_unique<AccountViewer>(
		_owner,
		address,
		std::move(state));
	const auto raw = result.get();
	viewers.list.push_back(raw);

	if (!viewers.nextRefresh) {
		viewers.nextRefresh = raw->refreshEach();
		refreshFromDatabase(address, viewers);
	}

	raw->refreshEachValue(
	) | rpl::start_with_next_done([=] {
		checkNextRefresh();
	}, [=] {
		const auto i = _map.find(address);
		Assert(i != end(_map));
		i->second.list.erase(
			ranges::remove(
				i->second.list,
				raw,
				&not_null<AccountViewer*>::get),
			end(i->second.list));
		if (i->second.list.empty() && !i->second.refreshing.current()) {
			_map.erase(i);
		}
	}, viewers.lifetime);

	raw->refreshNowRequests(
	) | rpl::start_with_next([=](Callback<> &&done) {
		const auto i = _map.find(address);
		Assert(i != end(_map));
		i->second.refreshed = std::move(done);
		if (!i->second.refreshing.current()) {
			refreshAccount(address, i->second);
		}
	}, viewers.lifetime);

	return result;
}

void AccountViewers::addPendingTransaction(
		const PendingTransaction &pending) {
	const auto address = pending.fake.incoming.destination;
	const auto i = _map.find(address);
	if (i != end(_map)) {
		auto state = i->second.state.current();
		state.pendingTransactions.insert(
			begin(state.pendingTransactions),
			pending);
		saveNewState(i->second, std::move(state), RefreshSource::Pending);
	}
}

} // namespace Ton::details
