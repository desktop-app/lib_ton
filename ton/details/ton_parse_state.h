// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "ton/ton_state.h"
#include "ton_tl.h"

namespace Ton::details {

[[nodiscard]] TransactionId Parse(const TLinternal_TransactionId &data);
[[nodiscard]] AccountState Parse(const TLFullAccountState &data);
[[nodiscard]] Transaction Parse(const TLraw_Transaction &data);
[[nodiscard]] TransactionsSlice Parse(const TLraw_Transactions &data);
[[nodiscard]] PendingTransaction Parse(
	const TLquery_Info &data,
	const QString &sender,
	const TransactionToSend &transaction);
[[nodiscard]] TransactionCheckResult Parse(const TLquery_Fees &data);
[[nodiscard]] std::vector<QString> Parse(const TLExportedKey &data);
[[nodiscard]] Update Parse(const TLUpdate &data);

[[nodiscard]] TLmsg_DataEncryptedArray MsgDataArrayFromEncrypted(
	const QVector<EncryptedText> &data);
[[nodiscard]] QVector<DecryptedText> MsgDataArrayToDecrypted(
	const TLmsg_DataDecryptedArray &data);

[[nodiscard]] QVector<EncryptedText> CollectEncryptedTexts(
	const TransactionsSlice &data);
[[nodiscard]] TransactionsSlice AddDecryptedTexts(
	TransactionsSlice parsed,
	const QVector<EncryptedText> &encrypted,
	const QVector<DecryptedText> &decrypted);

} // namespace Ton::details
