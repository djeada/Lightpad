#include "markdown/markdowntools.h"
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>

class TestMarkdownTools : public QObject {
  Q_OBJECT

private slots:
  void testExtractHeadings();
  void testExtractHeadingsIgnoresFencedBlocks();
  void testGenerateAnchor();
  void testGenerateToc();
  void testInsertOrUpdateToc();
  void testUpdateExistingToc();
  void testToggleCheckbox();
  void testCountTasks();
  void testFormatTable();
  void testNormalizeListNumbering();
  void testLintDuplicateHeadings();
  void testLintInconsistentHeadingLevels();
  void testLintMissingAltText();
  void testLintTrailingSpaces();
  void testLintBrokenLocalLinks();
  void testToHtmlHeadings();
  void testToHtmlInlineFormatting();
  void testToHtmlCodeBlocks();
  void testToHtmlLists();
  void testToHtmlTables();
  void testToHtmlCheckboxes();
  void testToHtmlBlockquotes();
  void testToHtmlWithBasePath();
};

void TestMarkdownTools::testExtractHeadings() {
  QString md = "# Title\n\nSome text\n\n## Section 1\n\nMore "
               "text\n\n### Subsection\n\n#### Deep";
  QList<MarkdownHeading> headings = MarkdownTools::extractHeadings(md);

  QCOMPARE(headings.size(), 4);
  QCOMPARE(headings[0].level, 1);
  QCOMPARE(headings[0].text, "Title");
  QCOMPARE(headings[0].lineNumber, 0);
  QCOMPARE(headings[1].level, 2);
  QCOMPARE(headings[1].text, "Section 1");
  QCOMPARE(headings[2].level, 3);
  QCOMPARE(headings[2].text, "Subsection");
  QCOMPARE(headings[3].level, 4);
  QCOMPARE(headings[3].text, "Deep");
}

void TestMarkdownTools::testExtractHeadingsIgnoresFencedBlocks() {
  QString md = "# Real Heading\n\n```\n# Not A Heading\n```\n\n## Another Real";
  QList<MarkdownHeading> headings = MarkdownTools::extractHeadings(md);

  QCOMPARE(headings.size(), 2);
  QCOMPARE(headings[0].text, "Real Heading");
  QCOMPARE(headings[1].text, "Another Real");
}

void TestMarkdownTools::testGenerateAnchor() {
  QCOMPARE(MarkdownTools::generateAnchor("Hello World"), QString("hello-world"));
  QCOMPARE(MarkdownTools::generateAnchor("Section 1.2"), QString("section-12"));
  QCOMPARE(MarkdownTools::generateAnchor("C++ Guide"), QString("c-guide"));
  QCOMPARE(MarkdownTools::generateAnchor("simple"), QString("simple"));
}

void TestMarkdownTools::testGenerateToc() {
  QString md = "# Title\n\n## Section A\n\n## Section B\n\n### Sub B1";
  QString toc = MarkdownTools::generateToc(md, 3);

  QVERIFY(toc.contains("- [Title](#title)"));
  QVERIFY(toc.contains("  - [Section A](#section-a)"));
  QVERIFY(toc.contains("  - [Section B](#section-b)"));
  QVERIFY(toc.contains("    - [Sub B1](#sub-b1)"));
}

void TestMarkdownTools::testInsertOrUpdateToc() {
  QString md = "# Title\n\n## Section A\n\n## Section B";
  QString result = MarkdownTools::insertOrUpdateToc(md);

  QVERIFY(result.contains("<!-- TOC -->"));
  QVERIFY(result.contains("<!-- /TOC -->"));
  QVERIFY(result.contains("[Title]"));
  QVERIFY(result.contains("[Section A]"));
}

void TestMarkdownTools::testUpdateExistingToc() {
  QString md = "<!-- TOC -->\n\nOld content\n\n<!-- /TOC -->\n\n# Title\n\n## "
               "New Section";
  QString result = MarkdownTools::insertOrUpdateToc(md);

  QVERIFY(!result.contains("Old content"));
  QVERIFY(result.contains("[Title]"));
  QVERIFY(result.contains("[New Section]"));
}

void TestMarkdownTools::testToggleCheckbox() {
  QCOMPARE(MarkdownTools::toggleCheckbox("- [ ] Task"),
           QString("- [x] Task"));
  QCOMPARE(MarkdownTools::toggleCheckbox("- [x] Done"),
           QString("- [ ] Done"));
  QCOMPARE(MarkdownTools::toggleCheckbox("- [X] Also done"),
           QString("- [ ] Also done"));
  QCOMPARE(MarkdownTools::toggleCheckbox("Not a checkbox"),
           QString("Not a checkbox"));
  QCOMPARE(MarkdownTools::toggleCheckbox("  * [ ] Indented"),
           QString("  * [x] Indented"));
}

void TestMarkdownTools::testCountTasks() {
  QString md = "- [ ] Task 1\n- [x] Task 2\n- [ ] Task 3\n- [X] Task 4";
  int completed = 0;
  int total = MarkdownTools::countTasks(md, &completed);
  QCOMPARE(total, 4);
  QCOMPARE(completed, 2);
}

void TestMarkdownTools::testFormatTable() {
  QString table = "| a | bb |\n| --- | --- |\n| ccc | d |";
  QString formatted = MarkdownTools::formatTable(table);

  QVERIFY(formatted.contains("| a   |"));
  QVERIFY(formatted.contains("| ccc |"));

  QStringList lines = formatted.split('\n');
  QCOMPARE(lines.size(), 3);
  for (const QString &line : lines) {
    QVERIFY(line.startsWith('|'));
    QVERIFY(line.endsWith('|'));
  }
}

