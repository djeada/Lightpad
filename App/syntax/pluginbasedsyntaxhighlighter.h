#ifndef PLUGINBASEDSYNTAXHIGHLIGHTER_H
#define PLUGINBASEDSYNTAXHIGHLIGHTER_H

#include "../plugins/isyntaxplugin.h"
#include "../settings/theme.h"
#include <QRegularExpression>
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

  // The find panel builds the pattern that actually drives matching (regex,
  // whole-word and case options); the highlighter must use the same one or the
  // highlights disagree with the match count and navigation.
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

  Theme m_theme;
  QString m_searchKeyword;
  QRegularExpression m_searchPattern;

  QVector<SyntaxRule> m_rules;

  QVector<MultiLineBlock> m_multiLineBlocks;

  QTextCharFormat m_searchFormat;

  int m_firstVisibleBlock = -1;
  int m_lastVisibleBlock = -1;
  int m_lastBlockCount = -1;

  static constexpr int VIEWPORT_BUFFER = 50;
};

#endif
