// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace Ton {

struct WalletState;

class AccountViewer final {
public:
	explicit AccountViewer(rpl::producer<WalletState> state);

	[[nodiscard]] rpl::producer<WalletState> state() const;

	void refreshNow();
	[[nodiscard]] rpl::producer<> refreshNowRequests() const;
	void setRefreshEach(crl::time delay);
	[[nodiscard]] crl::time refreshEach() const;
	[[nodiscard]] rpl::producer<crl::time> refreshEachValue() const;

private:
	rpl::producer<WalletState> _state;
	rpl::variable<crl::time> _refreshEach;
	rpl::event_stream<> _refreshNowRequests;

};

} // namespace Ton
