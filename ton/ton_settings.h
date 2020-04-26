// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

namespace Ton {

struct NetSettings {
	QString blockchainName;
	QString configUrl;
	QByteArray config;
	bool useCustomConfig = false;
};

struct Settings {
	NetSettings main;
	NetSettings test;
	bool useTestNetwork = false;
	bool useNetworkCallbacks = false;
	qint32 version = 0;

	[[nodiscard]] const NetSettings &net(bool useTestNetwork) const {
		return useTestNetwork ? test : main;
	}
	[[nodiscard]] NetSettings &net(bool useTestNetwork) {
		return useTestNetwork ? test : main;
	}
	[[nodiscard]] const NetSettings &net() const {
		return net(useTestNetwork);
	}
	[[nodiscard]] NetSettings &net() {
		return net(useTestNetwork);
	}
};

enum class ConfigUpgrade {
	None,
	TestnetToTestnet2,
	TestnetToMainnet,
};

} // namespace Ton
