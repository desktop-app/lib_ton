// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_local_time_syncer.h"

#include "ton/details/ton_request_sender.h"
#include "base/unixtime.h"

namespace Ton::details {
namespace {

constexpr auto kSyncTimeDelta = TimeId(30);
constexpr auto kSlowRequestThreshold = 10 * crl::time(1000);

[[nodiscard]] bool IsTimeSimilarEnough(TimeId a, TimeId b) {
	return std::abs(a - b) < kSyncTimeDelta;
}

[[nodiscard]] TimeId AdjustedBlockchainTime(BlockchainTime time) {
	return time.what + ((crl::now() - time.when) / crl::time(1000));
}

} // namespace

LocalTimeSyncer::LocalTimeSyncer(
	BlockchainTime time,
	not_null<RequestSender*> lib,
	Fn<void()> destroy)
: _lib(lib)
, _destroy(std::move(destroy))
, _blockchainTime(time) {
	getLiteServerTime();
}

void LocalTimeSyncer::getLiteServerTime() {
	const auto requested = crl::now();
	_lib->request(TLliteServer_GetInfo(
	)).done([=](const TLliteServer_Info &result) {
		if (IsRequestFastEnough(requested, crl::now())) {
			sync(result);
		}
		const auto onstack = _destroy;
		onstack();
	}).send();
}

void LocalTimeSyncer::updateBlockchainTime(BlockchainTime time) {
	_blockchainTime = time;
}

void LocalTimeSyncer::sync(const TLliteServer_Info &result) {
	result.match([&](const TLDliteServer_info &data) {
		const auto liteServerTime = TimeId(data.vnow().v);
		const auto blockchainTime = AdjustedBlockchainTime(_blockchainTime);
		const auto localTime = base::unixtime::now();
		if (IsTimeSimilarEnough(liteServerTime, blockchainTime)
			&& !IsTimeSimilarEnough(liteServerTime, localTime)) {
			base::unixtime::update(liteServerTime, true);
		}
	});
}

bool LocalTimeSyncer::IsRequestFastEnough(crl::time sent, crl::time done) {
	return (done - sent) < kSlowRequestThreshold;
}

bool LocalTimeSyncer::IsLocalTimeBad(BlockchainTime time) {
	return !IsTimeSimilarEnough(
		base::unixtime::now(),
		AdjustedBlockchainTime(time));
}

} // namespace Ton::details
