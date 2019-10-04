// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_result.h"

namespace Storage::Cache {
class Database;
} // namespace Storage::Cache

namespace Ton {
namespace details {
struct WalletList;
class External;
class KeyCreator;
class KeyDestroyer;
} // namespace details

class Wallet final {
public:
	explicit Wallet(const QString &path);
	~Wallet();

	void open(const QByteArray &globalPassword, Callback<> done);

	const std::vector<QByteArray> &publicKeys() const;

	void createKey(Fn<void(Result<std::vector<QString>>)> done);
	void saveKey(
		const QByteArray &password,
		Callback<QByteArray> done);
	void deleteKey(const QByteArray &publicKey, Callback<> done);

private:
	void setWalletList(const details::WalletList &list);
	[[nodiscard]] details::WalletList collectWalletList() const;

	const std::unique_ptr<details::External> _external;
	std::unique_ptr<details::KeyCreator> _keyCreator;
	std::unique_ptr<details::KeyDestroyer> _keyDestroyer;

	std::vector<QByteArray> _publicKeys;
	std::vector<QByteArray> _secrets;

};

} // namespace Ton
