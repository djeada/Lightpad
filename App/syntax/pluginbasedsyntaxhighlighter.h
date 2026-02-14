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

  QString searchKeyword() const { return m_searchKeyword; }

  void setVisibleBlockRange(int first, int last) {
    m_firstVisibleBlock = first;
    m_lastVisibleBlock = last;
  }

protected:
  void highlightBlock(const QString &text) override;

private:
  bool isBlockVisible(int blockNumber) const;

  void loadRulesFromPlugin(ISyntaxPlugin *plugin, const Theme &theme);

  QTextCharFormat applyThemeToFormat(const SyntaxRule &rule,
                                     const Theme &theme);

  Theme m_theme;
  QString m_searchKeyword;

  QVector<SyntaxRule> m_rules;

  QVector<MultiLineBlock> m_multiLineBlocks;

  QTextCharFormat m_searchFormat;

  int m_firstVisibleBlock = 0;
  int m_lastVisibleBlock = 1000;

  static constexpr int VIEWPORT_BUFFER = 50;
};

#endif
