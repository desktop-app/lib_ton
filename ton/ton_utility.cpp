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
#include "ton_tl_conversion.h"

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

namespace api = tonlib_api;
class Client;

std::unique_ptr<Client> GlobalClient;

class Client {
public:
	Client();
	~Client();

	void send(
		api::object_ptr<api::Function> request,
		FnMut<void(api::object_ptr<api::Object>)> handler);

	TLsecureString localPassword() const;
	TLsecureString mnemonicPassword() const;

private:
	void check();

	tonlib::Client _api;
	uint64 _requestIdAutoIncrement = 0;
	QMutex _mutex;
	base::flat_map<
		uint64,
		FnMut<void(api::object_ptr<api::Object>)>> _handlers;

	std::thread _thread;
	std::atomic<bool> _finished = false;

};

Client::Client() : _thread([=] { check(); }) {
}

Client::~Client() {
	_finished = true;
	send(tl_to(TLclose()), [](auto&&) {});
	_thread.join();

	for (auto &[id, handler] : _handlers) {
		handler(tl_to(make_error(
			tl::make_int(kLocalErrorCode),
			tl::make_string("TON_INSTANCE_DESTROYED"))));
	}
}

void Client::send(
		api::object_ptr<api::Function> request,
		FnMut<void(api::object_ptr<api::Object>)> handler) {
	const auto requestId = ++_requestIdAutoIncrement;

	QMutexLocker lock(&_mutex);
	_handlers.emplace(requestId, std::move(handler));
	lock.unlock();

	_api.send({ requestId, std::move(request) });
}

TLsecureString Client::localPassword() const {
	static constexpr auto kLocalPasswordSize = 256;
	using array = std::array<char, kLocalPasswordSize>;
	static auto kLocalPassword = openssl::RandomValue<array>();
	return TLsecureString{ QByteArray::fromRawData(
		kLocalPassword.data(),
		kLocalPassword.size()) };
}

TLsecureString Client::mnemonicPassword() const {
	return TLsecureString{ QByteArray() };
}

void Client::check() {
	while (!_finished) {
		auto response = _api.receive(60.);
		if (!response.object) {
			continue;
		}
		QMutexLocker lock(&_mutex);
		auto handler = _handlers.take(response.id);
		lock.unlock();

		if (handler) {
			crl::on_main([
				handler = std::move(*handler),
				response = std::move(response.object)
			]() mutable {
				handler(std::move(response));
			});
		}
	}
}

template <typename Request>
void Send(
		const Request &request,
		FnMut<void(typename Request::ResponseType)> done,
		FnMut<void(const TLerror &)> error) {
	if (!GlobalClient) {
		error(make_error(
			tl::make_int(kLocalErrorCode),
			tl::make_string("TON_INSTANCE_NOT_STARTED")));
		return;
	}
	auto converted = tl_to(request);
	using RequestPointer = std::decay_t<decltype(converted)>;
	using ReturnType = typename RequestPointer::element_type::ReturnType::element_type;
	auto handler = [done = std::move(done), error = std::move(error)](
			api::object_ptr<api::Object> response) mutable {
		if (response->get_id() == api::error::ID) {
			error(tl_from(
				api::move_object_as<api::error>(std::move(response))));
		} else {
			done(tl_from(
				api::move_object_as<ReturnType>(std::move(response))));
		}
	};
	GlobalClient->send(std::move(converted), std::move(handler));
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
	if (GlobalClient) {
		error(Error{ "TON_INSTANCE_ALREADY_STARTED" });
		return;
	}
	GlobalClient = std::make_unique<Client>();
	const auto init = [=](const TLok &) {
		Send(
			TLinit(
				make_options(
					nullptr,
					make_keyStoreTypeInMemory())),
			[=](const TLok &) { done(); },
			ErrorHandler(error));
	};
	const auto setVerbosity = [=](const TLok &) {
		Send(
			TLSetLogVerbosityLevel(tl::make_int(0)),
			init,
			ErrorHandler(error));
	};
	Send(
		TLsetLogStream(make_logStreamEmpty()),
		setVerbosity,
		ErrorHandler(error));
}

void GetValidWords(
		Fn<void(std::vector<QByteArray>)> done,
		Fn<void(Error)> error) {
	const auto got = [=](const TLbip39Hints &result) {
		result.match([&](const TLDbip39Hints &data) {
			auto list = std::vector<QByteArray>();
			list.reserve(data.vwords().v.size());
			for (const auto &word : data.vwords().v) {
				list.push_back(word.v);
			}
			done(std::move(list));
		});
	};
	Send(
		TLgetBip39Hints(tl::make_string()),
		got,
		ErrorHandler(error));
}

void CreateKey(
		const QByteArray &seed,
		Fn<void(Key)> done,
		Fn<void(Error)> error) {

	if (!GlobalClient) {
		error(Error{ "TON_INSTANCE_NOT_STARTED" });
		return;
	}
	const auto deleteAfterCreate = [=](const Key &key) {
		Send(
			TLdeleteKey(make_key(
				tl::make_bytes(key.publicKey),
				TLsecureString{ key.secret })),
			[=](const TLok &) { done(key); },
			[=](const TLerror &) { done(key); });
	};
	const auto created = [=](const TLDkey &result) {
		const auto publicKey = result.vpublic_key().v;
		const auto secret = result.vsecret().v;
		const auto exported = [=](const TLexportedKey &result) {
			result.match([&](const TLDexportedKey &data) {
				auto key = Key();
				key.publicKey = publicKey;
				key.secret = secret;
				key.words.reserve(data.vword_list().v.size());
				for (const auto &word : data.vword_list().v) {
					key.words.push_back(word.v);
				}
				deleteAfterCreate(key);
			});
		};
		Send(
			TLexportKey(make_inputKey(
				make_key(
					tl::make_bytes(publicKey),
					TLsecureString{ secret }),
				GlobalClient->localPassword())),
			exported,
			ErrorHandler(error));
	};
	const auto createdWrap = [=](const TLkey &result) {
		result.match(created);
	};
	Send(
		TLcreateNewKey(
			GlobalClient->localPassword(),
			GlobalClient->mnemonicPassword(),
			TLsecureString{ seed }),
		createdWrap,
		ErrorHandler(error));
}

void CheckKey(
		const Key &key,
		Fn<void()> done,
		Fn<void(Error)> error) {
	const auto publicKey = key.publicKey;
	auto words = QVector<TLsecureString>();
	for (const auto &word : key.words) {
		words.push_back(TLsecureString{ word });
	}

	const auto imported = [=](const TLDkey &result) {
		if (publicKey != result.vpublic_key().v) {
			error(Error{ "DIFFERENT_KEY" });
			return;
		}
		const auto secret = result.vsecret().v;
		Send(
			TLdeleteKey(make_key(
				tl::make_bytes(publicKey),
				TLsecureString{ secret })),
			[=](const TLok &) { done(); },
			[=](const TLerror &) { done(); });
	};
	const auto importedWrap = [=](const TLkey &result) {
		result.match(imported);
	};

	Send(
		TLimportKey(
			GlobalClient->localPassword(),
			GlobalClient->mnemonicPassword(),
			make_exportedKey(tl::make_vector<TLsecureString>(
				std::move(words)))),
		importedWrap,
		ErrorHandler(error));
}

void Finish() {
	GlobalClient = nullptr;
}

} // namespace Ton
