#include "pluginbasedsyntaxhighlighter.h"
#include "../core/logging/logger.h"
#include <QBitArray>
#include <functional>

PluginBasedSyntaxHighlighter::PluginBasedSyntaxHighlighter(
    ISyntaxPlugin *plugin, const Theme &theme, const QString &searchKeyword,
    QTextDocument *parent)
    : QSyntaxHighlighter(parent), m_theme(theme),
      m_searchKeyword(searchKeyword), m_firstVisibleBlock(-1),
      m_lastVisibleBlock(-1) {
  if (!plugin) {
    Logger::instance().warning(
        "PluginBasedSyntaxHighlighter created with null plugin");
    return;
  }

  loadRulesFromPlugin(plugin, theme);

  m_searchFormat.setBackground(QColor("#646464"));
}

void PluginBasedSyntaxHighlighter::setSearchPattern(
    const QRegularExpression &pattern) {
  m_searchPattern = pattern;
  rehighlight();
}

void PluginBasedSyntaxHighlighter::setSearchKeyword(const QString &keyword) {
  m_searchKeyword = keyword;
  m_searchPattern = QRegularExpression();
  rehighlight();
}

void PluginBasedSyntaxHighlighter::setVisibleBlockRange(int first, int last) {
  if (!document()) {
    return;
  }

  int newFirst = qMax(0, first);
  int newLast = qMax(newFirst, last);

  const int blockCount = document()->blockCount();
  const bool bulkEdit =
      (m_lastBlockCount >= 0 && qAbs(blockCount - m_lastBlockCount) > 1);
  m_lastBlockCount = blockCount;

  if (newFirst == m_firstVisibleBlock && newLast == m_lastVisibleBlock &&
      !bulkEdit) {
    return;
  }

  bool wasInitialized = (m_firstVisibleBlock >= 0 && m_lastVisibleBlock >= 0);
  int oldFirst = m_firstVisibleBlock;
  int oldLast = m_lastVisibleBlock;
  m_firstVisibleBlock = newFirst;
  m_lastVisibleBlock = newLast;

  if (!wasInitialized) {
    rehighlightBlockRange(0, m_lastVisibleBlock + VIEWPORT_BUFFER);
    return;
  }

  int oldMin = oldFirst - VIEWPORT_BUFFER;
  int oldMax = oldLast + VIEWPORT_BUFFER;
  int newMin = m_firstVisibleBlock - VIEWPORT_BUFFER;
  int newMax = m_lastVisibleBlock + VIEWPORT_BUFFER;

  if (bulkEdit) {
    rehighlightBlockRange(0, newMax);
    return;
  }

  if (newMin < oldMin) {
    rehighlightBlockRange(0, qMin(oldMin - 1, newMax));
  }
  if (newMax > oldMax) {
    rehighlightBlockRange(0, newMax);
  }
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
    SyntaxRule syntheticRule;
    syntheticRule.name = block.name;
    block.format = applyThemeToFormat(syntheticRule, theme);
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

  if (ruleName.contains("function") && ruleName.contains("definition")) {
    format.setForeground(theme.functionFormat);
    format.setFontItalic(false);
    format.setFontWeight(QFont::Bold);
  } else if ((ruleName.contains("class") || ruleName.contains("type")) &&
             ruleName.contains("definition")) {
    format.setForeground(theme.classFormat);
    format.setFontWeight(QFont::Bold);
    format.setFontItalic(false);
  } else if (ruleName.contains("decorator")) {
    format.setForeground(theme.keywordFormat_1);
    format.setFontItalic(false);
    format.setFontWeight(QFont::Normal);
  } else if (ruleName.contains("builtin")) {
    if (ruleName.contains("type") || ruleName.contains("class")) {
      format.setForeground(theme.classFormat);
    } else if (ruleName.contains("function")) {
      format.setForeground(theme.keywordFormat_1);
    } else {
      format.setForeground(theme.constantFormat);
    }
    format.setFontItalic(false);
    format.setFontWeight(QFont::Normal);
  } else if (ruleName.contains("magic")) {
    format.setForeground(theme.constantFormat);
    format.setFontItalic(false);
    format.setFontWeight(QFont::Normal);
  } else if (ruleName.contains("attribute") || ruleName.contains("member") ||
             ruleName.contains("property") || ruleName.contains("field")) {
    format.setForeground(theme.keywordFormat_2);
    format.setFontItalic(false);
    format.setFontWeight(QFont::Normal);
  } else if (ruleName.contains("keyword") ||
             ruleName.contains("preprocessor") ||
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
  } else if (ruleName.contains("escape")) {
    format.setForeground(theme.escapeFormat);
  } else if (ruleName.contains("regex")) {
    format.setForeground(theme.regexFormat);
  } else if (ruleName.contains("number")) {
    format.setForeground(theme.numberFormat);
  } else if (ruleName.contains("string") || ruleName.contains("quotation")) {
    format.setForeground(theme.quotationFormat);
  } else if (ruleName.contains("comment")) {
    format.setForeground(theme.singleLineCommentFormat);
  } else if (ruleName.contains("operator")) {
    format.setForeground(theme.operatorFormat);
  } else if (ruleName.contains("constant")) {
    format.setForeground(theme.constantFormat);
  } else if (ruleName.contains("function")) {
    format.setForeground(theme.functionFormat);
    format.setFontItalic(false);
    format.setFontWeight(QFont::Normal);
  } else if (ruleName.contains("class") || ruleName.contains("type") ||
             ruleName.contains("scope") || ruleName.contains("scoped")) {
    format.setForeground(theme.classFormat);
    format.setFontItalic(false);
    format.setFontWeight(QFont::Normal);
  }

  return format;
}

