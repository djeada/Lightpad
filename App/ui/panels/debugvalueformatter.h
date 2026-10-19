#ifndef DEBUGVALUEFORMATTER_H
#define DEBUGVALUEFORMATTER_H

#include <QString>

namespace DebugValueFormatter {
// Format adapter representations as text; never evaluate or reinterpret them.
QString format(const QString &value, int width = 88);
QString summary(const QString &value, int length = 160);
QString tableCell(const QString &value);
} // namespace DebugValueFormatter

#endif
