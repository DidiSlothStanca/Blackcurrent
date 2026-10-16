#pragma once

// Terjemahan UI sederhana (Indonesia <-> English).
//
// Teks sumber di kode adalah bahasa Indonesia. TR("teks") mencari padanannya di
// tabel (i18n.cpp); bila bahasa aktif Indonesia atau teks tidak ada di tabel,
// teks sumber dikembalikan apa adanya.

#include <QString>

namespace bc {
namespace i18n {

enum class Lang { Indonesian, English };

Lang current();
void setLanguage(Lang lang);

// Kode bahasa untuk disimpan di setelan: "id" atau "en".
QString codeOf(Lang lang);
QString currentCode();
Lang fromCode(const QString &code);

// Terjemahkan teks sumber (bahasa Indonesia) ke bahasa aktif.
QString translate(const QString &source);

} // namespace i18n
} // namespace bc

#define TR(text) ::bc::i18n::translate(QStringLiteral(text))
