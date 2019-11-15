// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace Ton {

struct Settings {
	QString blockchainName;
	QString configUrl;
	QByteArray config;
	bool useCustomConfig = false;
	bool useNetworkCallbacks = false;
	qint32 version = 0;
};

enum class ConfigUpgrade {
	None,
	TestnetToTestnet2,
};

} // namespace Ton
