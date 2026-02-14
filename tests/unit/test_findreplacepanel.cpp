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

private:
  QRegularExpression buildSearchPattern(const QString &searchWord,
                                        bool useRegex, bool wholeWords,
                                        bool matchCase) const;
  QString applyPreserveCase(const QString &replaceWord,
                            const QString &matchedText,
                            bool preserveCase) const;

  QPair<int, int> calculateLineColumn(const QString &text, int position) const;
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

QTEST_MAIN(TestSearchPatterns)
#include "test_findreplacepanel.moc"
