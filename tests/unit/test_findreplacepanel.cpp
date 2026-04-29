#include <QRegularExpression>
#include <QtTest/QtTest>

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
  void testGlobalResultsPagination();

private:
  QRegularExpression buildSearchPattern(const QString &searchWord,
                                        bool useRegex, bool wholeWords,
                                        bool matchCase) const;
  QString applyPreserveCase(const QString &replaceWord,
                            const QString &matchedText,
                            bool preserveCase) const;

  QPair<int, int> calculateLineColumn(const QString &text, int position) const;

  static constexpr int kGlobalResultsPageSize = 100;
  int globalResultsPageCount(int totalResults) const;
  int pageStart(int page) const;
  int pageEnd(int page, int totalResults) const;
};

QRegularExpression
TestSearchPatterns::buildSearchPattern(const QString &searchWord, bool useRegex,
                                       bool wholeWords, bool matchCase) const {
  QString pattern = searchWord;

  if (!useRegex) {
    pattern = QRegularExpression::escape(pattern);
  }

  if (wholeWords) {
    pattern = "\\b" + pattern + "\\b";
  }

  QRegularExpression::PatternOptions options =
      QRegularExpression::NoPatternOption;

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

    return result.toUpper();
  } else if (allLower && !allUpper) {

    return result.toLower();
  } else if (firstUpper && textLength > 1) {

    result = result.toLower();
    if (!result.isEmpty()) {
      result[0] = result[0].toUpper();
    }
    return result;
  }

  return replaceWord;
}

void TestSearchPatterns::testBasicPattern() {

  QRegularExpression pattern = buildSearchPattern("hello", false, false, false);

  QString text = "Hello World hello HELLO";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  QCOMPARE(count, 3);
}

void TestSearchPatterns::testCaseSensitivePattern() {

  QRegularExpression pattern = buildSearchPattern("hello", false, false, true);

  QString text = "Hello World hello HELLO";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  QCOMPARE(count, 1);
}

void TestSearchPatterns::testWholeWordPattern() {

  QRegularExpression pattern = buildSearchPattern("test", false, true, false);

  QString text = "test testing tested test";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  QCOMPARE(count, 2);
}

void TestSearchPatterns::testRegexPattern() {

  QRegularExpression pattern = buildSearchPattern("\\d+", true, false, false);

  QString text = "abc 123 def 456 ghi";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  QCOMPARE(count, 2);
}

void TestSearchPatterns::testEscapeSpecialCharacters() {

  QRegularExpression pattern =
      buildSearchPattern("test.cpp", false, false, false);

  QString text = "test.cpp testXcpp test.cpp";
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  int count = 0;
  while (matches.hasNext()) {
    matches.next();
    count++;
  }

  QCOMPARE(count, 2);
}

void TestSearchPatterns::testPreserveCase() {

  QString replacement = "world";

  QCOMPARE(applyPreserveCase(replacement, "HELLO", true), "WORLD");

  QCOMPARE(applyPreserveCase(replacement, "hello", true), "world");

  QCOMPARE(applyPreserveCase(replacement, "Hello", true), "World");

  QCOMPARE(applyPreserveCase(replacement, "HELLO", false), "world");
}

QPair<int, int> TestSearchPatterns::calculateLineColumn(const QString &text,
                                                        int position) const {
  QStringList lines = text.split('\n');

  QVector<int> lineStarts;
  int pos = 0;
  for (const QString &line : lines) {
    lineStarts.append(pos);
    pos += line.length() + 1;
  }

  int lineNum = 0;
  for (int j = 0; j < lineStarts.size(); ++j) {
    if (j + 1 < lineStarts.size() && position >= lineStarts[j + 1]) {
      continue;
    }
    lineNum = j;
    break;
  }

  int columnNum = position - lineStarts[lineNum] + 1;
  return qMakePair(lineNum + 1, columnNum);
}

void TestSearchPatterns::testSearchResultsLineCalculation() {

  QString text =
      "first line\nsecond line with test\nthird line\nfourth test line";

  QRegularExpression pattern = buildSearchPattern("test", false, false, false);
  QRegularExpressionMatchIterator matches = pattern.globalMatch(text);

  QVector<QPair<int, int>> results;
  while (matches.hasNext()) {
    QRegularExpressionMatch match = matches.next();
    results.append(calculateLineColumn(text, match.capturedStart()));
  }

  QCOMPARE(results.size(), 2);

  QCOMPARE(results[0].first, 2);
  QCOMPARE(results[0].second, 18);

  QCOMPARE(results[1].first, 4);
  QCOMPARE(results[1].second, 8);
}

int TestSearchPatterns::globalResultsPageCount(int totalResults) const {
  if (totalResults <= 0) {
    return 0;
  }
  return (totalResults + kGlobalResultsPageSize - 1) / kGlobalResultsPageSize;
}

int TestSearchPatterns::pageStart(int page) const {
  return page * kGlobalResultsPageSize;
}

int TestSearchPatterns::pageEnd(int page, int totalResults) const {
  return qMin((page + 1) * kGlobalResultsPageSize, totalResults);
}

void TestSearchPatterns::testGlobalResultsPagination() {
  // Zero results -> zero pages
  QCOMPARE(globalResultsPageCount(0), 0);

  // Fewer results than one page
  QCOMPARE(globalResultsPageCount(50), 1);

  // Exactly one page
  QCOMPARE(globalResultsPageCount(100), 1);

  // Just over one page
  QCOMPARE(globalResultsPageCount(101), 2);

  // Multiple complete pages
  QCOMPARE(globalResultsPageCount(300), 3);

  // Partial last page
  QCOMPARE(globalResultsPageCount(250), 3);

  // Page 0 range for 250 results
  QCOMPARE(pageStart(0), 0);
  QCOMPARE(pageEnd(0, 250), 100);

  // Page 1 range for 250 results
  QCOMPARE(pageStart(1), 100);
  QCOMPARE(pageEnd(1, 250), 200);

  // Last page range for 250 results
  QCOMPARE(pageStart(2), 200);
  QCOMPARE(pageEnd(2, 250), 250);

  // Single result on last page
  QCOMPARE(pageStart(1), 100);
  QCOMPARE(pageEnd(1, 101), 101);

  // Navigation: result at index 150 is on page 1
  QCOMPARE(150 / kGlobalResultsPageSize, 1);
  // Its local index within the page is 50
  QCOMPARE(150 - pageStart(1), 50);

  // Result at index 0 is on page 0
  QCOMPARE(0 / kGlobalResultsPageSize, 0);

  // Result at index 99 is still on page 0
  QCOMPARE(99 / kGlobalResultsPageSize, 0);

  // Result at index 100 is on page 1
  QCOMPARE(100 / kGlobalResultsPageSize, 1);
}

QTEST_MAIN(TestSearchPatterns)
#include "test_findreplacepanel.moc"
