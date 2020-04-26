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
#include "base/timer.h"

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
class WebLoader;
class LocalTimeSyncer;
struct BlockchainTime;
class TLerror;
class TLinputKey;
class TLinitialAccountState;
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
	[[nodiscard]] QString getUsedAddress(const QByteArray &publicKey) const;
	void checkConfig(const QByteArray &config, Callback<> done);

	void sync();

	[[nodiscard]] const Settings &settings() const;
	void updateSettings(Settings settings, Callback<> done);

	[[nodiscard]] rpl::producer<Update> updates() const;

	[[nodiscard]] std::vector<QByteArray> publicKeys() const;

	void createKey(Callback<std::vector<QString>> done);
	void importKey(const std::vector<QString> &words, Callback<> done);
	void queryRestrictedInitPublicKey(Callback<QByteArray> done);
	void saveKey(
		const QByteArray &password,
		const QByteArray &restrictedInitPublicKey,
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
	[[nodiscard]] static bool IsIncorrectPasswordError(const Error &error);

	[[nodiscard]] std::unique_ptr<AccountViewer> createAccountViewer(
		const QByteArray &publicKey,
		const QString &address);
	void updateViewersPassword(
		const QByteArray &publicKey,
		const QByteArray &password);

	void loadWebResource(const QString &url, Callback<QByteArray> done);

	void decrypt(
		const QByteArray &publicKey,
		std::vector<Transaction> &&list,
		Callback<std::vector<Transaction>> done);
	void trySilentDecrypt(
		const QByteArray &publicKey,
		std::vector<Transaction> &&list,
		Callback<std::vector<Transaction>> done);

	// Internal API.
	void requestState(const QString &address, Callback<AccountState> done);
	void requestTransactions(
		const QByteArray &publicKey,
		const QString &address,
		const TransactionId &lastId,
		Callback<TransactionsSlice> done);

private:
	struct ViewersPassword {
		QByteArray bytes;
		int generation = 1;
		crl::time expires = 0;
	};
	void setWalletList(const details::WalletList &list);
	[[nodiscard]] details::TLinputKey prepareInputKey(
		const QByteArray &publicKey,
		const QByteArray &password) const;
	[[nodiscard]] Fn<void(Update)> generateUpdatesCallback();
	void checkLocalTime(details::BlockchainTime time);
	void notifyPasswordGood(const QByteArray &publicKey, int generation);
	void checkPasswordsExpiration();

	[[nodiscard]] details::TLinitialAccountState getUsedInitialAccountState(
		const QByteArray &publicKey) const;
	[[nodiscard]] QString getUsedAddress(
		const details::TLinitialAccountState &state) const;

	void handleInputKeyError(
		const QByteArray &publicKey,
		int generation,
		const details::TLerror &error,
		Callback<> done);

	std::optional<int64> _walletId;
	QByteArray _restrictedInitPublicKey;
	rpl::event_stream<Update> _updates;
	SyncState _lastSyncStateUpdate;
	bool _switchedToMain = false;

	const std::unique_ptr<details::External> _external;
	const std::unique_ptr<details::AccountViewers> _accountViewers;
	const std::unique_ptr<details::WalletList> _list;
	std::unique_ptr<details::WebLoader> _webLoader;
	std::unique_ptr<details::KeyCreator> _keyCreator;
	std::unique_ptr<details::KeyDestroyer> _keyDestroyer;
	std::unique_ptr<details::PasswordChanger> _passwordChanger;
	std::unique_ptr<details::LocalTimeSyncer> _localTimeSyncer;

	base::flat_map<QByteArray, ViewersPassword> _viewersPasswords;
	base::flat_map<
		QByteArray,
		std::vector<Callback<>>> _viewersPasswordsWaiters;
	base::Timer _viewersPasswordsExpireTimer;

	rpl::lifetime _lifetime;

};

} // namespace Ton
