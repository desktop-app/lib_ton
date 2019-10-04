// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_key_creator.h"

#include "ton/details/ton_request_sender.h"
#include "ton/details/ton_external.h"

namespace Ton::details {
namespace {

inline constexpr auto kLocalPasswordSize = size_type(32);

[[nodiscard]] QByteArray GenerateLocalPassword() {
	auto result = QByteArray(kLocalPasswordSize, Qt::Uninitialized);
	bytes::set_random(bytes::make_detached_span(result));
	return result;
}

} // namespace

KeyCreator::KeyCreator(
	not_null<RequestSender*> lib,
	Fn<void(Result<std::vector<QString>>)> done)
: _lib(lib)
, _password(GenerateLocalPassword()) {
	_lib->request(TLCreateNewKey(
		TLsecureString{ _password },
		TLsecureString(),
		TLsecureString()
	)).done(crl::guard(this, [=](const TLKey &key) {
		key.match([&](const TLDkey &data) {
			_key = data.vpublic_key().v;
			_secret = data.vsecret().v;
		});
		exportWords(done);
	})).fail(crl::guard(this, [=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	})).send();
}

void KeyCreator::exportWords(
		Fn<void(Result<std::vector<QString>>)> done) {
	Expects(_state == State::Creating);
	Expects(!_key.isEmpty());
	Expects(!_secret.isEmpty());

	_lib->request(TLExportKey(
		make_inputKey(
			make_key(
				tl::make_bytes(_key),
				TLsecureBytes{ _secret }),
			TLsecureBytes{ _password })
	)).done(crl::guard(this, [=](const TLExportedKey &result) {
		auto list = result.match([&](const TLDexportedKey &data) {
			return ranges::view::all(
				data.vword_list().v
			) | ranges::view::transform([](const TLsecureString &data) {
				return tl::utf16(data.v);
			}) | ranges::to_vector;
		});
		_state = State::Created;
		InvokeCallback(done, std::move(list));
	})).fail(crl::guard(this, [=](const TLError &error) {
		deleteFromLibrary(_key, _secret);
		InvokeCallback(done, ErrorFromLib(error));
	})).send();
}

void KeyCreator::deleteFromLibrary(
		const QByteArray &publicKey,
		const QByteArray &secret,
		Callback<> done) {
	_lib->request(TLDeleteKey(
		make_key(tl::make_bytes(publicKey), TLsecureBytes{ secret })
	)).done(crl::guard(this, [=] {
		InvokeCallback(done);
	})).fail(crl::guard(this, [=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	})).send();
}

void KeyCreator::save(
		const QByteArray &password,
		const WalletList &existing,
		Fn<void(WalletList, Callback<>)> saveList,
		Callback<WalletList::Entry> done) {
	if (_password != password) {
		changePassword(password, [=](Result<> result) {
			_state = State::Created;
			if (!result) {
				InvokeCallback(done, result.error());
				return;
			}
			saveToDatabase(existing, saveList, done);
		});
	} else {
		saveToDatabase(existing, std::move(saveList), std::move(done));
	}
}

void KeyCreator::saveToDatabase(
		WalletList existing,
		Fn<void(WalletList, Callback<>)> saveList,
		Callback<WalletList::Entry> done) {
	Expects(_state == State::Created);
	Expects(!_key.isEmpty());
	Expects(!_secret.isEmpty());

	_state = State::Saving;
	const auto added = WalletList::Entry{ _key, _secret };
	existing.entries.push_back(added);
	saveList(std::move(existing), crl::guard(this, [=](Result<> result) {
		if (!result) {
			_state = State::Created;
			InvokeCallback(done, result.error());
		} else {
			InvokeCallback(done, added);
		}
	}));
}

void KeyCreator::changePassword(
		const QByteArray &password,
		Callback<> done) {
	Expects(_state == State::Created);
	Expects(!_key.isEmpty());
	Expects(!_secret.isEmpty());
	Expects(_password != password);

	_state = State::ChangingPassword;
	_lib->request(TLChangeLocalPassword(
		make_inputKey(
			make_key(
				tl::make_bytes(_key),
				TLsecureBytes{ _secret }),
			TLsecureBytes{ _password }),
		TLsecureBytes{ password }
	)).done(crl::guard(this, [=](const TLKey &result) {
		deleteFromLibrary(_key, _secret);
		result.match([&](const TLDkey &data) {
			_password = password;
			_secret = data.vsecret().v;
			InvokeCallback(done);
		});
	})).fail(crl::guard(this, [=](const TLError &error) {
		InvokeCallback(done, ErrorFromLib(error));
	})).send();
}

} // namespace Ton::details
