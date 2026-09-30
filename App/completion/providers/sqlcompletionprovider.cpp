#include "sqlcompletionprovider.h"

#include "../../database/databasemanager.h"
#include "../../database/sqlcompletion.h"
#include "../../database/sqlstatementsplitter.h"

#include <QTextStream>

namespace {

CompletionItemKind kindFor(SqlSuggestion::Kind kind) {
  switch (kind) {
  case SqlSuggestion::Kind::Keyword:
    return CompletionItemKind::Keyword;
  case SqlSuggestion::Kind::Type:
    return CompletionItemKind::TypeParameter;
  case SqlSuggestion::Kind::Function:
    return CompletionItemKind::Function;
  case SqlSuggestion::Kind::Schema:
    return CompletionItemKind::Module;
  case SqlSuggestion::Kind::Table:
    return CompletionItemKind::Struct;
  case SqlSuggestion::Kind::View:
    return CompletionItemKind::Interface;
  case SqlSuggestion::Kind::Column:
    return CompletionItemKind::Field;
  }
  return CompletionItemKind::Text;
}

int offsetOf(const QString &text, int line, int column) {
  int offset = 0;
  for (int i = 0; i < line; ++i) {
    const int nl = text.indexOf('\n', offset);
    if (nl < 0) {
      return text.size();
    }
    offset = nl + 1;
  }
  return qMin(text.size(), offset + column);
}

} // namespace

void SqlCompletionProvider::requestCompletions(
    const CompletionContext &context,
    std::function<void(const QList<CompletionItem> &)> callback) {
  QList<CompletionItem> items;
  if (!m_enabled || !m_documentText) {
    callback(items);
    return;
  }

  const QString text = m_documentText(context.documentUri);
  if (text.isEmpty()) {
    callback(items);
    return;
  }

  DatabaseManager &manager = DatabaseManager::instance();
  DbConnection *conn = nullptr;
  const QString directive = SqlStatementSplitter::connectionDirective(text);
  if (!directive.isEmpty()) {
    conn = manager.connectionByName(directive);
  }
  if (!conn) {
    conn = manager.activeConnection();
  }
  const DbEngine engine = conn ? conn->profile().engine : DbEngine::PostgreSql;
  const DbSchema *schema =
      conn && !conn->schema().isEmpty() ? &conn->schema() : nullptr;

  const int cursor = offsetOf(text, context.line, context.column);

  int stmtStart = 0;
  QString full;
  const auto statements = SqlStatementSplitter::split(text, engine);
  for (const SqlStatement &st : statements) {
    if (st.end <= cursor &&
        text.mid(st.start, st.end - st.start).endsWith(';')) {
      stmtStart = st.end;
      continue;
    }
    if (cursor >= st.start) {
      stmtStart = st.start;
      full = st.text;
    }
    if (cursor <= st.end) {
      break;
    }
  }
  const QString before = text.mid(stmtStart, cursor - stmtStart);
  if (full.size() < before.size()) {
    full = before;
  }

  const auto suggestions =
      SqlCompletion::suggest(schema, engine, before, full, context.prefix);
  items.reserve(suggestions.size());
  for (const SqlSuggestion &s : suggestions) {
    CompletionItem item;
    item.label = s.label;
    item.insertText = s.insertText;
    item.detail = s.detail;
    item.kind = kindFor(s.kind);
    item.priority = s.priority;
    item.providerId = id();
    items.append(item);
  }
  callback(items);
}
