// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ton/details/ton_web_loader.h"

#include <QtCore/QPointer>
#include <QtNetwork/QNetworkReply>

namespace Ton::details {
namespace {

Result<QByteArray> ParseResult(not_null<QNetworkReply*> reply) {
	if (reply->error() != QNetworkReply::NoError) {
		return Error{ Error::Type::Web, reply->errorString() };
	}
	return reply->readAll();
}

} // namespace

WebLoader::WebLoader(Fn<void()> finished) : _finished(std::move(finished)) {
}

WebLoader::~WebLoader() {
	_requests.clear();
}

void WebLoader::load(const QString &url, Callback<QByteArray> done) {
	auto &list = _requests[url];
	list.push_back(std::move(done));
	if (list.size() > 1) {
		return;
	}

	const auto weak = QPointer<QNetworkAccessManager>(&_manager);
	auto request = QNetworkRequest();
	request.setUrl(url);
	const auto reply = _manager.get(request);
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		crl::on_main(weak, [=] {
			reply->deleteLater();

			const auto result = ParseResult(reply);
			if (auto list = _requests.take(url)) {
				for (auto callback : *list) {
					callback(result);
					if (!weak) {
						return;
					}
				}
			}
			if (_requests.empty()) {
				const auto onstack = _finished;
				onstack();
			}
		});
	});
}

} // namespace Ton::details
