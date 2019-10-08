// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_password_changer.h"

#include "ton/details/ton_external.h"

namespace Ton::details {

PasswordChanger::PasswordChanger(
	not_null<RequestSender*> lib,
	const QByteArray &oldPassword,
	const QByteArray &newPassword,
	WalletList &&existing,
	Fn<void(WalletList, Callback<>)> saveList,
	Callback<std::vector<QByteArray>> done)
: _lib(lib)
, _oldPassword(oldPassword)
, _newPassword(newPassword)
, _saveList(std::move(saveList))
, _done(std::move(done))
, _list(std::move(existing)) {
	changeNext();
}

void PasswordChanger::changeNext() {
	Expects(_newSecrets.size() < _list.entries.size());

	const auto index = _newSecrets.size();
	_lib->request(TLChangeLocalPassword(
		make_inputKey(
			make_key(
				tl::make_bytes(_list.entries[index].publicKey),
				TLsecureBytes{ _list.entries[index].secret }),
			TLsecureBytes{ _oldPassword }),
		TLsecureBytes{ _newPassword }
	)).done([=](const TLKey &result) {
		savedNext(result.match([&](const TLDkey &data) {
			return data.vsecret().v;
		}));
	}).fail([=](const TLError &error) {
		rollback(ErrorFromLib(error));
	}).send();
}

void PasswordChanger::savedNext(const QByteArray &newSecret) {
	_newSecrets.push_back(newSecret);
	if (_newSecrets.size() < _list.entries.size()) {
		changeNext();
	} else {
		auto copy = _list;
		for (auto i = 0, count = int(_newSecrets.size()); i != count; ++i) {
			copy.entries[i].secret = _newSecrets[i];
		}
		_saveList(std::move(copy), [=](Result<> result) {
			if (!result) {
				rollback(result.error());
			} else {
				rollforward();
			}
		});
	}
}

void PasswordChanger::rollback(Error error) {
	if (_newSecrets.empty()) {
		InvokeCallback(_done, error);
		return;
	}
	const auto newSecret = _newSecrets.back();
	_newSecrets.pop_back();
	const auto key = _list.entries[_newSecrets.size()].publicKey;
	DeleteKeyFromLibrary(
		_lib,
		key,
		newSecret,
		crl::guard(this, [=](Result<>) { rollback(error); }));
}

void PasswordChanger::rollforward() {
	if (_list.entries.empty()) {
		InvokeCallback(_done, std::move(_newSecrets));
		return;
	}
	const auto oldEntry = _list.entries.back();
	_list.entries.pop_back();
	DeleteKeyFromLibrary(
		_lib,
		oldEntry.publicKey,
		oldEntry.secret,
		crl::guard(this, [=](Result<>) { rollforward(); }));
}

} // namespace Ton::details
