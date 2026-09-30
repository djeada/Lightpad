#ifndef SQLCONSOLEEDIT_H
#define SQLCONSOLEEDIT_H

#include "../../database/dbcatalog.h"
#include "../../database/sqlcompletion.h"
#include "../../settings/theme.h"

#include <QCompleter>
#include <QPlainTextEdit>
#include <QStandardItemModel>
#include <QSyntaxHighlighter>
#include <functional>

class SqlHighlighter : public QSyntaxHighlighter {
public:
  explicit SqlHighlighter(QTextDocument *document);
  void setTheme(const Theme &theme);

protected:
  void highlightBlock(const QString &text) override;

private:
  QTextCharFormat m_keyword, m_type, m_function, m_string, m_number, m_comment,
      m_operator, m_identifier, m_variable;
  QSet<QString> m_keywords, m_types, m_functions;
};

class SqlConsoleEdit : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit SqlConsoleEdit(QWidget *parent = nullptr);

  void setEngine(DbEngine engine);
  DbEngine engine() const { return m_engine; }

  void setSchemaProvider(std::function<const DbSchema *()> provider) {
    m_schemaProvider = std::move(provider);
  }
  void applyTheme(const Theme &theme);

  QString selectionOrCurrentStatement() const;

signals:
  void runStatementRequested();
  void runScriptRequested();
  void historyStep(int direction);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  bool event(QEvent *event) override;

private slots:
  void insertCompletion(const QModelIndex &index);
  void updateCurrentStatement();

private:
  void showCompletions(bool explicitRequest);
  QString prefixAtCursor(int *start) const;

  DbEngine m_engine = DbEngine::PostgreSql;
  std::function<const DbSchema *()> m_schemaProvider;
  SqlHighlighter *m_highlighter;
  QCompleter *m_completer;
  QStandardItemModel *m_completionModel;
  QVector<SqlSuggestion> m_suggestions;
  Theme m_theme;
};

#endif
