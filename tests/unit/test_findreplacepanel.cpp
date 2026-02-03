#include <QRegularExpression>
#include <QtTest/QtTest>

// Test the search pattern building logic without depending on the full
// FindReplacePanel
class TestSearchPatterns : public QObject {
  Q_OBJECT

private slots:
  void testBasicPattern();
  void testCaseSensitivePattern();
  void testWholeWordPattern();
  void testRegexPattern();
  void testEscapeSpecialCharacters();
  void testPreserveCase();
  void testSearchResultsLineCalculation();

private:
  // Helper functions that mirror the FindReplacePanel logic
  QRegularExpression buildSearchPattern(const QString &searchWord,
                                        bool useRegex, bool wholeWords,
                                        bool matchCase) const;
  QString applyPreserveCase(const QString &replaceWord,
                            const QString &matchedText,
                            bool preserveCase) const;

  // Helper to calculate line/column from position (mirrors local search results
  // logic)
  QPair<int, int> calculateLineColumn(const QString &text, int position) const;
};

QRegularExpression
TestSearchPatterns::buildSearchPattern(const QString &searchWord, bool useRegex,
                                       bool wholeWords, bool matchCase) const {
  QString pattern = searchWord;

  // Escape regex special characters unless using regex mode
  if (!useRegex) {
    pattern = QRegularExpression::escape(pattern);
  }

  // Add whole word boundaries if required
  if (wholeWords) {
    pattern = "\\b" + pattern + "\\b";
  }

  QRegularExpression::PatternOptions options =
      QRegularExpression::NoPatternOption;

  // Case sensitivity
  if (!matchCase) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  return QRegularExpression(pattern, options);
}

QString TestSearchPatterns::applyPreserveCase(const QString &replaceWord,
                                              const QString &matchedText,
                                              bool preserveCase) const {
  if (!preserveCase || matchedText.isEmpty()) {
    return replaceWord;
  }

  QString result = replaceWord;

  // Check if matched text is all uppercase
  bool allUpper = true;
  bool allLower = true;
  bool firstUpper = false;

  const int textLength = matchedText.length();
  for (int i = 0; i < textLength; ++i) {
    QChar c = matchedText.at(i);
    if (c.isLetter()) {
      if (c.isUpper()) {
        allLower = false;
        if (i == 0)
          firstUpper = true;
      } else {
        allUpper = false;
      }
    }
  }

  if (allUpper && !allLower) {
    // Matched text is all uppercase, make replacement all uppercase
    return result.toUpper();
  } else if (allLower && !allUpper) {
    // Matched text is all lowercase, make replacement all lowercase
    return result.toLower();
  } else if (firstUpper && textLength > 1) {
    // Title case: first letter uppercase, rest lowercase
    result = result.toLower();
    if (!result.isEmpty()) {
      result[0] = result[0].toUpper();
    }
    return result;
  }

  return replaceWord;
}

void TestSearchPatterns::testBasicPattern() {
  // Basic search without any options
  QRegularExpression pattern = buildSearchPattern("hello", false, false, false);

  QString text = "Hello World hello HELLO";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  // Should find all 3 occurrences (case insensitive by default)
  QCOMPARE(count, 3);
}

void TestSearchPatterns::testCaseSensitivePattern() {
  // Case sensitive search
  QRegularExpression pattern = buildSearchPattern("hello", false, false, true);

  QString text = "Hello World hello HELLO";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  // Should find only 1 occurrence (lowercase "hello")
  QCOMPARE(count, 1);
}

void TestSearchPatterns::testWholeWordPattern() {
  // Whole word search
  QRegularExpression pattern = buildSearchPattern("test", false, true, false);

  QString text = "test testing tested test";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  // Should find only 2 occurrences (standalone "test")
  QCOMPARE(count, 2);
}

void TestSearchPatterns::testRegexPattern() {
  // Regex pattern
  QRegularExpression pattern = buildSearchPattern("\\d+", true, false, false);

  QString text = "abc 123 def 456 ghi";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  // Should find 2 number sequences
  QCOMPARE(count, 2);
}

void TestSearchPatterns::testEscapeSpecialCharacters() {
  // Special characters should be escaped in non-regex mode
  QRegularExpression pattern =
      buildSearchPattern("test.cpp", false, false, false);

  QString text = "test.cpp testXcpp test.cpp";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  // Should find only 2 occurrences (the literal "test.cpp", not "testXcpp")
  QCOMPARE(count, 2);
}

void TestSearchPatterns::testPreserveCase() {
  // Test preserve case functionality
  QString replacement = "world";

  // All uppercase matched text
  QCOMPARE(applyPreserveCase(replacement, "HELLO", true), "WORLD");

  // All lowercase matched text
  QCOMPARE(applyPreserveCase(replacement, "hello", true), "world");

  // Title case matched text
  QCOMPARE(applyPreserveCase(replacement, "Hello", true), "World");

  // Preserve case disabled
  QCOMPARE(applyPreserveCase(replacement, "HELLO", false), "world");
}

QPair<int, int> TestSearchPatterns::calculateLineColumn(const QString &text,
                                                        int position) const {
  QStringList lines = text.split('\n');

  // Build line start positions
  QVector<int> lineStarts;
  int pos = 0;
  for (const QString &line : lines) {
    lineStarts.append(pos);
    pos += line.length() + 1; // +1 for newline
  }

  // Find line number for this position
  int lineNum = 0;
  for (int j = 0; j < lineStarts.size(); ++j) {
    if (j + 1 < lineStarts.size() && position >= lineStarts[j + 1]) {
      continue;
    }
    lineNum = j;
    break;
  }

  int columnNum = position - lineStarts[lineNum] + 1; // 1-based
  return qMakePair(lineNum + 1, columnNum);           // 1-based line number
}

void TestSearchPatterns::testSearchResultsLineCalculation() {
  // Test that search results correctly calculate line and column numbers
  QString text =
      "first line\nsecond line with test\nthird line\nfourth test line";

  // Search for "test"
  QRegularExpression pattern = buildSearchPattern("test", false, false, false);
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  QVector<QPair<int, int>> results;
  while (matches.hasNext()) {
    QRegularExpressionMatch match = matches.next();
    results.append(calculateLineColumn(text, match.capturedStart()));
  }

  // Should find 2 occurrences
  QCOMPARE(results.size(), 2);

  // First "test" is on line 2 (1-based), column 18
  QCOMPARE(results[0].first, 2);   // line 2
  QCOMPARE(results[0].second, 18); // column 18 (after "second line with ")

  // Second "test" is on line 4 (1-based), column 8
  QCOMPARE(results[1].first, 4);  // line 4
  QCOMPARE(results[1].second, 8); // column 8 (after "fourth ")
}

QTEST_MAIN(TestSearchPatterns)
#include "test_findreplacepanel.moc"
