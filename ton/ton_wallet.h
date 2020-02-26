// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_result.h"
#include "ton/ton_state.h"
#include "base/weak_ptr.h"

namespace Storage::Cache {
class Database;
} // namespace Storage::Cache

namespace Ton {
namespace details {
struct WalletList;
class External;
class KeyCreator;
class KeyDestroyer;
class PasswordChanger;
class AccountViewers;
class TLinputKey;
class WebLoader;
class LocalTimeSyncer;
struct BlockchainTime;
} // namespace details

struct Settings;
class AccountViewer;

class Wallet final : public base::has_weak_ptr {
public:
	explicit Wallet(const QString &path);
	~Wallet();

	void open(
		const QByteArray &globalPassword,
		const Settings &defaultSettings,
		Callback<> done);
	void start(Callback<> done);
	[[nodiscard]] QString getAddress(const QByteArray &publicKey) const;
	void checkConfig(const QByteArray &config, Callback<> done);

	[[nodiscard]] const Settings &settings() const;
	void updateSettings(const Settings &settings, Callback<> done);

	[[nodiscard]] rpl::producer<Update> updates() const;

	[[nodiscard]] const std::vector<QByteArray> &publicKeys() const;

	void createKey(Callback<std::vector<QString>> done);
	void importKey(const std::vector<QString> &words, Callback<> done);
	void saveKey(
		const QByteArray &password,
		Callback<QByteArray> done);
	void exportKey(
		const QByteArray &publicKey,
		const QByteArray &password,
		Callback<std::vector<QString>> done);
	void deleteKey(const QByteArray &publicKey, Callback<> done);
	void deleteAllKeys(Callback<> done);
	void changePassword(
		const QByteArray &oldPassword,
		const QByteArray &newPassword,
		Callback<> done);

	void checkSendGrams(
		const QByteArray &publicKey,
		const TransactionToSend &transaction,
		Callback<TransactionCheckResult> done);
	void sendGrams(
		const QByteArray &publicKey,
		const QByteArray &password,
		const TransactionToSend &transaction,
		Callback<PendingTransaction> ready,
		Callback<> done);

	static void EnableLogging(bool enabled, const QString &basePath);
	static void LogMessage(const QString &message);
	[[nodiscard]] static bool CheckAddress(const QString &address);
	[[nodiscard]] static base::flat_set<QString> GetValidWords();

	void requestState(const QString &address, Callback<AccountState> done);
	void requestTransactions(
		const QString &address,
		const TransactionId &lastId,
		Callback<TransactionsSlice> done);

	[[nodiscard]] std::unique_ptr<AccountViewer> createAccountViewer(
		const QString &address);

	void loadWebResource(const QString &url, Callback<QByteArray> done);

private:
	void setWalletList(const details::WalletList &list);
	[[nodiscard]] details::WalletList collectWalletList() const;
	[[nodiscard]] details::TLinputKey prepareInputKey(
		const QByteArray &publicKey,
		const QByteArray &password) const;
	[[nodiscard]] Fn<void(Update)> generateUpdatesCallback();
	void checkLocalTime(details::BlockchainTime time);

	std::optional<int64> _walletId;
	rpl::event_stream<Update> _updates;
	SyncState _lastSyncStateUpdate;

	const std::unique_ptr<details::External> _external;
	const std::unique_ptr<details::AccountViewers> _accountViewers;
	std::unique_ptr<details::WebLoader> _webLoader;
	std::unique_ptr<details::KeyCreator> _keyCreator;
	std::unique_ptr<details::KeyDestroyer> _keyDestroyer;
	std::unique_ptr<details::PasswordChanger> _passwordChanger;
	std::unique_ptr<details::LocalTimeSyncer> _localTimeSyncer;

	std::vector<QByteArray> _publicKeys;
	std::vector<QByteArray> _secrets;

	rpl::lifetime _lifetime;

};

} // namespace Ton
