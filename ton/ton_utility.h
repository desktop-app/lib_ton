// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"
#include "ton/ton_result.h"

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <vector>

namespace Ton {

struct UtilityKey {
	QByteArray publicKey;
	std::vector<QByteArray> words;
};

void Start(Callback<> done);
void CreateKey(
	const QByteArray &seed,
	Callback<UtilityKey> done);
void CheckKey(
	const std::vector<QByteArray> &words,
	Callback<QByteArray> done);
void Finish();

} // namespace Ton
