#include "i18n.h"

#include <QHash>

namespace bc {
namespace i18n {

namespace {

Lang g_lang = Lang::English; // bawaan: English (bisa diganti & disimpan)

struct Entry
{
    const char *id; // teks sumber (Indonesia)
    const char *en; // terjemahan Inggris
};

const Entry kEntries[] = {
#include "i18n_table.inc"
};

const QHash<QString, QString> &table()
{
    static const QHash<QString, QString> t = []() {
        QHash<QString, QString> h;
        for (const Entry &e : kEntries)
            h.insert(QString::fromUtf8(e.id), QString::fromUtf8(e.en));
        return h;
    }();
    return t;
}

} // namespace

Lang current() { return g_lang; }

void setLanguage(Lang lang) { g_lang = lang; }

QString codeOf(Lang lang)
{
    return lang == Lang::English ? QStringLiteral("en") : QStringLiteral("id");
}

QString currentCode() { return codeOf(g_lang); }

Lang fromCode(const QString &code)
{
    // Kode tak dikenal/kosong -> English (bawaan).
    return code.startsWith(QLatin1String("id"), Qt::CaseInsensitive) ? Lang::Indonesian
                                                                      : Lang::English;
}

QString translate(const QString &source)
{
    if (g_lang == Lang::Indonesian)
        return source; // teks sumber sudah berbahasa Indonesia
    const auto it = table().constFind(source);
    return it != table().constEnd() ? it.value() : source;
}

} // namespace i18n
} // namespace bc
