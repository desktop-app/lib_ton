// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <vector>

namespace Ton {

struct Error {
	QString code;
};

struct Key {
	QByteArray publicKey;
	std::vector<QByteArray> words;
};

void Start(
	const QString &path,
	Fn<void()> done,
	Fn<void(Error)> error);
void GetValidWords(
	Fn<void(std::vector<QByteArray>)> done,
	Fn<void(Error)> error);
void CreateKey(
	const QByteArray &seed,
	Fn<void(Key)> done,
	Fn<void(Error)> error);
void CheckKey(
	const Key &key,
	Fn<void()> done,
	Fn<void(Error)> error);
void Finish();

} // namespace Ton
