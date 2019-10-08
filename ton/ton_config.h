// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace Ton {

struct Config {
	QByteArray json;
	QString blockchainName;
	bool useNetworkCallbacks = false;
	bool ignoreCache = false;
};

} // namespace Ton
