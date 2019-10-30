// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_result.h"

#include <QtNetwork/QNetworkAccessManager>

namespace Ton::details {

class WebLoader final {
public:
	WebLoader(Fn<void()> finished);
	~WebLoader();

	void load(const QString &url, Callback<QByteArray> done);

private:
	const Fn<void()> _finished;
	QNetworkAccessManager _manager;
	base::flat_map<QString, std::vector<Callback<QByteArray>>> _requests;

};

} // namespace Ton::details
