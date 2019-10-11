// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton_tl.h"
#include "ton_tl-conversion.h"
#include "ton/details/ton_client.h"
#include "ton/ton_result.h"
#include "base/weak_ptr.h"

namespace Ton::details {

[[nodiscard]] Error ErrorFromLib(const TLerror &error);

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
		explicit RequestBuilder(not_null<RequestSender*> sender) noexcept;
		RequestBuilder(RequestBuilder &&other) = default;

		void setDoneOnMainHandler(FnMut<void()> &&handler) noexcept;
		void setFailOnMainHandler(FnMut<void(const TLError&)> &&handler) noexcept;
		void setFailOnMainHandler(FnMut<void()> &&handler) noexcept;
		void setDoneHandler(FnMut<void()> &&handler) noexcept;
		void setFailHandler(FnMut<void(const TLError&)> &&handler) noexcept;
		void setFailHandler(FnMut<void()> &&handler) noexcept;
		void setDoneHandler(FnMut<void(LibResponse)> &&handler) noexcept;
		void setFailHandler(FnMut<bool(LibError)> &&handler) noexcept;
		RequestId send(Fn<LibRequest()> request) noexcept;
		base::weak_ptr<RequestSender> on_main_guard() const;

	private:
		const not_null<RequestSender*> _sender;
		FnMut<void(LibResponse)> _done;
		FnMut<bool(LibError)> _fail;

	};

public:
	template <typename Request>
	class SpecificRequestBuilder final : public RequestBuilder {
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
			setDoneOnMainHandler(std::move(callback));
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
			setFailOnMainHandler(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &fail(
				FnMut<void(const TLError &error)> callback) noexcept {
			setFailOnMainHandler(std::move(callback));
			return *this;
		}

		[[nodiscard]] SpecificRequestBuilder &done_async(
				FnMut<void()> callback) {
			setDoneHandler(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &done_async(
			FnMut<void(
				const typename Request::ResponseType &result)> callback) {
			setDoneHandler([callback = std::move(callback)](
					LibResponse result) mutable {
				using FPointer = std::decay_t<decltype(tl_to(_request))>;
				using Function = typename FPointer::element_type;
				using RPointer = typename Function::ReturnType;
				using ReturnType = typename RPointer::element_type;
				callback(tl_from(tonlib_api::move_object_as<ReturnType>(
					std::move(result))));
			});
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &fail_async(
				FnMut<void()> callback) noexcept {
			setFailHandler(std::move(callback));
			return *this;
		}
		[[nodiscard]] SpecificRequestBuilder &fail_async(
				FnMut<void(const TLError &error)> callback) noexcept {
			setFailHandler(std::move(callback));
			return *this;
		}

		RequestId send() {
			return RequestBuilder::send([copy = std::move(_request)] {
				return tl_to(copy);
			});
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

	template <
		typename Request,
		typename = std::enable_if_t<
		!std::is_reference_v<Request>
		&& std::is_class_v<typename Request::ResponseType>
		&& std::is_class_v<typename Request::Unboxed>>>
	static Result<typename Request::ResponseType> Execute(Request &&request);

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

template <typename Request, typename>
Result<typename Request::ResponseType> RequestSender::Execute(
		Request &&request) {
	auto response = Client::Execute(tl_to(request));
	Assert(response != nullptr);

	if (response->get_id() == tonlib_api::error::ID) {
		return ErrorFromLib(tl_from(
			tonlib_api::move_object_as<tonlib_api::error>(
				std::move(response))));
	}
	using FPointer = std::decay_t<decltype(tl_to(request))>;
	using Function = typename FPointer::element_type;
	using RPointer = typename Function::ReturnType;
	using ReturnType = typename RPointer::element_type;
	return tl_from(tonlib_api::move_object_as<ReturnType>(
		std::move(response)));
}

} // namespace Ton::details