void TestMarkdownTools::testNormalizeListNumbering() {
  QString md = "3. First\n7. Second\n2. Third";
  QString result = MarkdownTools::normalizeListNumbering(md);

  QVERIFY(result.startsWith("1. First"));
  QVERIFY(result.contains("2. Second"));
  QVERIFY(result.contains("3. Third"));
}

void TestMarkdownTools::testLintDuplicateHeadings() {
  QString md = "# Title\n\n## Section\n\n## Section";
  QList<LspDiagnostic> diags = MarkdownTools::lint(md);

  bool foundDuplicate = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MD024") {
      foundDuplicate = true;
      QCOMPARE(d.severity, LspDiagnosticSeverity::Warning);
      QVERIFY(d.message.contains("Duplicate heading"));
      break;
    }
  }
  QVERIFY(foundDuplicate);
}

void TestMarkdownTools::testLintInconsistentHeadingLevels() {
  QString md = "# Title\n\n### Skipped Level";
  QList<LspDiagnostic> diags = MarkdownTools::lint(md);

  bool foundSkipped = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MD001") {
      foundSkipped = true;
      QCOMPARE(d.severity, LspDiagnosticSeverity::Warning);
      QVERIFY(d.message.contains("Heading level skipped"));
      break;
    }
  }
  QVERIFY(foundSkipped);
}

void TestMarkdownTools::testLintMissingAltText() {
  QString md = "![](image.png)\n\n![good alt](other.png)";
  QList<LspDiagnostic> diags = MarkdownTools::lint(md);

  bool foundMissingAlt = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MD045") {
      foundMissingAlt = true;
      QVERIFY(d.message.contains("no alt text"));
      break;
    }
  }
  QVERIFY(foundMissingAlt);
}

void TestMarkdownTools::testLintTrailingSpaces() {
  QString md = "Line with trailing spaces   \nClean line\nAnother dirty line   ";
  QList<LspDiagnostic> diags = MarkdownTools::lint(md);

  int trailingCount = 0;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MD009")
      ++trailingCount;
  }
  QVERIFY(trailingCount >= 1);
}

void TestMarkdownTools::testLintBrokenLocalLinks() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString mdPath = tempDir.path() + "/test.md";
  QFile mdFile(mdPath);
  QVERIFY(mdFile.open(QIODevice::WriteOnly));
  mdFile.write("[exists](test.md)\n[broken](nonexistent.md)");
  mdFile.close();

  QFile content(mdPath);
  QVERIFY(content.open(QIODevice::ReadOnly));
  QString text = content.readAll();
  content.close();

  QList<LspDiagnostic> diags = MarkdownTools::lint(text, mdPath);

  bool foundBroken = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MD042") {
      foundBroken = true;
      QVERIFY(d.message.contains("nonexistent.md"));
      break;
    }
  }
  QVERIFY(foundBroken);
}

void TestMarkdownTools::testToHtmlHeadings() {
  QString md = "# Title\n\n## Section";
  QString html = MarkdownTools::toHtml(md);

  QVERIFY(html.contains("<h1"));
  QVERIFY(html.contains("Title"));
  QVERIFY(html.contains("<h2"));
  QVERIFY(html.contains("Section"));
}

void TestMarkdownTools::testToHtmlInlineFormatting() {
  QString md = "Some **bold** and *italic* and `code` text";
  QString html = MarkdownTools::toHtml(md);

  QVERIFY(html.contains("<strong>bold</strong>"));
  QVERIFY(html.contains("<em>italic</em>"));
  QVERIFY(html.contains("<code>code</code>"));
}

void TestMarkdownTools::testToHtmlCodeBlocks() {
  QString md = "```cpp\nint main() {}\n```";
  QString html = MarkdownTools::toHtml(md);

  QVERIFY(html.contains("<pre>"));
  QVERIFY(html.contains("<code"));
  QVERIFY(html.contains("language-cpp"));
  QVERIFY(html.contains("int main()"));
}

void TestMarkdownTools::testToHtmlLists() {
  QString md = "- Item 1\n- Item 2\n- Item 3";
  QString html = MarkdownTools::toHtml(md);

  QVERIFY(html.contains("<ul>"));
  QVERIFY(html.contains("<li>"));
  QVERIFY(html.contains("Item 1"));
}

void TestMarkdownTools::testToHtmlTables() {
  QString md = "| H1 | H2 |\n| --- | --- |\n| A | B |";
  QString html = MarkdownTools::toHtml(md);

  QVERIFY(html.contains("<table>"));
  QVERIFY(html.contains("<th>"));
  QVERIFY(html.contains("<td>"));
  QVERIFY(html.contains("H1"));
  QVERIFY(html.contains("A"));
}

void TestMarkdownTools::testToHtmlCheckboxes() {
  QString md = "- [ ] Unchecked\n- [x] Checked";
  QString html = MarkdownTools::toHtml(md);

  QVERIFY(html.contains("task-list-item"));
  QVERIFY(html.contains("type=\"checkbox\""));
  QVERIFY(html.contains("checked"));
  QVERIFY(html.contains("Unchecked"));
}

void TestMarkdownTools::testToHtmlBlockquotes() {
  QString md = "> This is a quote";
  QString html = MarkdownTools::toHtml(md);

  QVERIFY(html.contains("<blockquote>"));
  QVERIFY(html.contains("This is a quote"));
}

void TestMarkdownTools::testToHtmlWithBasePath() {
  QString md = "![img](pic.png)";
  QString html = MarkdownTools::toHtml(md, "/tmp/docs");

  QVERIFY(html.contains("file:///tmp/docs"));
}

QTEST_MAIN(TestMarkdownTools)
#include "test_markdowntools.moc"
