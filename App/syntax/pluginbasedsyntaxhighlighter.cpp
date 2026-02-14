#include "pluginbasedsyntaxhighlighter.h"
#include "../core/logging/logger.h"

PluginBasedSyntaxHighlighter::PluginBasedSyntaxHighlighter(
    ISyntaxPlugin *plugin, const Theme &theme, const QString &searchKeyword,
    QTextDocument *parent)
    : QSyntaxHighlighter(parent), m_theme(theme),
      m_searchKeyword(searchKeyword), m_firstVisibleBlock(0),
      m_lastVisibleBlock(1000) {
  if (!plugin) {
    Logger::instance().warning(
        "PluginBasedSyntaxHighlighter created with null plugin");
    return;
  }

  loadRulesFromPlugin(plugin, theme);

  m_searchFormat.setBackground(QColor("#646464"));
}

void PluginBasedSyntaxHighlighter::setSearchKeyword(const QString &keyword) {
  m_searchKeyword = keyword;
  rehighlight();
}

void PluginBasedSyntaxHighlighter::loadRulesFromPlugin(ISyntaxPlugin *plugin,
                                                       const Theme &theme) {
  if (!plugin) {
    return;
  }

  m_rules = plugin->syntaxRules();

  for (SyntaxRule &rule : m_rules) {
    rule.format = applyThemeToFormat(rule, theme);
  }

  m_multiLineBlocks = plugin->multiLineBlocks();

  for (MultiLineBlock &block : m_multiLineBlocks) {

    block.format.setForeground(theme.singleLineCommentFormat);
  }

  Logger::instance().info(
      QString("Loaded %1 rules and %2 multi-line blocks from plugin '%3'")
          .arg(m_rules.size())
          .arg(m_multiLineBlocks.size())
          .arg(plugin->languageName()));
}

QTextCharFormat
PluginBasedSyntaxHighlighter::applyThemeToFormat(const SyntaxRule &rule,
                                                 const Theme &theme) {
  QTextCharFormat format = rule.format;
  QString ruleName = rule.name.toLower();

  if (ruleName.contains("keyword") || ruleName.contains("preprocessor") ||
      ruleName.contains("directive")) {
    if (ruleName.contains("0") || ruleName.contains("primary")) {
      format.setForeground(theme.keywordFormat_0);
      format.setFontWeight(QFont::Bold);
    } else if (ruleName.contains("1") || ruleName.contains("secondary")) {
      format.setForeground(theme.keywordFormat_1);
      format.setFontWeight(QFont::Bold);
    } else if (ruleName.contains("2") || ruleName.contains("tertiary")) {
      format.setForeground(theme.keywordFormat_2);
    } else {
      format.setForeground(theme.keywordFormat_0);
      format.setFontWeight(QFont::Bold);
    }
  } else if (ruleName.contains("number")) {
    format.setForeground(theme.numberFormat);
  } else if (ruleName.contains("string") || ruleName.contains("quotation")) {
    format.setForeground(theme.quotationFormat);
  } else if (ruleName.contains("comment")) {
    format.setForeground(theme.singleLineCommentFormat);
  } else if (ruleName.contains("function")) {
    format.setForeground(theme.functionFormat);
    format.setFontItalic(true);
  } else if (ruleName.contains("class") || ruleName.contains("type") ||
             ruleName.contains("scope") || ruleName.contains("scoped")) {
    format.setForeground(theme.classFormat);
    format.setFontWeight(QFont::Bold);
  }

  return format;
}

void PluginBasedSyntaxHighlighter::highlightBlock(const QString &text) {

  if (text.isEmpty()) {
    setCurrentBlockState(previousBlockState());
    return;
  }

  int blockNum = currentBlock().blockNumber();
  if (!isBlockVisible(blockNum)) {

    setCurrentBlockState(previousBlockState());
    return;
  }

  for (const SyntaxRule &rule : m_rules) {
    QRegularExpressionMatchIterator matchIterator =
        rule.pattern.globalMatch(text);

    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }

  setCurrentBlockState(0);
  for (int i = 0; i < m_multiLineBlocks.size(); ++i) {
    const MultiLineBlock &block = m_multiLineBlocks[i];
    int stateId = i + 1;

    int startIndex = 0;
    if (previousBlockState() != stateId) {
      QRegularExpressionMatch startMatch = block.startPattern.match(text);
      startIndex = startMatch.hasMatch() ? startMatch.capturedStart() : -1;
    }

    while (startIndex >= 0) {

      QRegularExpressionMatch startMatch =
          block.startPattern.match(text, startIndex);
      int searchFrom =
          (previousBlockState() == stateId)
              ? startIndex
              : startIndex +
                    (startMatch.hasMatch() ? startMatch.capturedLength() : 1);

      QRegularExpressionMatch endMatch =
          block.endPattern.match(text, searchFrom);
      int endIndex = endMatch.capturedStart();
      int blockLength = 0;

      if (endIndex == -1) {
        setCurrentBlockState(stateId);
        blockLength = text.length() - startIndex;
      } else {
        blockLength = endIndex - startIndex + endMatch.capturedLength();
      }

      setFormat(startIndex, blockLength, block.format);

      QRegularExpressionMatch nextStart =
          block.startPattern.match(text, startIndex + blockLength);
      startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
    }
  }

  if (!m_searchKeyword.isEmpty()) {
    QRegularExpression searchPattern(m_searchKeyword,
                                     QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator matchIterator =
        searchPattern.globalMatch(text);

    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), m_searchFormat);
    }
  }
}

bool PluginBasedSyntaxHighlighter::isBlockVisible(int blockNumber) const {

  int minBlock = m_firstVisibleBlock - VIEWPORT_BUFFER;
  int maxBlock = m_lastVisibleBlock + VIEWPORT_BUFFER;

  return (blockNumber >= minBlock && blockNumber <= maxBlock);
}
