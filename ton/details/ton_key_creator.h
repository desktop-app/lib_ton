// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/details/ton_external.h"

namespace Ton::details {

class RequestSender;
struct WalletList;

class KeyCreator final : public base::has_weak_ptr {
public:
	KeyCreator(
		not_null<RequestSender*> lib,
		Fn<void(Result<std::vector<QString>>)> done);

	void save(
		const QByteArray &password,
		const WalletList &existing,
		Fn<void(WalletList, Callback<>)> saveList,
		Callback<WalletList::Entry> done);

private:
	enum class State {
		Creating,
		Created,
		ChangingPassword,
		Saving,
	};

	void exportWords(Fn<void(Result<std::vector<QString>>)> done);
	void changePassword(const QByteArray &password, Callback<> done);
	void saveToDatabase(
		WalletList existing,
		Fn<void(WalletList, Callback<>)> saveList,
		Callback<WalletList::Entry> done);

	const not_null<RequestSender*> _lib;

	State _state = State::Creating;
	QByteArray _key;
	QByteArray _secret;
	QByteArray _password;

};

} // namespace Ton::details
