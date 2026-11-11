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
bool supportsNativeBuffers(const QString &adapterId,
                           const QString &adapterType);
bool canViewBuffer(const QString &type, const QString &memoryReference);
int arrayLength(const QString &type);
QString pointeeType(const QString &type);
int pointerDepth(const QString &type);
bool isCharacterPointer(const QString &type);
bool isCharacterArray(const QString &type);
bool stringBytes(const QString &value, QByteArray *bytes);
} // namespace DebugValueFormatter

#endif
