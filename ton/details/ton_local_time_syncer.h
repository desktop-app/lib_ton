// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton_tl.h"

namespace Ton::details {

class RequestSender;

struct BlockchainTime {
	crl::time when = 0;
	TimeId what = 0;
};

class LocalTimeSyncer final {
public:
	LocalTimeSyncer(
		BlockchainTime time,
		not_null<RequestSender*> lib,
		Fn<void()> destroy);

	void updateBlockchainTime(BlockchainTime time);

	[[nodiscard]] static bool IsRequestFastEnough(
		crl::time sent,
		crl::time done);
	[[nodiscard]] static bool IsLocalTimeBad(BlockchainTime time);

private:
	void getLiteServerTime();
	void sync(const TLliteServer_Info &result);

	const not_null<RequestSender*> _lib;
	const Fn<void()> _destroy;

	BlockchainTime _blockchainTime;

};

} // namespace Ton::details
