#ifndef PLUGINBASEDSYNTAXHIGHLIGHTER_H
#define PLUGINBASEDSYNTAXHIGHLIGHTER_H

#include "../plugins/isyntaxplugin.h"
#include "../settings/theme.h"
#include <QRegularExpression>
#include <QSet>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class PluginBasedSyntaxHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

public:
  PluginBasedSyntaxHighlighter(ISyntaxPlugin *plugin, const Theme &theme,
                               const QString &searchKeyword = "",
                               QTextDocument *parent = nullptr);

  void setSearchKeyword(const QString &keyword);

  void setSearchPattern(const QRegularExpression &pattern);

  QString searchKeyword() const { return m_searchKeyword; }

  void setVisibleBlockRange(int first, int last);

protected:
  void highlightBlock(const QString &text) override;

private:
  bool isBlockVisible(int blockNumber) const;
  void rehighlightBlockRange(int firstBlock, int lastBlock);

  void loadRulesFromPlugin(ISyntaxPlugin *plugin, const Theme &theme);

  QTextCharFormat applyThemeToFormat(const SyntaxRule &rule,
                                     const Theme &theme);

  void refreshTypeNames();
  QVector<SyntaxRule> m_typeDefinitions;
  QSet<QString> m_typeNames;
  bool m_singleQuotedStrings = false;
  int m_typeRevision = -1;
  bool m_typeRefreshPending = false;

  Theme m_theme;
  QString m_searchKeyword;
  QRegularExpression m_searchPattern;

  QVector<SyntaxRule> m_rules;
  QVector<quint8> m_ruleKinds;

  QVector<MultiLineBlock> m_multiLineBlocks;

  QTextCharFormat m_searchFormat;

  int m_firstVisibleBlock = -1;
  int m_lastVisibleBlock = -1;
  int m_lastBlockCount = -1;

  static constexpr int VIEWPORT_BUFFER = 50;
};

#endif
