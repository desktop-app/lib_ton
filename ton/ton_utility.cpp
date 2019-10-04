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
#include "ton/ton_request_sender.h"

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

Fn<void(const TLerror &)> ErrorHandler(Fn<void(Error)> handler) {
	return [=](const TLerror &error) {
		error.match([&](const TLDerror &data) {
			const auto text = "TONLIB_ERROR_"
				+ QString::number(data.vcode().v)
				+ ": "
				+ tl::utf16(data.vmessage());
			handler(Error{ text });
		});
	};
}

} // namespace

void Start(Fn<void()> done, Fn<void(Error)> error) {
	if (GlobalSender) {
		error(Error{ "TON_INSTANCE_ALREADY_STARTED" });
		return;
	}
	GlobalSender = std::make_unique<RequestSender>();
	GlobalSender->request(TLSetLogStream(
		make_logStreamEmpty()
	)).done([=] {
		GlobalSender->request(TLSetLogVerbosityLevel(
			tl::make_int(0)
		)).done([=] {
			GlobalSender->request(TLInit(
				make_options(
					nullptr,
					make_keyStoreTypeInMemory())
			)).done(
				done
			).fail(ErrorHandler(error)).send();
		}).fail(ErrorHandler(error)).send();
	}).fail(ErrorHandler(error)).send();
}

void GetValidWords(
		Fn<void(std::vector<QByteArray>)> done,
		Fn<void(Error)> error) {
	if (!GlobalSender) {
		error(Error{ "TON_INSTANCE_NOT_STARTED" });
		return;
	}

	GlobalSender->request(TLGetBip39Hints(
		tl::make_string()
	)).done([=](const TLbip39Hints &result) {
		result.match([&](const TLDbip39Hints &data) {
			auto list = std::vector<QByteArray>();
			list.reserve(data.vwords().v.size());
			for (const auto &word : data.vwords().v) {
				list.push_back(word.v);
			}
			done(std::move(list));
		});
	}).fail(ErrorHandler(error)).send();
}

void CreateKey(
		const QByteArray &seed,
		Fn<void(Key)> done,
		Fn<void(Error)> error) {
	if (!GlobalSender) {
		error(Error{ "TON_INSTANCE_NOT_STARTED" });
		return;
	}

	const auto deleteAfterCreate = [=](QByteArray secret, const Key &key) {
		GlobalSender->request(TLDeleteKey(
			make_key(
				tl::make_bytes(key.publicKey),
				TLsecureString{ secret })
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
				make_inputKey(
					make_key(
						tl::make_bytes(publicKey),
						TLsecureString{ secret }),
					LocalPassword())
			)).done([=](const TLexportedKey &result) {
				result.match([&](const TLDexportedKey &data) {
					auto key = Key();
					key.publicKey = publicKey;
					key.words.reserve(data.vword_list().v.size());
					for (const auto &word : data.vword_list().v) {
						key.words.push_back(word.v);
					}
					deleteAfterCreate(secret, key);
				});
			}).fail(ErrorHandler(error)).send();
		});
	}).fail(ErrorHandler(error)).send();
}

void CheckKey(
		const std::vector<QByteArray> &words,
		Fn<void(QByteArray)> done,
		Fn<void(Error)> error) {
	auto wrapped = QVector<TLsecureString>();
	for (const auto &word : words) {
		wrapped.push_back(TLsecureString{ word });
	}

	GlobalSender->request(TLImportKey(
		LocalPassword(),
		MnemonicPassword(),
		make_exportedKey(tl::make_vector<TLsecureString>(
			std::move(wrapped)))
	)).done([=](const TLkey &result) {
		result.match([=](const TLDkey &result) {
			const auto publicKey = result.vpublic_key().v;
			const auto secret = result.vsecret().v;
			GlobalSender->request(TLDeleteKey(
				make_key(
					tl::make_bytes(publicKey),
					TLsecureString{ secret })
			)).done([=] {
				done(publicKey);
			}).fail([=] {
				done(publicKey);
			}).send();
		});
	}).fail(ErrorHandler(error)).send();
}

void Finish() {
	GlobalSender = nullptr;
}

} // namespace Ton
