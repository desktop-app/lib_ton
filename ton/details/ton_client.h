// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/timer.h"
#include "base/weak_ptr.h"

#include <QtCore/QMutex>
#include <auto/tl/tonlib_api.h>
#include <tonlib/Client.h>
#include <thread>
#include <atomic>

namespace Ton::details {

namespace tonlib_api = ::ton::tonlib_api;
using RequestId = uint32;

class Client final : public base::has_weak_ptr {
public:
	using LibRequest = tonlib_api::object_ptr<tonlib_api::Function>;
	using LibResponse = tonlib_api::object_ptr<tonlib_api::Object>;
	using LibUpdate = tonlib_api::object_ptr<tonlib_api::Update>;

	explicit Client(Fn<void(LibUpdate)> updateCallback);
	~Client();

	RequestId send(
		Fn<LibRequest()> request,
		FnMut<bool(LibResponse)> handler);
	void cancel(RequestId requestId);

	[[nodiscard]] rpl::producer<RequestId> resendingOnError() const;

	static LibResponse Execute(LibRequest request);

private:
	void check();
	void scheduleResendOnError(RequestId requestId);
	void resend(RequestId requestId);

	tonlib::Client _wrapped;
	std::atomic<RequestId> _requestIdAutoIncrement = 0;
	std::atomic<uint32> _libRequestIdAutoIncrement = 0;
	const Fn<void(LibUpdate)> _updateCallback;

	QMutex _mutex;
	base::flat_map<uint32, RequestId> _requestIdByLibRequestId;
	base::flat_map<RequestId, Fn<LibRequest()>> _requests;
	base::flat_map<RequestId, FnMut<bool(LibResponse)>> _handlers;

	std::thread _thread;
	std::atomic<bool> _finished = false;

	// Accessed from main thread only.
	base::flat_map<RequestId, crl::time> _requestResendDelays;
	base::DelayedCallTimer _resendTimer;
	rpl::event_stream<RequestId> _resendingOnError;

};

} // namespace Ton::details
