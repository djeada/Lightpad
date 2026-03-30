#include "core/editor/texttransforms.h"
#include <QtTest/QtTest>

class TestTextTransforms : public QObject {
  Q_OBJECT

private slots:
  void testSortLinesAscending();
  void testSortLinesDescending();
  void testToUppercase();
  void testToLowercase();
  void testToTitleCase();
  void testToTitleCaseSpecialDelimiters();
  void testRemoveDuplicateLines();
  void testRemoveDuplicateLinesPreservesOrder();
  void testReverseLines();
  void testTrimLines();
  void testEmptyInput();
  void testSingleLine();
};

void TestTextTransforms::testSortLinesAscending() {
  QString input = "banana\napple\ncherry";
  QString expected = "apple\nbanana\ncherry";
  QCOMPARE(TextTransforms::sortLinesAscending(input), expected);
}

void TestTextTransforms::testSortLinesDescending() {
  QString input = "banana\napple\ncherry";
  QString expected = "cherry\nbanana\napple";
  QCOMPARE(TextTransforms::sortLinesDescending(input), expected);
}

void TestTextTransforms::testToUppercase() {
  QCOMPARE(TextTransforms::toUppercase("hello world"), QString("HELLO WORLD"));
  QCOMPARE(TextTransforms::toUppercase("Hello 123"), QString("HELLO 123"));
}

void TestTextTransforms::testToLowercase() {
  QCOMPARE(TextTransforms::toLowercase("HELLO WORLD"), QString("hello world"));
  QCOMPARE(TextTransforms::toLowercase("Hello 123"), QString("hello 123"));
}

void TestTextTransforms::testToTitleCase() {
  QCOMPARE(TextTransforms::toTitleCase("hello world"), QString("Hello World"));
  QCOMPARE(TextTransforms::toTitleCase("HELLO WORLD"), QString("Hello World"));
}

void TestTextTransforms::testToTitleCaseSpecialDelimiters() {
  QCOMPARE(TextTransforms::toTitleCase("hello-world"), QString("Hello-World"));
  QCOMPARE(TextTransforms::toTitleCase("hello_world"), QString("Hello_World"));
  QCOMPARE(TextTransforms::toTitleCase("hello.world"), QString("Hello.World"));
  QCOMPARE(TextTransforms::toTitleCase("path/to/file"),
           QString("Path/To/File"));
}

void TestTextTransforms::testRemoveDuplicateLines() {
  QString input = "line1\nline2\nline1\nline3\nline2";
  QString expected = "line1\nline2\nline3";
  QCOMPARE(TextTransforms::removeDuplicateLines(input), expected);
}

void TestTextTransforms::testRemoveDuplicateLinesPreservesOrder() {
  QString input = "charlie\nalpha\nbravo\nalpha\ncharlie";
  QString expected = "charlie\nalpha\nbravo";
  QCOMPARE(TextTransforms::removeDuplicateLines(input), expected);
}

void TestTextTransforms::testReverseLines() {
  QString input = "first\nsecond\nthird";
  QString expected = "third\nsecond\nfirst";
  QCOMPARE(TextTransforms::reverseLines(input), expected);
}

void TestTextTransforms::testTrimLines() {
  QString input = "  hello  \n  world  \n  test  ";
  QString expected = "hello\nworld\ntest";
  QCOMPARE(TextTransforms::trimLines(input), expected);
}

void TestTextTransforms::testEmptyInput() {
  QCOMPARE(TextTransforms::sortLinesAscending(""), QString(""));
  QCOMPARE(TextTransforms::sortLinesDescending(""), QString(""));
  QCOMPARE(TextTransforms::toUppercase(""), QString(""));
  QCOMPARE(TextTransforms::toLowercase(""), QString(""));
  QCOMPARE(TextTransforms::toTitleCase(""), QString(""));
  QCOMPARE(TextTransforms::removeDuplicateLines(""), QString(""));
  QCOMPARE(TextTransforms::reverseLines(""), QString(""));
  QCOMPARE(TextTransforms::trimLines(""), QString(""));
}

void TestTextTransforms::testSingleLine() {
  QCOMPARE(TextTransforms::sortLinesAscending("single"), QString("single"));
  QCOMPARE(TextTransforms::reverseLines("single"), QString("single"));
  QCOMPARE(TextTransforms::removeDuplicateLines("single"), QString("single"));
}

QTEST_MAIN(TestTextTransforms)
#include "test_texttransforms.moc"
