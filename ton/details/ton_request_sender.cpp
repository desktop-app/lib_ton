// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_request_sender.h"

namespace Ton::details {

Error ErrorFromLib(const TLerror &error) {
	const auto message = error.match([&](const TLDerror &data) {
		return tl::utf16(data.vmessage());
	});
	return { Error::Type::TonLib, message };
}

RequestSender::RequestBuilder::RequestBuilder(
	not_null<RequestSender*> sender) noexcept
: _sender(sender) {
}

void RequestSender::RequestBuilder::setDoneOnMainHandler(
		FnMut<void()> &&handler) noexcept {
	setDoneHandler([
		callback = std::move(handler),
		guard = on_main_guard()
	](LibResponse result) mutable {
		crl::on_main(guard, std::move(callback));
	});
}

void RequestSender::RequestBuilder::setFailOnMainHandler(
		FnMut<void(const TLError&)> &&handler) noexcept {
	setFailHandler([
		callback = std::move(handler),
		guard = on_main_guard()
	](LibError error) mutable {
		crl::on_main(guard, [
			callback = std::move(callback),
			error = tl_from(std::move(error))
		]() mutable {
			callback(error);
		});
	});
}

void RequestSender::RequestBuilder::setFailOnMainHandler(
		FnMut<void()> &&handler) noexcept {
	setFailHandler([
		callback = std::move(handler),
		guard = on_main_guard()
	](LibError error) mutable {
		crl::on_main(guard, std::move(callback));
	});
}

void RequestSender::RequestBuilder::setDoneHandler(FnMut<void()> &&handler) noexcept {
	setDoneHandler([callback = std::move(handler)](
			LibResponse result) mutable {
		callback();
	});
}

void RequestSender::RequestBuilder::setFailHandler(
		FnMut<void(const TLError&)> &&handler) noexcept {
	setFailHandler([callback = std::move(handler)](
			LibError error) mutable {
		callback(tl_from(std::move(error)));
	});
}

void RequestSender::RequestBuilder::setFailHandler(
		FnMut<void()> &&handler) noexcept {
	setFailHandler([callback = std::move(handler)](LibError error) mutable {
		callback();
	});
}

void RequestSender::RequestBuilder::setDoneHandler(
		FnMut<void(LibResponse)> &&handler) noexcept {
	_done = std::move(handler);
}

void RequestSender::RequestBuilder::setFailHandler(
		FnMut<void(LibError)> &&handler) noexcept {
	_fail = std::move(handler);
}

RequestId RequestSender::RequestBuilder::send(LibRequest request) noexcept {
	auto ready = [done = std::move(_done), fail = std::move(_fail)](
			LibResponse response) mutable {
		Expects(response != nullptr);

		if (response->get_id() == tonlib_api::error::ID) {
			if (fail) {
				fail(tonlib_api::move_object_as<tonlib_api::error>(
					std::move(response)));
			}
		} else if (done) {
			done(std::move(response));
		}
	};
	return _sender->_client.send(
		std::move(request),
		std::move(ready));
}

auto RequestSender::RequestBuilder::on_main_guard() const
-> base::weak_ptr<RequestSender> {
	return base::make_weak(_sender.get());
}

} // namespace Ton::details
