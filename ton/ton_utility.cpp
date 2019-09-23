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

#include <crl/crl_on_main.h>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QFile>
#include <thread>

#undef signals

#include <auto/tl/tonlib_api.hpp>
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

	td::SecureString localPassword() const;
	td::SecureString mnemonicPassword() const;

private:
	void check();

	tonlib::Client _api;
	uint64 _requestIdAutoIncrement = 0;
	QMutex _mutex;
	base::flat_map<
		uint64,
		FnMut<void(api::object_ptr<api::Object>)>> _handlers;
	QString _path;

	std::thread _thread;
	std::atomic<bool> _finished = false;

};

template <typename Type, typename ...Args>
auto Make(Args &&...args) {
	return api::make_object<Type>(std::forward<Args>(args)...);
}

template <typename Converted>
api::object_ptr<Converted> TakeAs(api::object_ptr<api::Object> &object) {
	return api::move_object_as<Converted>(std::move(object));
}

Error WrapError(std::string message) {
	return Error{ QString::fromStdString(message) };
}

template <typename Type, typename ...Handlers>
auto Match(Type &&value, Handlers &&...handlers) {
	return api::downcast_call(
		value,
		base::overload(std::forward<Handlers>(handlers)...));
}

Client::Client() : _thread([=] { check(); }) {
}

Client::~Client() {
	_finished = true;
	send(Make<api::close>(), [](auto&&) {});
	_thread.join();

	for (auto &[id, handler] : _handlers) {
		handler(Make<api::error>(kLocalErrorCode, "TON_INSTANCE_DESTROYED"));
	}
}

void Client::send(
		api::object_ptr<api::Function> request,
		FnMut<void(api::object_ptr<api::Object>)> handler) {
	const auto requestId = ++_requestIdAutoIncrement;

	if (request->get_id() == api::init::ID) {
		const auto init = static_cast<api::init*>(request.get());
		const auto &folder = init->options_->keystore_directory_;
		_path = QString::fromUtf8(folder.data(), folder.size());
	}

	QMutexLocker lock(&_mutex);
	_handlers.emplace(requestId, std::move(handler));
	lock.unlock();

	_api.send({ requestId, std::move(request) });
}

td::SecureString Client::localPassword() const {
	static constexpr auto kLocalPasswordSize = 256;
	using array = std::array<char, kLocalPasswordSize>;
	static auto kLocalPassword = openssl::RandomValue<array>();
	return td::SecureString{ kLocalPassword.data(), kLocalPassword.size() };
}

td::SecureString Client::mnemonicPassword() const {
	return td::SecureString();
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
		api::object_ptr<Request> request,
		FnMut<void(typename Request::ReturnType)> done,
		FnMut<void(api::object_ptr<api::error>)> error) {
	using ReturnType = typename Request::ReturnType::element_type;
	if (!GlobalClient) {
		error(Make<api::error>(kLocalErrorCode, "TON_INSTANCE_NOT_STARTED"));
		return;
	}
	auto handler = [done = std::move(done), error = std::move(error)](
			api::object_ptr<api::Object> response) mutable {
		if (response->get_id() == api::error::ID) {
			error(TakeAs<api::error>(response));
		} else {
			done(TakeAs<ReturnType>(response));
		}
	};
	GlobalClient->send(std::move(request), std::move(handler));
}

Fn<void(api::object_ptr<api::error>)> ErrorHandler(Fn<void(Error)> handler) {
	return [=](api::object_ptr<api::error> error) {
		const auto text = "TONLIB_ERROR_"
			+ QString::number(error->code_).toStdString()
			+ ": "
			+ error->message_;
		handler(WrapError(text));
	};
}

} // namespace

void Start(
		const QString &path,
		Fn<void()> done,
		Fn<void(Error)> error) {
	if (GlobalClient) {
		error(WrapError("TON_INSTANCE_ALREADY_STARTED"));
		return;
	}
	GlobalClient = std::make_unique<Client>();

	Send(
		Make<api::init>(
			Make<api::options>(
				std::string(),
				path.toUtf8().toStdString(),
				false)),
		[=](api::object_ptr<api::ok>) { done(); },
		ErrorHandler(error));
}

void GetValidWords(
		Fn<void(std::vector<QByteArray>)> done,
		Fn<void(Error)> error) {
	const auto got = [=](api::object_ptr<api::bip39Hints> hints) {
		auto result = std::vector<QByteArray>();
		result.reserve(hints->words_.size());
		for (const auto &word : hints->words_) {
			result.push_back(QByteArray::fromStdString(word));
		}
		done(std::move(result));
	};
	Send(
		Make<api::getBip39Hints>(std::string()),
		got,
		ErrorHandler(error));
}

void CreateKey(
		const QByteArray &seed,
		Fn<void(Key)> done,
		Fn<void(Error)> error) {
	if (!GlobalClient) {
		error(WrapError("TON_INSTANCE_NOT_STARTED"));
		return;
	}
	const auto created = [=](api::object_ptr<api::key> key) {
		const auto publicKey = QByteArray::fromStdString(key->public_key_);
		const auto exported = [=](api::object_ptr<api::exportedKey> result) {
			auto key = Key();
			key.publicKey = publicKey;
			key.words.reserve(result->word_list_.size());
			for (const auto &word : result->word_list_) {
				key.words.push_back(QByteArray(word.data(), word.size()));
			}
			Send(
				Make<api::deleteKey>(publicKey.toStdString()),
				[=](api::object_ptr<api::ok>) { done(key); },
				ErrorHandler(error));
		};
		Send(
			Make<api::exportKey>(
				Make<api::inputKey>(
					std::move(key),
					GlobalClient->localPassword())),
			exported,
			ErrorHandler(error));
	};
	Send(
		Make<api::createNewKey>(
			GlobalClient->localPassword(),
			GlobalClient->mnemonicPassword(),
			td::SecureString{ seed.data(), size_t(seed.size()) }),
				created,
				ErrorHandler(error));
}

void CheckKey(
		const Key &key,
		Fn<void()> done,
		Fn<void(Error)> error) {
	const auto publicKey = key.publicKey;
	const auto imported = [=](api::object_ptr<api::key> result) {
		if (publicKey.toStdString() != result->public_key_) {
			error(WrapError("DIFFERENT_KEY"));
			return;
		}
		Send(
			Make<api::deleteKey>(publicKey.toStdString()),
			[=](api::object_ptr<api::ok>) { done(); },
			ErrorHandler(error));
	};
	const auto importKey = [=] {
		auto words = std::vector<td::SecureString>();
		for (const auto &word : key.words) {
			words.emplace_back(word.data(), word.size());
		}
		Send(
			Make<api::importKey>(
				GlobalClient->localPassword(),
				GlobalClient->mnemonicPassword(),
				Make<api::exportedKey>(std::move(words))),
			imported,
			ErrorHandler(error));
	};
	Send(
		Make<api::deleteKey>(publicKey.toStdString()),
		[=](api::object_ptr<api::ok>) { importKey(); },
		[=](api::object_ptr<api::error>) { importKey(); });
}

void Finish() {
	GlobalClient = nullptr;
}

} // namespace Ton
