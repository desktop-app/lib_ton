// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_parse_state.h"

#include <QtCore/QDateTime>

namespace Ton::details {

TransactionId Parse(const TLinternal_TransactionId &data) {
	return data.match([&](const TLDinternal_transactionId &data) {
		auto result = TransactionId();
		result.id = data.vlt().v;
		result.hash = data.vhash().v;
		return result;
	});
}

AccountState Parse(const TLwallet_AccountState &data) {
	return data.match([&](const TLDwallet_accountState &data) {
		auto result = AccountState();
		result.balance = data.vbalance().v;
		result.lastTransactionId = Parse(data.vlast_transaction_id());
		result.seqNo = data.vseqno().v;
		result.syncTime = data.vsync_utime().v;
		return result;
	});
}

AccountState Parse(const TLuninited_AccountState &data) {
	return data.match([&](const TLDuninited_accountState &data) {
		auto result = AccountState();
		result.balance = data.vbalance().v;
		result.frozenHash = data.vfrozen_hash().v;
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

SentTransaction Parse(
		const TLSendGramsResult &data,
		const QString &sender,
		const TransactionToSend &transaction) {
	auto result = SentTransaction();
	result.fake.time = QDateTime::currentDateTime().toTime_t();
	data.match([&](const TLDsendGramsResult &data) {
		result.sentUntilSyncTime = data.vsent_until().v;
		result.fake.incoming.bodyHash = data.vbody_hash().v;
	});
	result.fake.incoming.destination = sender;
	auto &outgoing = result.fake.outgoing.emplace_back();
	outgoing.source = sender;
	outgoing.destination = transaction.recipient;
	outgoing.message = transaction.comment.toUtf8();
	outgoing.value = transaction.amount;
	return result;
}

} // namespace Ton::details
