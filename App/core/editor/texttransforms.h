#ifndef TEXTTRANSFORMS_H
#define TEXTTRANSFORMS_H

#include <QString>
#include <QStringList>

namespace TextTransforms {

QString sortLinesAscending(const QString &text);

QString sortLinesDescending(const QString &text);

QString toUppercase(const QString &text);

QString toLowercase(const QString &text);

QString toTitleCase(const QString &text);

QString removeDuplicateLines(const QString &text);

QString reverseLines(const QString &text);

QString trimLines(const QString &text);

} // namespace TextTransforms

#endif