void PluginBasedSyntaxHighlighter::highlightBlock(const QString &text) {

  if (text.isEmpty()) {
    setCurrentBlockState(previousBlockState());
    return;
  }

  int blockNum = currentBlock().blockNumber();
  const bool shouldFormat = isBlockVisible(blockNum);

  QBitArray protectedCharacters(text.size());
  QBitArray stringCharacters(text.size());

  auto applyFormatRange = [&](int start, int length,
                              const QTextCharFormat &format, bool protect,
                              QBitArray *trackedCharacters = nullptr,
                              bool allowProtected = false,
                              const QBitArray *requiredMask = nullptr) {
    if (start < 0 || length <= 0) {
      return;
    }

    int clampedStart = qMax(0, start);
    int clampedEnd = qMin(clampedStart + length, text.size());
    int segmentStart = -1;

    for (int position = clampedStart; position < clampedEnd; ++position) {
      const bool isProtected = protectedCharacters.testBit(position);
      const bool inRequiredMask =
          !requiredMask || requiredMask->testBit(position);
      const bool canApply = inRequiredMask && (!isProtected || allowProtected);

      if (canApply) {
        if (segmentStart < 0) {
          segmentStart = position;
        }
        if (protect) {
          protectedCharacters.setBit(position);
        }
        if (trackedCharacters) {
          trackedCharacters->setBit(position);
        }
      } else if (segmentStart >= 0) {
        setFormat(segmentStart, position - segmentStart, format);
        segmentStart = -1;
      }
    }

    if (segmentStart >= 0) {
      setFormat(segmentStart, clampedEnd - segmentStart, format);
    }
  };

  auto isStringLikeRule = [](const QString &ruleName) {
    QString normalized = ruleName.toLower();
    return normalized.contains("string") || normalized.contains("quotation");
  };

  auto isInterpolatedStringRule = [](const QString &ruleName) {
    return ruleName.toLower().contains("interpolated");
  };

  auto isEscapeLikeRule = [](const QString &ruleName) {
    return ruleName.toLower().contains("escape");
  };

  auto applyInterpolatedStringRange = [&](int start, int length,
                                          const QTextCharFormat &format,
                                          QBitArray *trackedCharacters) {
    if (start < 0 || length <= 0) {
      return;
    }

    int end = qMin(start + length, text.size());
    int literalStart = start;
    int expressionDepth = 0;

    for (int position = start; position < end; ++position) {
      QChar character = text.at(position);

      if (character == '{') {
        if (position + 1 < end && text.at(position + 1) == '{' &&
            expressionDepth == 0) {
          ++position;
          continue;
        }

        if (expressionDepth == 0) {
          applyFormatRange(literalStart, position - literalStart + 1, format,
                           true, trackedCharacters);
          literalStart = position + 1;
        }
        ++expressionDepth;
        continue;
      }

      if (character == '}' && expressionDepth > 0) {
        --expressionDepth;
        if (expressionDepth == 0) {
          applyFormatRange(position, 1, format, true, trackedCharacters);
          literalStart = position + 1;
        }
      }
    }

    if (literalStart < end) {
      applyFormatRange(literalStart, end - literalStart, format, true,
                       trackedCharacters);
    }
  };

  auto isCommentLikeRule = [](const QString &ruleName) {
    QString normalized = ruleName.toLower();
    return normalized.contains("comment");
  };

  auto applyRules = [&](const std::function<bool(const QString &)> &predicate,
                        bool protect, QBitArray *trackedCharacters = nullptr,
                        bool allowProtected = false,
                        const QBitArray *requiredMask = nullptr) {
    for (const SyntaxRule &rule : m_rules) {
      if (!predicate(rule.name)) {
        continue;
      }

      QRegularExpressionMatchIterator matchIterator =
          rule.pattern.globalMatch(text);

      while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        int start = match.capturedStart(rule.captureGroup);
        int length = match.capturedLength(rule.captureGroup);
        if (start < 0 || length <= 0) {
          continue;
        }
        if (protect && isInterpolatedStringRule(rule.name)) {
          applyInterpolatedStringRange(start, length, rule.format,
                                       trackedCharacters);
        } else {
          applyFormatRange(start, length, rule.format, protect,
                           trackedCharacters, allowProtected, requiredMask);
        }
      }
    }
  };

  setCurrentBlockState(0);

  if (!m_multiLineBlocks.isEmpty()) {
    struct PendingMatch {
      int start = -2;
      int length = 0;
    };

    auto nextMatch = [&](const QRegularExpression &pattern, int captureGroup,
                         int from, PendingMatch &cached, bool requireLength) {
      if (cached.start == -1 || cached.start >= from) {
        return;
      }
      int offset = from;
      while (offset <= text.size()) {
        QRegularExpressionMatch match = pattern.match(text, offset);
        if (!match.hasMatch()) {
          break;
        }
        const int matchStart = match.capturedStart(captureGroup);
        const int matchLength = match.capturedLength(captureGroup);
        if (matchStart >= from && (!requireLength || matchLength > 0)) {
          cached.start = matchStart;
          cached.length = matchLength;
          return;
        }
        offset = qMax(match.capturedStart() + 1, match.capturedEnd());
      }
      cached.start = -1;
    };

    QVector<const SyntaxRule *> skipRules;
    for (const SyntaxRule &rule : m_rules) {
      if (isStringLikeRule(rule.name) || isCommentLikeRule(rule.name)) {
        skipRules.append(&rule);
      }
    }
    QVector<PendingMatch> skipMatches(skipRules.size());
    QVector<PendingMatch> blockStarts(m_multiLineBlocks.size());

    int activeBlock = previousBlockState() - 1;
    if (activeBlock >= m_multiLineBlocks.size()) {
      activeBlock = -1;
    }

    int position = 0;
    while (position <= text.size()) {
      int regionStart = 0;
      int searchFrom = 0;

      if (activeBlock < 0) {
        int blockStart = -1;
        int blockLength = 0;
        for (int i = 0; i < m_multiLineBlocks.size(); ++i) {
          nextMatch(m_multiLineBlocks[i].startPattern, 0, position,
                    blockStarts[i], false);
          if (blockStarts[i].start >= 0 &&
              (blockStart < 0 || blockStarts[i].start < blockStart)) {
            blockStart = blockStarts[i].start;
            blockLength = blockStarts[i].length;
            activeBlock = i;
          }
        }
        if (blockStart < 0) {
          break;
        }

        int skipStart = -1;
        int skipEnd = -1;
        for (int i = 0; i < skipRules.size(); ++i) {
          nextMatch(skipRules[i]->pattern, skipRules[i]->captureGroup, position,
                    skipMatches[i], true);
          const PendingMatch &match = skipMatches[i];
          if (match.start >= 0 && match.start < blockStart &&
              (skipStart < 0 || match.start < skipStart ||
               (match.start == skipStart &&
                match.start + match.length > skipEnd))) {
            skipStart = match.start;
            skipEnd = match.start + match.length;
          }
        }
        if (skipStart >= 0) {
          activeBlock = -1;
          position = skipEnd;
          continue;
        }

        regionStart = blockStart;
        searchFrom = blockStart + blockLength;
      }

      const MultiLineBlock &block = m_multiLineBlocks[activeBlock];
      QRegularExpressionMatch endMatch =
          block.endPattern.match(text, searchFrom);
      const int regionEnd =
          endMatch.hasMatch() ? endMatch.capturedEnd() : text.size();

      if (shouldFormat) {
        applyFormatRange(
            regionStart, regionEnd - regionStart, block.format, true,
            isStringLikeRule(block.name) ? &stringCharacters : nullptr);
      }

      if (!endMatch.hasMatch()) {
        setCurrentBlockState(activeBlock + 1);
        break;
      }

      activeBlock = -1;
      position = qMax(regionEnd, regionStart + 1);
    }
  }

  if (!shouldFormat) {
    return;
  }

  applyRules(isStringLikeRule, true, &stringCharacters);
  applyRules(isEscapeLikeRule, false, nullptr, true, &stringCharacters);
  applyRules(isCommentLikeRule, true);
  applyRules(
      [&](const QString &ruleName) {
        return !isStringLikeRule(ruleName) && !isCommentLikeRule(ruleName) &&
               !isEscapeLikeRule(ruleName);
      },
      false);

  if (!m_searchKeyword.isEmpty()) {

    const QRegularExpression searchPattern =
        m_searchPattern.isValid() && !m_searchPattern.pattern().isEmpty()
            ? m_searchPattern
            : QRegularExpression(QRegularExpression::escape(m_searchKeyword),
                                 QRegularExpression::CaseInsensitiveOption);
    if (!searchPattern.isValid()) {
      return;
    }
    QRegularExpressionMatchIterator matchIterator =
        searchPattern.globalMatch(text);

    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), m_searchFormat);
    }
  }
}

bool PluginBasedSyntaxHighlighter::isBlockVisible(int blockNumber) const {
  if (m_firstVisibleBlock < 0 || m_lastVisibleBlock < 0) {
    return true;
  }

  int minBlock = m_firstVisibleBlock - VIEWPORT_BUFFER;
  int maxBlock = m_lastVisibleBlock + VIEWPORT_BUFFER;

  return (blockNumber >= minBlock && blockNumber <= maxBlock);
}

void PluginBasedSyntaxHighlighter::rehighlightBlockRange(int firstBlock,
                                                         int lastBlock) {
  if (!document()) {
    return;
  }

  int blockCount = document()->blockCount();
  if (blockCount <= 0) {
    return;
  }

  int clampedFirst = qBound(0, firstBlock, blockCount - 1);
  int clampedLast = qBound(0, lastBlock, blockCount - 1);
  if (clampedFirst > clampedLast) {
    return;
  }

  QTextBlock block = document()->findBlockByNumber(clampedFirst);
  for (int current = clampedFirst; block.isValid() && current <= clampedLast;
       ++current, block = block.next()) {
    rehighlightBlock(block);
  }
}
