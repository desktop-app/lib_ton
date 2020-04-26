// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/weak_ptr.h"
#include "ton/ton_result.h"

namespace Storage::Cache {
class Database;
} // namespace Storage::Cache

namespace Ton::details {

class RequestSender;
struct WalletList;

class KeyDestroyer final : public base::has_weak_ptr {
public:
	KeyDestroyer(
		not_null<RequestSender*> lib,
		not_null<Storage::Cache::Database*> db,
		const WalletList &existing,
		index_type index,
		bool useTestNetwork,
		Callback<> done);
	KeyDestroyer(
		not_null<RequestSender*> lib,
		not_null<Storage::Cache::Database*> db,
		bool useTestNetwork,
		Callback<> done);

};

} // namespace Ton::details
