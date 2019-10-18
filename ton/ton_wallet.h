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
} // namespace details

struct Config;
class AccountViewer;

class Wallet final : public base::has_weak_ptr {
public:
	explicit Wallet(const QString &path);
	~Wallet();

	void open(
		const QByteArray &globalPassword,
		const Config &config,
		Callback<> done);
	void setConfig(const Config &config, Callback<> done);

	const std::vector<QByteArray> &publicKeys() const;

	void createKey(Callback<std::vector<QString>> done);
	void saveKey(
		const QByteArray &password,
		Callback<QByteArray> done);
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

	[[nodiscard]] static QString GetAddress(const QByteArray &publicKey);
	[[nodiscard]] static bool CheckAddress(const QString &address);
	[[nodiscard]] static base::flat_set<QString> GetValidWords();

	void requestState(const QString &address, Callback<AccountState> done);
	void requestTransactions(
		const QString &address,
		const TransactionId &lastId,
		Callback<TransactionsSlice> done);

	[[nodiscard]] std::unique_ptr<AccountViewer> createAccountViewer(
		const QString &address);

private:
	void setWalletList(const details::WalletList &list);
	[[nodiscard]] details::WalletList collectWalletList() const;

	const std::unique_ptr<details::External> _external;
	const std::unique_ptr<details::AccountViewers> _accountViewers;
	std::unique_ptr<details::KeyCreator> _keyCreator;
	std::unique_ptr<details::KeyDestroyer> _keyDestroyer;
	std::unique_ptr<details::PasswordChanger> _passwordChanger;

	std::vector<QByteArray> _publicKeys;
	std::vector<QByteArray> _secrets;

};

} // namespace Ton
