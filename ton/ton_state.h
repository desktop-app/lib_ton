// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_settings.h"

namespace Ton {

inline constexpr auto kUnknownBalance = int64(-666);

struct TransactionId {
	int64 lt = 0;
	QByteArray hash;
};

bool operator==(
	const TransactionId &a,
	const TransactionId &b);
bool operator!=(
	const TransactionId &a,
	const TransactionId &b);
bool operator<(
	const TransactionId &a,
	const TransactionId &b);

struct AccountState {
	int64 balance = kUnknownBalance;
	int64 syncTime = 0;
	TransactionId lastTransactionId;
};

bool operator==(const AccountState &a, const AccountState &b);
bool operator!=(const AccountState &a, const AccountState &b);

struct Message {
	QString source;
	QString destination;
	int64 value = 0;
	int64 created = 0;
	QByteArray bodyHash;
	QString message;
};

struct Transaction {
	TransactionId id;
	int64 time = 0;
	int64 fee = 0;
	int64 storageFee = 0;
	int64 otherFee = 0;
	Message incoming;
	std::vector<Message> outgoing;
};

bool operator==(const Transaction &a, const Transaction &b);
bool operator!=(const Transaction &a, const Transaction &b);

struct TransactionsSlice {
	std::vector<Transaction> list;
	TransactionId previousId;
};

bool operator==(
	const TransactionsSlice &a,
	const TransactionsSlice &b);
bool operator!=(
	const TransactionsSlice &a,
	const TransactionsSlice &b);

struct TransactionToSend {
	QString recipient;
	int64 amount = 0;
	int timeout = 0;
	bool allowSendToUninited = false;
	QString comment;
};

struct TransactionFees {
	int64 inForward = 0;
	int64 storage = 0;
	int64 gas = 0;
	int64 forward = 0;

	[[nodiscard]] int64 sum() const;
};

struct TransactionCheckResult {
	TransactionFees sourceFees;
	std::vector<TransactionFees> destinationFees;
};

struct PendingTransaction {
	Transaction fake;
	int64 sentUntilSyncTime = 0;
};

bool operator==(
	const PendingTransaction &a,
	const PendingTransaction &b);
bool operator!=(
	const PendingTransaction &a,
	const PendingTransaction &b);

struct WalletState {
	QString address;
	AccountState account;
	TransactionsSlice lastTransactions;
	std::vector<PendingTransaction> pendingTransactions;
};

bool operator==(const WalletState &a, const WalletState &b);
bool operator!=(const WalletState &a, const WalletState &b);

struct WalletViewerState {
	WalletState wallet;
	crl::time lastRefresh = 0;
	bool refreshing = false;
};

struct LoadedSlice {
	TransactionId after;
	TransactionsSlice data;
};

struct SyncState {
	int from = 0;
	int to = 0;
	int current = 0;

	bool valid() const;
};

bool operator==(const SyncState &a, const SyncState &b);
bool operator!=(const SyncState &a, const SyncState &b);

struct LiteServerQuery {
	int64 id = 0;
	QByteArray bytes;
};

struct Update {
	base::variant<SyncState, LiteServerQuery, ConfigUpgrade> data;
};

} // namespace Ton
