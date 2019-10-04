// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton_tl.h"
#include "ton_tl_conversion.h"
#include "ton/ton_client.h"
#include "base/weak_ptr.h"

namespace Ton {

class RequestSender final : public base::has_weak_ptr {
	using LibRequest = tonlib_api::object_ptr<tonlib_api::Function>;
	using LibResponse = tonlib_api::object_ptr<tonlib_api::Object>;
	using LibError = tonlib_api::object_ptr<tonlib_api::error>;

	class RequestBuilder {
	public:
		RequestBuilder(const RequestBuilder &other) = delete;
		RequestBuilder &operator=(const RequestBuilder &other) = delete;
		RequestBuilder &operator=(RequestBuilder &&other) = delete;

	protected:
		explicit RequestBuilder(not_null<RequestSender*> sender) noexcept
		: _sender(sender) {
		}
		RequestBuilder(RequestBuilder &&other) = default;

		void setDoneHandler(FnMut<void(LibResponse)> &&handler) noexcept {
			_done = std::move(handler);
		}
		void setFailHandler(FnMut<void(LibError)> &&handler) noexcept {
			_fail = std::move(handler);
		}
		RequestId send(LibRequest request) noexcept {
			auto ready = [done = std::move(_done), fail = std::move(_fail)](
					LibResponse response) mutable {
				Expects(response != nullptr);

				if (response->get_id() == tonlib_api::error::ID) {
					fail(tonlib_api::move_object_as<tonlib_api::error>(
						std::move(response)));
				} else {
					done(std::move(response));
				}
			};
			return _sender->_client.send(
				std::move(request),
				std::move(ready));
		}
		base::weak_ptr<RequestSender> on_main_guard() const {
			return base::make_weak(_sender.get());
		}

	private:
		const not_null<RequestSender*> _sender;
		FnMut<void(LibResponse)> _done;
		FnMut<void(LibError)> _fail;

	};

public:
	template <typename Request>
	class SpecificRequestBuilder : public RequestBuilder {
	private:
		friend class RequestSender;
		SpecificRequestBuilder(
			not_null<RequestSender*> sender,
			Request &&request) noexcept
		: RequestBuilder(sender)
		, _request(std::move(request)) {
		}
		SpecificRequestBuilder(SpecificRequestBuilder &&other) = default;

	public:
		[[nodiscard]] SpecificRequestBuilder &done(FnMut<void()> callback) {
			setDoneHandler([
				callback = std::move(callback),
				guard = on_main_guard()
			](LibResponse result) mutable {
				crl::on_main(guard, std::move(callback));
			});
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &done(
			FnMut<void(
				const typename Request::ResponseType &result)> callback) {
			setDoneHandler([
				callback = std::move(callback),
				guard = on_main_guard()
			](LibResponse result) mutable {
				using FPointer = std::decay_t<decltype(tl_to(_request))>;
				using Function = typename FPointer::element_type;
				using RPointer = typename Function::ReturnType;
				using ReturnType = typename RPointer::element_type;
				crl::on_main(guard, [
					callback = std::move(callback),
					result = tl_from(tonlib_api::move_object_as<ReturnType>(
						std::move(result)))
				]() mutable {
					callback(result);
				});
			});
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &fail(
				FnMut<void()> callback) noexcept {
			setFailHandler([
				callback = std::move(callback),
				guard = on_main_guard()
			](LibError error) mutable {
				crl::on_main(guard, std::move(callback));
			});
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &fail(
				FnMut<void(const TLError &error)> callback) noexcept {
			setFailHandler([
				callback = std::move(callback),
				guard = on_main_guard()
			](LibError error) mutable {
				crl::on_main(guard, [
					callback = std::move(callback),
					error = tl_from(std::move(error))
				]() mutable {
					callback(error);
				});
			});
			return *this;
		}

		RequestId send() {
			return RequestBuilder::send(tl_to(_request));
		}

	private:
		Request _request;

	};

	template <
		typename Request,
		typename = std::enable_if_t<
			!std::is_reference_v<Request>
			&& std::is_class_v<typename Request::ResponseType>
			&& std::is_class_v<typename Request::Unboxed>>>
	[[nodiscard]] SpecificRequestBuilder<Request> request(
		Request &&request) noexcept;

private:
	template <typename Request>
	friend class SpecialRequestBuilder;
	friend class RequestBuilder;

	Client _client;
	base::flat_set<RequestId> _requests;

};

template <typename Request, typename>
RequestSender::SpecificRequestBuilder<Request> RequestSender::request(
		Request &&request) noexcept {
	return SpecificRequestBuilder<Request>(this, std::move(request));
}

} // namespace Ton
