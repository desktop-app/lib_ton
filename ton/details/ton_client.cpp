// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_client.h"

namespace Ton::details {

Client::Client() : _thread([=] { check(); }) {
}

Client::~Client() {
	_finished = true;
	send(tonlib_api::make_object<tonlib_api::close>(), nullptr);
	_thread.join();
}

RequestId Client::send(
		LibRequest request,
		FnMut<void(LibResponse)> handler) {
	const auto requestId = (_requestIdAutoIncrement++) + 1;

	if (handler) {
		QMutexLocker lock(&_mutex);
		_handlers.emplace(requestId, std::move(handler));
		lock.unlock();
	}

	_wrapped.send({ requestId, std::move(request) });
	return requestId;
}

Client::LibResponse Client::Execute(LibRequest request) {
	return tonlib::Client::execute({ 0, std::move(request) }).object;
}

void Client::cancel(RequestId requestId) {
	QMutexLocker lock(&_mutex);
	_handlers.remove(requestId);
}

void Client::check() {
	while (!_finished) {
		auto response = _wrapped.receive(60.);
		if (!response.object) {
			continue;
		}
		QMutexLocker lock(&_mutex);
		auto handler = _handlers.take(response.id);
		lock.unlock();

		if (handler) {
			(*handler)(std::move(response.object));
		}
	}
}

} // namespace Ton::details
