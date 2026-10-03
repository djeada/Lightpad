#ifndef SQLPARAMS_H
#define SQLPARAMS_H

#include "dbtypes.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace SqlParams {

QStringList findParameters(const QString &sql);

QString literalFromInput(const QString &input, DbEngine engine);

QString bind(const QString &sql, const QHash<QString, QString> &inputs,
             DbEngine engine, QStringList *missing = nullptr);

} // namespace SqlParams

#endif
