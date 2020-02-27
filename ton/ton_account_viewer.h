// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_result.h"
#include "ton/ton_state.h"
#include "base/weak_ptr.h"

namespace Ton {

class Wallet;
struct WalletViewerState;
struct LoadedSlice;

class AccountViewer final : public base::has_weak_ptr {
public:
	AccountViewer(
		not_null<Wallet*> wallet,
		const QByteArray &publicKey,
		const QString &address,
		rpl::producer<WalletViewerState> state);

	[[nodiscard]] rpl::producer<WalletViewerState> state() const;

	void refreshNow(Callback<>);
	[[nodiscard]] rpl::producer<Callback<>> refreshNowRequests() const;
	void setRefreshEach(crl::time delay);
	[[nodiscard]] crl::time refreshEach() const;
	[[nodiscard]] rpl::producer<crl::time> refreshEachValue() const;

	void preloadSlice(const TransactionId &lastId);
	[[nodiscard]] rpl::producer<Result<LoadedSlice>> loaded() const;

private:
	const not_null<Wallet*> _wallet;
	const QByteArray _publicKey;
	const QString _address;

	base::flat_set<TransactionId> _preloadIds;

	rpl::producer<WalletViewerState> _state;
	rpl::variable<crl::time> _refreshEach;
	rpl::event_stream<Callback<>> _refreshNowRequests;
	rpl::event_stream<Result<LoadedSlice>> _loadedResults;

};

} // namespace Ton
