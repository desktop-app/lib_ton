// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/ton_account_viewer.h"

#include "ton/ton_state.h"

namespace Ton {
namespace {

constexpr auto kDefaultRefreshEach = 60 * crl::time(1000);

} // namespace

AccountViewer::AccountViewer(rpl::producer<WalletViewerState> state)
: _state(std::move(state))
, _refreshEach(kDefaultRefreshEach) {
}

rpl::producer<WalletViewerState> AccountViewer::state() const {
	return rpl::duplicate(_state);
}

void AccountViewer::refreshNow(Callback<> done) {
	_refreshNowRequests.fire(std::move(done));
}

rpl::producer<Callback<>> AccountViewer::refreshNowRequests() const {
	return _refreshNowRequests.events();
}

void AccountViewer::setRefreshEach(crl::time delay) {
	_refreshEach = delay;
}

crl::time AccountViewer::refreshEach() const {
	return _refreshEach.current();
}

rpl::producer<crl::time> AccountViewer::refreshEachValue() const {
	return _refreshEach.value();
}

} // namespace Ton
