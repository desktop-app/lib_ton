// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/ton_account_viewer.h"

#include "ton/ton_state.h"
#include "ton/ton_wallet.h"
#include "ton/details/ton_parse_state.h"

namespace Ton {
namespace {

using namespace details;

constexpr auto kDefaultRefreshEach = 60 * crl::time(1000);

} // namespace

AccountViewer::AccountViewer(
	not_null<Wallet*> wallet,
	const QByteArray &publicKey,
	const QString &address,
	rpl::producer<WalletViewerState> state)
: _wallet(wallet)
, _publicKey(publicKey)
, _address(address)
, _state(std::move(state))
, _refreshEach(kDefaultRefreshEach) {
}

rpl::producer<WalletViewerState> AccountViewer::state() const {
	return rpl::duplicate(_state);
}

rpl::producer<Result<LoadedSlice>> AccountViewer::loaded() const {
	return _loadedResults.events();
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

void AccountViewer::preloadSlice(const TransactionId &lastId) {
	if (_preloadIds.contains(lastId)) {
		return;
	}
	_preloadIds.emplace(lastId);
	const auto done = [=](Result<TransactionsSlice> result) {
		if (!result) {
			_loadedResults.fire(std::move(result.error()));
			return;
		}
		auto last = std::move(*result);
		const auto encrypted = CollectEncryptedTexts(last);
		const auto done = [=](Result<QVector<DecryptedText>> result) {
			if (!result) {
				_loadedResults.fire(std::move(result.error()));
				return;
			}
			_preloadIds.remove(lastId);
			_loadedResults.fire(LoadedSlice{
				lastId,
				AddDecryptedTexts(last, encrypted, *result)
			});
		};
		_wallet->decryptTexts(_publicKey, encrypted, done);
	};
	_wallet->requestTransactions(
		_publicKey,
		_address,
		lastId,
		crl::guard(this, done));
}

} // namespace Ton
