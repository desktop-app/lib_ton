// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_storage.h"

#include "ton/details/ton_request_sender.h"
#include "ton/ton_state.h"
#include "ton/ton_settings.h"
#include "storage/cache/storage_cache_database.h"
#include "ton_storage_tl.h"

namespace Ton::details {
namespace {

constexpr auto kSettingsKey = Storage::Cache::Key{ 1ULL, 0ULL };
constexpr auto kWalletTestListKey = Storage::Cache::Key{ 1ULL, 1ULL };
constexpr auto kWalletMainListKey = Storage::Cache::Key{ 1ULL, 2ULL };

[[nodiscard]] Storage::Cache::Key WalletListKey(bool useTestNetwork) {
	return useTestNetwork
		? kWalletTestListKey
		: kWalletMainListKey;
}

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

TLstorage_Bool Serialize(const bool &data);
bool Deserialize(const TLstorage_Bool &data);
TLstorage_WalletEntry Serialize(const WalletList::Entry &data);
WalletList::Entry Deserialize(const TLstorage_WalletEntry &data);
TLstorage_WalletList Serialize(const WalletList &data);
WalletList Deserialize(const TLstorage_WalletList &data);
TLstorage_TransactionId Serialize(const TransactionId &data);
TransactionId Deserialize(const TLstorage_TransactionId &data);
TLstorage_RestrictionLimit Serialize(const RestrictionLimit &data);
RestrictionLimit Deserialize(const TLstorage_RestrictionLimit &data);
TLstorage_AccountState Serialize(const AccountState & data);
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
TLstorage_Settings Serialize(const Settings &data);
Settings Deserialize(const TLstorage_Settings &data);

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

TLstorage_Bool Serialize(const bool &data) {
	return data ? make_storage_true() : make_storage_false();
}

bool Deserialize(const TLstorage_Bool &data) {
	return data.match([&](const TLDstorage_true &data) {
		return true;
	}, [&](const TLDstorage_false &data) {
		return false;
	});
}

TLstorage_WalletEntry Serialize(const WalletList::Entry &data) {
	const auto restricted = !data.restrictedInitPublicKey.isEmpty();
	return make_storage_walletEntry2(
		tl_string(data.publicKey),
		tl_bytes(data.secret),
		tl_int32(data.revision),
		tl_int32(data.workchainId),
		(restricted
			? make_storage_walletDetailsRestricted(
				tl_string(data.restrictedInitPublicKey))
			: make_storage_walletDetailsNormal()));
}

void ApplyWalletEntryDetails(
		WalletList::Entry &entry,
		const TLstorage_WalletEntryDetails &details) {
	details.match([&](const TLDstorage_walletDetailsNormal &) {
	}, [&](const TLDstorage_walletDetailsRestricted &data) {
		entry.restrictedInitPublicKey = data.vinitPublicKey().v;
	});
}

WalletList::Entry Deserialize(const TLstorage_WalletEntry &data) {
	return data.match([&](const TLDstorage_walletEntry &data) {
		return WalletList::Entry{
			.publicKey = data.vpublicKey().v,
			.secret = data.vsecret().v,
			.revision = 1,
			.workchainId = 0,
		};
	}, [&](const TLDstorage_walletEntryRestricted &data) {
		return WalletList::Entry{
			.publicKey = data.vpublicKey().v,
			.secret = data.vsecret().v,
			.restrictedInitPublicKey = data.vinitPublicKey().v,
			.revision = 1,
			.workchainId = 0,
		};
	}, [&](const TLDstorage_walletEntryGeneric &data) {
		auto result = WalletList::Entry{
			.publicKey = data.vpublicKey().v,
			.secret = data.vsecret().v,
			.revision = data.vrevision().v,
			.workchainId = 0,
		};
		ApplyWalletEntryDetails(result, data.vdetails());
		return result;
	}, [&](const TLDstorage_walletEntry2 &data) {
		auto result = WalletList::Entry{
			.publicKey = data.vpublicKey().v,
			.secret = data.vsecret().v,
			.revision = data.vrevision().v,
			.workchainId = data.vworkchainId().v,
		};
		ApplyWalletEntryDetails(result, data.vdetails());
		return result;
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

TLstorage_RestrictionLimit Serialize(const RestrictionLimit &data) {
	return make_storage_restrictionLimit(
		tl_int32(data.seconds),
		tl_int64(data.lockedAmount));
}

RestrictionLimit Deserialize(const TLstorage_RestrictionLimit &data) {
	return data.match([&](const TLDstorage_restrictionLimit &data) {
		return RestrictionLimit{
			.seconds = data.vseconds().v,
			.lockedAmount = data.vlockedAmount().v
		};
	});
}

TLstorage_AccountState Serialize(const AccountState &data) {
	const auto restricted = data.lockedBalance
		|| data.restrictionStartAt
		|| !data.restrictionLimits.empty();
	return make_storage_accountStateFull(
		tl_int64(data.fullBalance),
		tl_int64(data.syncTime),
		Serialize(data.lastTransactionId),
		(restricted
			? make_storage_accountStateRestricted(
				tl_int64(data.lockedBalance),
				tl_int64(data.restrictionStartAt),
				Serialize(data.restrictionLimits))
			: make_storage_accountStateNormal()));
}

AccountState Deserialize(const TLstorage_AccountState &data) {
	return data.match([&](const TLDstorage_accountState &data) {
		return AccountState{
			.fullBalance = data.vbalance().v,
			.syncTime = data.vsyncTime().v,
			.lastTransactionId = Deserialize(data.vlastTransactionId())
		};
	}, [&](const TLDstorage_accountStateFull &data) {
		auto result = AccountState{
			.fullBalance = data.vbalance().v,
			.syncTime = data.vsyncTime().v,
			.lastTransactionId = Deserialize(data.vlastTransactionId())
		};
		data.vdetails().match([&](const TLDstorage_accountStateNormal &) {
		}, [&](const TLDstorage_accountStateRestricted &data) {
			result.restrictionStartAt = data.vstartAt().v;
			result.lockedBalance = data.vlockedBalance().v;
			result.restrictionLimits = Deserialize(data.vlimits());
		});
		return result;
	});
}

TLstorage_MessageText Serialize(const MessageText &data) {
	return !data.encrypted.isEmpty()
		? make_storage_messageTextEncrypted(tl_bytes(data.encrypted))
		: data.decrypted
		? make_storage_messageTextDecrypted(tl_string(data.text))
		: make_storage_messageTextPlain(tl_string(data.text));
}

MessageText Deserialize(const TLstorage_MessageText &data) {
	return data.match([&](const TLDstorage_messageTextEncrypted &data) {
		return MessageText{ QString(), tl::utf8(data.vdata()) };
	}, [&](const TLDstorage_messageTextDecrypted &data) {
		return MessageText{ tl::utf16(data.vtext()), QByteArray(), true };
	}, [&](const TLDstorage_messageTextPlain &data) {
		return MessageText{ tl::utf16(data.vtext()) };
	});
}

TLstorage_Message Serialize(const Message &data) {
	return make_storage_message2(
		tl_string(data.source),
		tl_string(data.destination),
		tl_int64(data.value),
		tl_int64(data.created),
		tl_bytes(data.bodyHash),
		Serialize(data.message));
}

Message Deserialize(const TLstorage_Message &data) {
	return data.match([&](const TLDstorage_message &data) {
		return Message{
			tl::utf16(data.vsource()),
			tl::utf16(data.vdestination()),
			data.vvalue().v,
			data.vcreated().v,
			data.vbodyHash().v,
			MessageText{ tl::utf16(data.vmessage()) }
		};
	}, [&](const TLDstorage_message2 &data) {
		return Message{
			tl::utf16(data.vsource()),
			tl::utf16(data.vdestination()),
			data.vvalue().v,
			data.vcreated().v,
			data.vbodyHash().v,
			Deserialize(data.vmessage())
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
	return data.match([&](const TLDstorage_walletState &data) {
		return WalletState{
			tl::utf16(data.vaddress()),
			Deserialize(data.vaccount()),
			Deserialize(data.vlastTransactions()),
			Deserialize(data.vpendingTransactions())
		};
	});
}

TLstorage_Network Serialize(const NetSettings &data) {
	return make_storage_network(
		tl_string(data.blockchainName),
		tl_string(data.configUrl),
		tl_string(data.config),
		Serialize(data.useCustomConfig));
}

NetSettings Deserialize(const TLstorage_Network &data) {
	return data.match([&](const TLDstorage_network &data) {
		return NetSettings{
			.blockchainName = tl::utf16(data.vblockchainName()),
			.configUrl = tl::utf16(data.vconfigUrl()),
			.config = tl::utf8(data.vconfig()),
			.useCustomConfig = Deserialize(data.vuseCustomConfig())
		};
	});
}

TLstorage_Settings Serialize(const Settings &data) {
	return make_storage_settings3(
		Serialize(data.main),
		Serialize(data.test),
		Serialize(data.useTestNetwork),
		Serialize(data.useNetworkCallbacks),
		tl_int32(data.version));
}

Settings Deserialize(const TLstorage_Settings &data) {
	auto result = Settings();
	return data.match([&](const TLDstorage_settings &data) {
		return Settings{
			.test = NetSettings{
				.blockchainName = tl::utf16(data.vblockchainName()),
				.configUrl = tl::utf16(data.vconfigUrl()),
				.config = tl::utf8(data.vconfig()),
				.useCustomConfig = Deserialize(data.vuseCustomConfig())
			},
			.useTestNetwork = true,
			.useNetworkCallbacks = Deserialize(data.vuseNetworkCallbacks()),
			.version = 0
		};
	}, [&](const TLDstorage_settings2 &data) {
		return Settings{
			.test = NetSettings{
				.blockchainName = tl::utf16(data.vblockchainName()),
				.configUrl = tl::utf16(data.vconfigUrl()),
				.config = tl::utf8(data.vconfig()),
				.useCustomConfig = Deserialize(data.vuseCustomConfig())
			},
			.useTestNetwork = true,
			.useNetworkCallbacks = Deserialize(data.vuseNetworkCallbacks()),
			.version = data.vversion().v
		};
	}, [&](const TLDstorage_settings3 &data) {
		return Settings{
			.main = Deserialize(data.vmain()),
			.test = Deserialize(data.vtest()),
			.useTestNetwork = Deserialize(data.vuseTestNetwork()),
			.useNetworkCallbacks = Deserialize(data.vuseNetworkCallbacks()),
			.version = data.vversion().v
		};
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
		bool useTestNetwork,
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
		db->remove(WalletListKey(useTestNetwork), std::move(saved));
	} else {
		db->put(WalletListKey(useTestNetwork), Pack(list), std::move(saved));
	}
}

void LoadWalletList(
		not_null<Storage::Cache::Database*> db,
		bool useTestNetwork,
		Fn<void(WalletList&&)> done) {
	Expects(done != nullptr);

	db->get(WalletListKey(useTestNetwork), [=](QByteArray value) {
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

void SaveSettings(
		not_null<Storage::Cache::Database*> db,
		const Settings &settings,
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
	db->put(kSettingsKey, Pack(settings), std::move(saved));
}

void LoadSettings(
		not_null<Storage::Cache::Database*> db,
		Fn<void(Settings&&)> done) {
	Expects(done != nullptr);

	db->get(kSettingsKey, [=](QByteArray value) {
		crl::on_main([=, result = Unpack<Settings>(value)]() mutable {
			done(std::move(result));
		});
	});
}

} // namespace Ton::details
