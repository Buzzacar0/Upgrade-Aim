#pragma once

#include <QString>

// UI language, chosen once in the launcher before any page is built.
// Pages call T("English text") for every user-visible string; with Portuguese
// selected it returns the pt-BR translation from i18n_pt_br.cpp, or the English
// text when there is none. Data written to files (profiles, settings keys,
// exported reports' field names) stays English.
namespace i18n {

enum class Lang { PtBr, En };

void setLanguage(Lang lang);
Lang language();
QString code(Lang lang);       // "pt-BR" / "en"
Lang fromCode(const QString& code);  // anything that is not "en" -> PtBr (the default)

}  // namespace i18n

QString T(const char* en);
