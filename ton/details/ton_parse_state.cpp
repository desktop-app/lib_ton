// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_parse_state.h"

#include <QtCore/QDateTime>

namespace Ton::details {
namespace {

[[nodiscard]] PendingTransaction PreparePending(
		const QString &sender,
		const TransactionToSend &transaction,
		int64 sentUntilSyncTime,
		const QByteArray &bodyHash) {
	auto result = PendingTransaction();
	result.sentUntilSyncTime = sentUntilSyncTime;
	result.fake.time = QDateTime::currentDateTime().toTime_t();
	result.fake.incoming.bodyHash = bodyHash;
	result.fake.incoming.destination = sender;
	auto &outgoing = result.fake.outgoing.emplace_back();
	outgoing.source = sender;
	outgoing.destination = transaction.recipient;
	outgoing.message = transaction.comment.toUtf8();
	outgoing.value = transaction.amount;
	return result;
}

} // namespace

TransactionId Parse(const TLinternal_TransactionId &data) {
	return data.match([&](const TLDinternal_transactionId &data) {
		auto result = TransactionId();
		result.lt = data.vlt().v;
		result.hash = data.vhash().v;
		return result;
	});
}

AccountState Parse(const TLwallet_AccountState &data) {
	return data.match([&](const TLDwallet_accountState &data) {
		auto result = AccountState();
		result.balance = data.vbalance().v;
		result.lastTransactionId = Parse(data.vlast_transaction_id());
		result.syncTime = data.vsync_utime().v;
		return result;
	});
}

AccountState Parse(const TLuninited_AccountState &data) {
	return data.match([&](const TLDuninited_accountState &data) {
		auto result = AccountState();
		result.balance = data.vbalance().v;
		result.lastTransactionId = Parse(data.vlast_transaction_id());
		result.syncTime = data.vsync_utime().v;
		return result;
	});
}

Message Parse(const TLraw_Message &data) {
	return data.match([&](const TLDraw_message &data) {
		auto result = Message();
		result.bodyHash = data.vbody_hash().v;
		result.created = data.vcreated_lt().v;
		result.source = tl::utf16(data.vsource());
		result.destination = tl::utf16(data.vdestination());
		result.message = data.vmessage().v;
		result.value = data.vvalue().v;
		return result;
	});
}

Transaction Parse(const TLraw_Transaction &data) {
	return data.match([&](const TLDraw_transaction &data) {
		auto result = Transaction();
		result.fee = data.vfee().v;
		result.id = Parse(data.vtransaction_id());
		result.incoming = Parse(data.vin_msg());
		result.outgoing = ranges::view::all(
			data.vout_msgs().v
		) | ranges::view::transform([](const TLraw_Message &data) {
			return Parse(data);
		}) | ranges::to_vector;
		result.otherFee = data.vother_fee().v;
		result.storageFee = data.vstorage_fee().v;
		result.time = data.vutime().v;
		return result;
	});
}

TransactionsSlice Parse(const TLraw_Transactions &data) {
	return data.match([&](const TLDraw_transactions &data) {
		auto result = TransactionsSlice();
		result.previousId = Parse(data.vprevious_transaction_id());
		result.list = ranges::view::all(
			data.vtransactions().v
		) | ranges::view::transform([](const TLraw_Transaction &data) {
			return Parse(data);
		}) | ranges::to_vector;
		return result;
	});
}

PendingTransaction Parse(
		const TLSendGramsResult &data,
		const QString &sender,
		const TransactionToSend &transaction) {
	return data.match([&](const TLDsendGramsResult &data) {
		return PreparePending(
			sender,
			transaction,
			data.vsent_until().v,
			data.vbody_hash().v);
	});
}

PendingTransaction Parse(
		const TLquery_Info &data,
		const QString &sender,
		const TransactionToSend &transaction) {
	return data.match([&](const TLDquery_info &data) {
		return PreparePending(
			sender,
			transaction,
			data.vvalid_until().v,
			data.vbody_hash().v);
	});
}

TransactionFees Parse(const TLFees &data) {
	return data.match([&](const TLDfees &data) {
		auto result = TransactionFees();
		result.inForward = data.vin_fwd_fee().v;
		result.gas = data.vgas_fee().v;
		result.storage = data.vstorage_fee().v;
		result.forward = data.vfwd_fee().v;
		return result;
	});
}

TransactionCheckResult Parse(const TLquery_Fees &data) {
	return data.match([&](const TLDquery_fees &data) {
		auto result = TransactionCheckResult();
		result.sourceFees = Parse(data.vsource_fees());
		result.destinationFees = Parse(data.vdestination_fees());
		return result;
	});
}

} // namespace Ton::details
