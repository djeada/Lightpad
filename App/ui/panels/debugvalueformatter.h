#ifndef DEBUGVALUEFORMATTER_H
#define DEBUGVALUEFORMATTER_H

#include <QByteArray>
#include <QString>

namespace DebugValueFormatter {

enum class DisplayFormat { Natural, Decimal, Hexadecimal, Binary, Character };

QString format(const QString &value, int width = 88);
QString summary(const QString &value, int length = 160);
QString tableCell(const QString &value);
QString scalar(const QString &value, const QString &type,
               DisplayFormat display);
QString childExpression(const QString &parent, const QString &parentType,
                        const QString &name);
int arrayLength(const QString &type);
QString pointeeType(const QString &type);
int pointerDepth(QString type);
bool stringBytes(const QString &value, QByteArray *bytes);
} // namespace DebugValueFormatter

#endif
