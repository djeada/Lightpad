#ifndef SQLPARAMS_H
#define SQLPARAMS_H

#include "dbtypes.h"

#include <QHash>
#include <QString>
#include <QStringList>

// Named query parameters (":name") for ad-hoc SQL. Parameters are bound by
// substituting properly escaped literals, outside of strings, comments and
// quoted identifiers; "::" casts are left alone.
namespace SqlParams {

// Distinct parameter names in order of first appearance.
QStringList findParameters(const QString &sql);

// Turn text typed by the user into a SQL literal: NULL and numbers stay bare,
// TRUE/FALSE become the engine's boolean form, everything else is quoted.
QString literalFromInput(const QString &input, DbEngine engine);

// Replaces every :name with its literal. Names without an input are reported
// in 'missing' (when given) and left untouched.
QString bind(const QString &sql, const QHash<QString, QString> &inputs,
             DbEngine engine, QStringList *missing = nullptr);

} // namespace SqlParams

#endif
