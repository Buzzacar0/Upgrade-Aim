#include "i18n.hpp"

#include <QHash>

#include <cstddef>
#include <utility>

// Defined in i18n_pt_br.cpp.
extern const std::pair<const char*, const char*> kPtBr[];
extern const std::size_t kPtBrCount;

namespace i18n {
namespace {

Lang g_lang = Lang::PtBr;

const QHash<QByteArray, QString>& ptBr() {
    static const QHash<QByteArray, QString> table = [] {
        QHash<QByteArray, QString> h;
        h.reserve(static_cast<qsizetype>(kPtBrCount));
        for (std::size_t i = 0; i < kPtBrCount; ++i) h.insert(QByteArray(kPtBr[i].first), QString::fromUtf8(kPtBr[i].second));
        return h;
    }();
    return table;
}

}  // namespace

void setLanguage(Lang lang) { g_lang = lang; }
Lang language() { return g_lang; }
QString code(Lang lang) { return lang == Lang::En ? "en" : "pt-BR"; }
Lang fromCode(const QString& c) { return c.compare("en", Qt::CaseInsensitive) == 0 ? Lang::En : Lang::PtBr; }

}  // namespace i18n

QString T(const char* en) {
    if (i18n::g_lang == i18n::Lang::En) return QString::fromUtf8(en);
    const auto& table = i18n::ptBr();
    const auto it = table.constFind(QByteArray::fromRawData(en, static_cast<qsizetype>(qstrlen(en))));
    return it != table.cend() ? *it : QString::fromUtf8(en);
}
