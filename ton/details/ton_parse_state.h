// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_state.h"
#include "ton_tl.h"

namespace Ton::details {

[[nodiscard]] TransactionId Parse(const TLinternal_TransactionId &data);
[[nodiscard]] AccountState Parse(const TLwallet_AccountState &data);
[[nodiscard]] AccountState Parse(const TLuninited_AccountState &data);
[[nodiscard]] Transaction Parse(const TLraw_Transaction &data);
[[nodiscard]] TransactionsSlice Parse(const TLraw_Transactions &data);
[[nodiscard]] PendingTransaction Parse(
	const TLSendGramsResult &data,
	const QString &sender,
	const TransactionToSend &transaction);

} // namespace Ton::details
