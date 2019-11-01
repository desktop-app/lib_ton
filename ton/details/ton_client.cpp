// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_client.h"

namespace Ton::details {
namespace {

constexpr auto kMinRequestResendDelay = crl::time(100);
constexpr auto kMaxRequestResendDelay = 10 * crl::time(1000);

crl::time NextRequestResendDelay(crl::time currentDelay) {
	return !currentDelay
		? kMinRequestResendDelay
		: std::clamp(
			currentDelay * 2,
			crl::time(1000),
			kMaxRequestResendDelay);
}

} // namespace

Client::Client(Fn<void(LibUpdate)> updateCallback)
: _updateCallback(std::move(updateCallback))
, _thread([=] { check(); }) {
}

Client::~Client() {
	_finished = true;
	QMutexLocker lock(&_mutex);
	_handlers.clear();
	lock.unlock();
	send(
		[] { return tonlib_api::make_object<tonlib_api::close>(); },
		nullptr);
	_thread.join();
}

RequestId Client::send(
		Fn<LibRequest()> request,
		FnMut<bool(LibResponse)> handler) {
	const auto requestId = (_requestIdAutoIncrement++) + 1;
	const auto libRequestId = (_libRequestIdAutoIncrement++) + 1;
	auto sending = request();

	QMutexLocker lock(&_mutex);
	_requestIdByLibRequestId.emplace(libRequestId, requestId);
	_requests.emplace(requestId, std::move(request));
	if (handler) {
		_handlers.emplace(requestId, std::move(handler));
	}
	lock.unlock();

	_wrapped.send({ libRequestId, std::move(sending) });
	return requestId;
}

void Client::resend(RequestId requestId) {
	const auto libRequestId = (_libRequestIdAutoIncrement++) + 1;

	QMutexLocker lock(&_mutex);
	const auto i = _requests.find(requestId);
	if (i == end(_requests)) {
		return;
	}
	auto request = i->second;
	_requestIdByLibRequestId.emplace(libRequestId, requestId);
	lock.unlock();

	_wrapped.send({ libRequestId, request() });
}

Client::LibResponse Client::Execute(LibRequest request) {
	return tonlib::Client::execute({ 0, std::move(request) }).object;
}

void Client::cancel(RequestId requestId) {
	QMutexLocker lock(&_mutex);
	_requests.remove(requestId);
	_handlers.remove(requestId);
}

void Client::scheduleResendOnError(RequestId requestId) {
	crl::on_main(this, [=] {
		const auto delay = _requestResendDelays.take(requestId).value_or(0);
		const auto nextDelay = NextRequestResendDelay(delay);
		_requestResendDelays.emplace(requestId, nextDelay);
		_resendTimer.call(nextDelay, [=] {
			resend(requestId);
		});
	});
}

void Client::check() {
	while (!_finished) {
		auto response = _wrapped.receive(60.);
		if (!response.object) {
			continue;
		}
		QMutexLocker lock(&_mutex);
		const auto requestId = _requestIdByLibRequestId.take(response.id);
		auto handler = requestId ? _handlers.take(*requestId) : std::nullopt;
		lock.unlock();

		if (handler) {
			if ((*handler)(std::move(response.object))) {
				QMutexLocker lock(&_mutex);
				_requests.remove(*requestId);
			} else {
				QMutexLocker lock(&_mutex);
				_handlers.emplace(*requestId, std::move(*handler));
				lock.unlock();

				scheduleResendOnError(*requestId);
			}
		} else {
			if (requestId) {
				QMutexLocker lock(&_mutex);
				_requests.remove(*requestId);
			} else if (!response.id && _updateCallback) {
				_updateCallback(tonlib_api::move_object_as<tonlib_api::Update>(
					std::move(response.object)));
			}
		}
	}
}

} // namespace Ton::details
