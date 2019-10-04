// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <QtCore/QMutex>
#include <auto/tl/tonlib_api.h>
#include <tonlib/Client.h>
#include <thread>
#include <atomic>

namespace Ton::details {

namespace tonlib_api = ::ton::tonlib_api;
using RequestId = uint32;

class Client final {
public:
	using LibRequest = tonlib_api::object_ptr<tonlib_api::Function>;
	using LibResponse = tonlib_api::object_ptr<tonlib_api::Object>;

	Client();
	~Client();

	RequestId send(LibRequest request, FnMut<void(LibResponse)> handler);
	void cancel(RequestId requestId);

	static LibResponse Execute(LibRequest request);

private:
	void check();

	tonlib::Client _wrapped;
	std::atomic<RequestId> _requestIdAutoIncrement = 0;
	QMutex _mutex;
	base::flat_map<RequestId, FnMut<void(LibResponse)>> _handlers;

	std::thread _thread;
	std::atomic<bool> _finished = false;

};

} // namespace Ton::details
