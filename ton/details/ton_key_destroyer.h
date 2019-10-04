// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/weak_ptr.h"
#include "ton/ton_wallet.h"

namespace Ton::details {

class RequestSender;
struct WalletList;

class KeyDestroyer final : public base::has_weak_ptr {
public:
	KeyDestroyer(
		not_null<RequestSender*> lib,
		const WalletList &existing,
		index_type index,
		Fn<void(WalletList, Callback<>)> saveList,
		Callback<> done);

};

} // namespace Ton::details
