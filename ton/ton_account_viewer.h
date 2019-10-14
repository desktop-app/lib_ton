// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_result.h"

namespace Ton {

struct WalletViewerState;

class AccountViewer final {
public:
	explicit AccountViewer(rpl::producer<WalletViewerState> state);

	[[nodiscard]] rpl::producer<WalletViewerState> state() const;

	void refreshNow(Callback<>);
	[[nodiscard]] rpl::producer<Callback<>> refreshNowRequests() const;
	void setRefreshEach(crl::time delay);
	[[nodiscard]] crl::time refreshEach() const;
	[[nodiscard]] rpl::producer<crl::time> refreshEachValue() const;

private:
	rpl::producer<WalletViewerState> _state;
	rpl::variable<crl::time> _refreshEach;
	rpl::event_stream<Callback<>> _refreshNowRequests;

};

} // namespace Ton
