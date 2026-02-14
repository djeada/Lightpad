#ifndef LIGHTPADSYNTAXHIGHLIGHTER_H
#define LIGHTPADSYNTAXHIGHLIGHTER_H

#include "../settings/theme.h"
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTimer>

class HighlightingRule {

public:
  HighlightingRule(QRegularExpression pattern, QTextCharFormat format);
  QRegularExpression pattern;
  QTextCharFormat format;
};

class LightpadSyntaxHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

public:
  LightpadSyntaxHighlighter(QVector<HighlightingRule> highlightingRules,
                            QRegularExpression commentStartExpression,
                            QRegularExpression commentEndExpression,
                            QTextDocument *parent = nullptr);

  void setVisibleBlockRange(int first, int last) {
    m_firstVisibleBlock = first;
    m_lastVisibleBlock = last;
  }

protected:
  void highlightBlock(const QString &text) override;

private:
  bool isBlockVisible(int blockNumber) const;

  Theme colors;
  QVector<HighlightingRule> highlightingRules;
  QRegularExpression commentStartExpression;
  QRegularExpression commentEndExpression;
  QTextCharFormat multiLineCommentFormat;

  int m_firstVisibleBlock = 0;
  int m_lastVisibleBlock = 1000;

  static constexpr int VIEWPORT_BUFFER = 50;
};

QString cutEndOfLine(QString line);
QVector<HighlightingRule>
highlightingRulesCpp(Theme colors, const QString &searchKeyword = "");
QVector<HighlightingRule>
highlightingRulesJs(Theme colors, const QString &searchKeyword = "");
QVector<HighlightingRule>
highlightingRulesPy(Theme colors, const QString &searchKeyword = "");

#endif
