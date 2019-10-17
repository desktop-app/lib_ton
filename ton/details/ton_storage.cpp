// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_storage.h"

#include "ton/details/ton_request_sender.h"
#include "ton/ton_state.h"
#include "storage/cache/storage_cache_database.h"
#include "ton_storage_tl.h"

namespace Ton::details {
namespace {

constexpr auto kWalletListKey = Storage::Cache::Key{ 1ULL, 1ULL };

[[nodiscard]] Storage::Cache::Key WalletStateKey(const QString &address) {
	const auto utf8 = address.toUtf8();
	const auto decoded = QByteArray::fromBase64(
		address.toUtf8(),
		QByteArray::Base64UrlEncoding);
	Assert(decoded.size() == 36);
	auto a = uint64();
	auto b = uint64();
	memcpy(&a, decoded.data() + 2, sizeof(uint64));
	memcpy(&b, decoded.data() + 2 + sizeof(uint64), sizeof(uint64));
	return { 0x2ULL | (a & 0xFFFFFFFFFFFF0000ULL), b };
}

TLstorage_WalletEntry Serialize(const WalletList::Entry &data);
WalletList::Entry Deserialize(const TLstorage_WalletEntry &data);
TLstorage_WalletList Serialize(const WalletList &data);
WalletList Deserialize(const TLstorage_WalletList &data);
TLstorage_TransactionId Serialize(const TransactionId &data);
TransactionId Deserialize(const TLstorage_TransactionId &data);
TLstorage_AccountState Serialize(const AccountState &data);
AccountState Deserialize(const TLstorage_AccountState &data);
TLstorage_Message Serialize(const Message &data);
Message Deserialize(const TLstorage_Message &data);
TLstorage_Transaction Serialize(const Transaction &data);
Transaction Deserialize(const TLstorage_Transaction &data);
TLstorage_TransactionsSlice Serialize(const TransactionsSlice &data);
TransactionsSlice Deserialize(const TLstorage_TransactionsSlice &data);
TLstorage_PendingTransaction Serialize(const PendingTransaction &data);
PendingTransaction Deserialize(const TLstorage_PendingTransaction &data);
TLstorage_WalletState Serialize(const WalletState &data);
WalletState Deserialize(const TLstorage_WalletState &data);

template <
	typename Data,
	typename Result = decltype(Serialize(std::declval<Data>()))>
TLvector<Result> Serialize(const std::vector<Data> &data) {
	auto result = QVector<Result>();
	result.reserve(data.size());
	for (const auto &entry : data) {
		result.push_back(Serialize(entry));
	}
	return tl_vector<Result>(std::move(result));
}

template <
	typename TLType,
	typename Result = decltype(Deserialize(std::declval<TLType>()))>
std::vector<Result> Deserialize(const TLvector<TLType> &data) {
	auto result = std::vector<Result>();
	result.reserve(data.v.size());
	for (const auto &entry : data.v) {
		result.emplace_back(Deserialize(entry));
	}
	return result;
}

TLstorage_WalletEntry Serialize(const WalletList::Entry &data) {
	return make_storage_walletEntry(
		tl_string(data.publicKey),
		tl_bytes(data.secret));
}

WalletList::Entry Deserialize(const TLstorage_WalletEntry &data) {
	return data.match([&](const TLDstorage_walletEntry &data) {
		return WalletList::Entry{
			data.vpublicKey().v,
			data.vsecret().v
		};
	});
}

TLstorage_WalletList Serialize(const WalletList &data) {
	return make_storage_walletList(Serialize(data.entries));
}

WalletList Deserialize(const TLstorage_WalletList &data) {
	auto result = WalletList();
	data.match([&](const TLDstorage_walletList &data) {
		result.entries = Deserialize(data.ventries());
	});
	return result;
}

TLstorage_TransactionId Serialize(const TransactionId &data) {
	return make_storage_transactionId(
		tl_int64(data.lt),
		tl_bytes(data.hash));
}

TransactionId Deserialize(const TLstorage_TransactionId &data) {
	return data.match([&](const TLDstorage_transactionId &data) {
		return TransactionId{
			data.vlt().v,
			data.vhash().v
		};
	});
}

TLstorage_AccountState Serialize(const AccountState &data) {
	return make_storage_accountState(
		tl_int64(data.balance),
		tl_int64(data.syncTime),
		Serialize(data.lastTransactionId));
}

AccountState Deserialize(const TLstorage_AccountState &data) {
	return data.match([&](const TLDstorage_accountState &data) {
		return AccountState{
			data.vbalance().v,
			data.vsyncTime().v,
			Deserialize(data.vlastTransactionId())
		};
	});
}

TLstorage_Message Serialize(const Message &data) {
	return make_storage_message(
		tl_string(data.source),
		tl_string(data.destination),
		tl_int64(data.value),
		tl_int64(data.created),
		tl_bytes(data.bodyHash),
		tl_string(data.message));
}

Message Deserialize(const TLstorage_Message &data) {
	return data.match([&](const TLDstorage_message &data) {
		return Message{
			tl::utf16(data.vsource()),
			tl::utf16(data.vdestination()),
			data.vvalue().v,
			data.vcreated().v,
			data.vbodyHash().v,
			tl::utf16(data.vmessage())
		};
	});
}

TLstorage_Transaction Serialize(const Transaction &data) {
	return make_storage_transaction(
		Serialize(data.id),
		tl_int64(data.time),
		tl_int64(data.fee),
		tl_int64(data.storageFee),
		tl_int64(data.otherFee),
		Serialize(data.incoming),
		Serialize(data.outgoing));
}

Transaction Deserialize(const TLstorage_Transaction &data) {
	return data.match([&](const TLDstorage_transaction &data) {
		return Transaction{
			Deserialize(data.vid()),
			data.vtime().v,
			data.vfee().v,
			data.vstorageFee().v,
			data.votherFee().v,
			Deserialize(data.vincoming()),
			Deserialize(data.voutgoing())
		};
	});
}

TLstorage_TransactionsSlice Serialize(const TransactionsSlice &data) {
	return make_storage_transactionsSlice(
		Serialize(data.list),
		Serialize(data.previousId));
}

TransactionsSlice Deserialize(const TLstorage_TransactionsSlice &data) {
	return data.match([&](const TLDstorage_transactionsSlice &data) {
		return TransactionsSlice{
			Deserialize(data.vlist()),
			Deserialize(data.vpreviousId())
		};
	});
}

TLstorage_PendingTransaction Serialize(const PendingTransaction &data) {
	return make_storage_pendingTransaction(
		Serialize(data.fake),
		tl_int64(data.sentUntilSyncTime));
}

PendingTransaction Deserialize(const TLstorage_PendingTransaction &data) {
	return data.match([&](const TLDstorage_pendingTransaction &data) {
		return PendingTransaction{
			Deserialize(data.vfake()),
			data.vsentUntilSyncTime().v
		};
	});
}

TLstorage_WalletState Serialize(const WalletState &data) {
	return make_storage_walletState(
		tl_string(data.address),
		Serialize(data.account),
		Serialize(data.lastTransactions),
		Serialize(data.pendingTransactions));
}

WalletState Deserialize(const TLstorage_WalletState &data) {
	auto result = WalletState();
	data.match([&](const TLDstorage_walletState &data) {
		result.address = tl::utf16(data.vaddress());
		result.account = Deserialize(data.vaccount());
		result.lastTransactions = Deserialize(data.vlastTransactions());
		result.pendingTransactions = Deserialize(
			data.vpendingTransactions());
	});
	return result;
}

template <typename Data>
QByteArray Pack(const Data &data) {
	const auto packed = Serialize(data);
	auto result = QByteArray();
	result.reserve(tl::count_length(packed));
	packed.write(result);
	return result;
}

template <
	typename Data,
	typename TLType = decltype(Serialize(std::declval<Data>()))>
Data Unpack(const QByteArray &data) {
	auto result = TLType();
	auto from = data.data();
	const auto till = from + data.size();
	return result.read(from, till) ? Deserialize(result) : Data();
}

} // namespace

std::optional<Error> ErrorFromStorage(const Storage::Cache::Error &error) {
	using Type = Storage::Cache::Error::Type;
	if (error.type == Type::IO || error.type == Type::LockFailed) {
		return Error{ Error::Type::IO, error.path };
	} else if (error.type == Type::WrongKey) {
		return Error{ Error::Type::WrongPassword };
	}
	return std::nullopt;
}

void DeletePublicKey(
		not_null<RequestSender*> lib,
		const QByteArray &publicKey,
		const QByteArray &secret,
		Callback<> done) {
	lib->request(TLDeleteKey(
		tl_key(tl_string(publicKey), TLsecureBytes{ secret })
	)).done([=] {
		InvokeCallback(done);
	}).fail([=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	}).send();
}

void SaveWalletList(
		not_null<Storage::Cache::Database*> db,
		const WalletList &list,
		Callback<> done) {
	auto saved = [=](Storage::Cache::Error error) {
		crl::on_main([=] {
			if (const auto bad = ErrorFromStorage(error)) {
				InvokeCallback(done, *bad);
			} else {
				InvokeCallback(done);
			}
		});
	};
	if (list.entries.empty()) {
		db->remove(kWalletListKey, std::move(saved));
	} else {
		db->put(kWalletListKey, Pack(list), std::move(saved));
	}
}

void LoadWalletList(
		not_null<Storage::Cache::Database*> db,
		Fn<void(WalletList&&)> done) {
	Expects(done != nullptr);

	db->get(kWalletListKey, [=](QByteArray value) {
		crl::on_main([done, result = Unpack<WalletList>(value)]() mutable {
			done(std::move(result));
		});
	});
}

void SaveWalletState(
		not_null<Storage::Cache::Database*> db,
		const WalletState &state,
		Callback<> done) {
	if (state == WalletState{ state.address }) {
		InvokeCallback(done);
		return;
	}
	auto saved = [=](Storage::Cache::Error error) {
		crl::on_main([=] {
			if (const auto bad = ErrorFromStorage(error)) {
				InvokeCallback(done, *bad);
			} else {
				InvokeCallback(done);
			}
		});
	};
	db->put(WalletStateKey(state.address), Pack(state), std::move(saved));
}

void LoadWalletState(
		not_null<Storage::Cache::Database*> db,
		const QString &address,
		Fn<void(WalletState&&)> done) {
	Expects(done != nullptr);

	db->get(WalletStateKey(address), [=](QByteArray value) {
		crl::on_main([=, result = Unpack<WalletState>(value)]() mutable {
			done((result.address == address)
				? std::move(result)
				: WalletState{ address });
		});
	});
}

} // namespace Ton::details
