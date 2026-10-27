#ifndef COMPLETIONINSERTION_H
#define COMPLETIONINSERTION_H

#include "completionitem.h"
#include <QTextCursor>

namespace CompletionInsertion {
QString prefix(const QTextCursor &cursor);
// Returns false without editing when a server range is invalid or stale.
bool apply(QTextCursor &cursor, const CompletionItem &item);
} // namespace CompletionInsertion

#endif
