// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/ton_utility.h"

#include "base/assertion.h"
#include "base/flat_map.h"
#include "base/overload.h"
#include "ton_tl.h"
#include "ton/details/ton_request_sender.h"

#include <crl/crl_on_main.h>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QFile>
#include <thread>

#undef signals

#include <auto/tl/tonlib_api.hpp>
#include <auto/tl/tonlib_api.h>
#include <tonlib/Client.h>

#include "base/openssl_help.h"

namespace Ton {
namespace {

using namespace details;

constexpr auto kLocalErrorCode = -666;

std::unique_ptr<RequestSender> GlobalSender;

TLsecureString LocalPassword();
TLsecureString MnemonicPassword();

TLsecureString LocalPassword() {
	static constexpr auto kLocalPasswordSize = 256;
	using array = std::array<char, kLocalPasswordSize>;
	static auto kLocalPassword = openssl::RandomValue<array>();
	return TLsecureString{ QByteArray::fromRawData(
		kLocalPassword.data(),
		kLocalPassword.size()) };
}

TLsecureString MnemonicPassword() {
	return TLsecureString{ QByteArray() };
}

template <typename T>
Fn<void(const TLerror &)> ErrorHandler(Callback<T> handler) {
	return [=](const TLerror &error) {
		handler(ErrorFromLib(error));
	};
}

} // namespace

void Start(Callback<> done) {
	Expects(!GlobalSender);

	const auto ok1 = RequestSender::Execute(TLSetLogStream(
		tl_logStreamEmpty()));
	Assert(ok1);

	const auto ok2 = RequestSender::Execute(TLSetLogVerbosityLevel(
		tl_int32(0)));
	Assert(ok2);

	GlobalSender = std::make_unique<RequestSender>();
	GlobalSender->request(TLInit(
		tl_options(
			nullptr,
			tl_keyStoreTypeInMemory())
	)).done([=] {
		done({});
	}).fail(ErrorHandler(done)).send();
}

void CreateKey(
		const QByteArray &seed,
		Callback<UtilityKey> done) {
	Expects(GlobalSender != nullptr);

	const auto deleteAfterCreate = [=](
			QByteArray secret,
			const UtilityKey &key) {
		GlobalSender->request(TLDeleteKey(
			tl_key(tl_string(key.publicKey), TLsecureString{ secret })
		)).done([=] {
			done(key);
		}).fail([=] {
			done(key);
		}).send();
	};
	GlobalSender->request(TLCreateNewKey(
		LocalPassword(),
		MnemonicPassword(),
		TLsecureString{ seed }
	)).done([=](const TLkey &result) {
		result.match([=](const TLDkey &result) {
			const auto publicKey = result.vpublic_key().v;
			const auto secret = result.vsecret().v;
			GlobalSender->request(TLExportKey(
				tl_inputKey(
					tl_key(tl_string(publicKey), TLsecureString{ secret }),
					LocalPassword())
			)).done([=](const TLexportedKey &result) {
				result.match([&](const TLDexportedKey &data) {
					auto key = UtilityKey();
					key.publicKey = publicKey;
					key.words.reserve(data.vword_list().v.size());
					for (const auto &word : data.vword_list().v) {
						key.words.push_back(word.v);
					}
					deleteAfterCreate(secret, key);
				});
			}).fail(ErrorHandler(done)).send();
		});
	}).fail(ErrorHandler(done)).send();
}

void CheckKey(
		const std::vector<QByteArray> &words,
		Callback<QByteArray> done) {
	Expects(GlobalSender != nullptr);

	auto wrapped = QVector<TLsecureString>();
	for (const auto &word : words) {
		wrapped.push_back(TLsecureString{ word });
	}

	GlobalSender->request(TLImportKey(
		LocalPassword(),
		MnemonicPassword(),
		tl_exportedKey(tl_vector<TLsecureString>(std::move(wrapped)))
	)).done([=](const TLkey &result) {
		result.match([=](const TLDkey &result) {
			const auto publicKey = result.vpublic_key().v;
			const auto secret = result.vsecret().v;
			GlobalSender->request(TLDeleteKey(
				tl_key(tl_string(publicKey), TLsecureString{ secret })
			)).done([=] {
				done(publicKey);
			}).fail([=] {
				done(publicKey);
			}).send();
		});
	}).fail(ErrorHandler(done)).send();
}

void Finish() {
	Expects(GlobalSender != nullptr);

	GlobalSender = nullptr;
}

} // namespace Ton
