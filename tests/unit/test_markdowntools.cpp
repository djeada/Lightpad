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

  void testWrapParagraph();
  void testWrapParagraphPreservesStructure();
  void testNormalizeHeadingSpacing();
  void testNormalizeBulletIndentation();
  void testFormatCodeFences();

  void testExtractLinks();
  void testExtractLinksIgnoresImages();
  void testExtractImages();
  void testExtractFootnotes();
  void testExtractFootnotesOrphanRef();
  void testParseFrontmatter();
  void testParseFrontmatterMissing();
  void testGenerateDocumentOutline();

  void testWordCount();
  void testWordCountIgnoresCodeBlocks();
  void testReadingTimeMinutes();

  void testLintBrokenImagePaths();
  void testLintMalformedLists();
  void testLintOverlongLines();
  void testLintOverlongLinesSkipsExceptions();
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
  QCOMPARE(MarkdownTools::generateAnchor("Hello World"),
           QString("hello-world"));
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
  QCOMPARE(MarkdownTools::toggleCheckbox("- [ ] Task"), QString("- [x] Task"));
  QCOMPARE(MarkdownTools::toggleCheckbox("- [x] Done"), QString("- [ ] Done"));
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
  QString md =
      "Line with trailing spaces   \nClean line\nAnother dirty line   ";
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

void TestMarkdownTools::testWrapParagraph() {
  QString md = "This is a very long paragraph that should be wrapped at a "
               "reasonable width to improve readability.";
  QString result = MarkdownTools::wrapParagraph(md, 40);

  QStringList lines = result.split('\n');
  QVERIFY(lines.size() > 1);
  for (const QString &line : lines) {
    QVERIFY(line.length() <= 40 || !line.contains(' '));
  }
}

void TestMarkdownTools::testWrapParagraphPreservesStructure() {
  QString md = "# Heading\n\n- List item\n\n```\ncode block\n```\n\n> "
               "blockquote\n\nShort line";
  QString result = MarkdownTools::wrapParagraph(md, 40);

  QVERIFY(result.contains("# Heading"));
  QVERIFY(result.contains("- List item"));
  QVERIFY(result.contains("```\ncode block\n```"));
  QVERIFY(result.contains("> blockquote"));
}

void TestMarkdownTools::testNormalizeHeadingSpacing() {
  QString md = "# Title\nSome text\n## Section\n\n\n\nMore text";
  QString result = MarkdownTools::normalizeHeadingSpacing(md);

  QVERIFY(result.contains("# Title\n\nSome text"));
  QVERIFY(result.contains("## Section\n\n"));
}

void TestMarkdownTools::testNormalizeBulletIndentation() {
  QString md = "- Item 1\n   * Nested\n      + Deep nested";
  QString result = MarkdownTools::normalizeBulletIndentation(md);

  QVERIFY(result.contains("- Item 1"));
  QVERIFY(result.contains("  - Nested"));
  QVERIFY(result.contains("    - Deep nested"));
}

void TestMarkdownTools::testFormatCodeFences() {
  QString md = "````python\nprint('hello')\n````";
  QString result = MarkdownTools::formatCodeFences(md);

  QVERIFY(result.contains("```python"));
  QVERIFY(result.contains("print('hello')"));
  QVERIFY(result.endsWith("```"));
}

void TestMarkdownTools::testExtractLinks() {
  QString md = "Check [Google](https://google.com) and [local](file.md).\n\n"
               "![image](pic.png)";
  QList<MarkdownLink> links = MarkdownTools::extractLinks(md);

  QCOMPARE(links.size(), 2);
  QCOMPARE(links[0].text, QString("Google"));
  QCOMPARE(links[0].target, QString("https://google.com"));
  QCOMPARE(links[0].lineNumber, 0);
  QVERIFY(!links[0].isImage);
  QCOMPARE(links[1].text, QString("local"));
  QCOMPARE(links[1].target, QString("file.md"));
}

void TestMarkdownTools::testExtractLinksIgnoresImages() {
  QString md = "![alt](image.png)\n[real link](page.md)";
  QList<MarkdownLink> links = MarkdownTools::extractLinks(md);

  QCOMPARE(links.size(), 1);
  QCOMPARE(links[0].text, QString("real link"));
}

void TestMarkdownTools::testExtractImages() {
  QString md = "![Screenshot](./images/screen.png)\n![Logo](logo.svg)\n[not "
               "image](link.md)";
  QList<MarkdownLink> images = MarkdownTools::extractImages(md);

  QCOMPARE(images.size(), 2);
  QCOMPARE(images[0].text, QString("Screenshot"));
  QCOMPARE(images[0].target, QString("./images/screen.png"));
  QVERIFY(images[0].isImage);
  QCOMPARE(images[1].text, QString("Logo"));
  QCOMPARE(images[1].target, QString("logo.svg"));
}

void TestMarkdownTools::testExtractFootnotes() {
  QString md =
      "Some text[^1] and more[^note].\n\n[^1]: First footnote\n[^note]: "
      "A longer footnote";
  QList<MarkdownFootnote> footnotes = MarkdownTools::extractFootnotes(md);

  QCOMPARE(footnotes.size(), 2);

  bool found1 = false, foundNote = false;
  for (const MarkdownFootnote &fn : footnotes) {
    if (fn.label == "1") {
      found1 = true;
      QCOMPARE(fn.text, QString("First footnote"));
      QVERIFY(fn.definitionLine >= 0);
      QVERIFY(!fn.referenceLines.isEmpty());
    }
    if (fn.label == "note") {
      foundNote = true;
      QCOMPARE(fn.text, QString("A longer footnote"));
    }
  }
  QVERIFY(found1);
  QVERIFY(foundNote);
}

void TestMarkdownTools::testExtractFootnotesOrphanRef() {
  QString md = "Text with[^orphan] reference but no definition.";
  QList<MarkdownFootnote> footnotes = MarkdownTools::extractFootnotes(md);

  QCOMPARE(footnotes.size(), 1);
  QCOMPARE(footnotes[0].label, QString("orphan"));
  QCOMPARE(footnotes[0].definitionLine, -1);
}

void TestMarkdownTools::testParseFrontmatter() {
  QString md = "---\ntitle: My Document\ndate: 2024-01-15\ntags: [a, "
               "b]\n---\n\n# Content";
  MarkdownFrontmatter fm = MarkdownTools::parseFrontmatter(md);

  QCOMPARE(fm.startLine, 0);
  QCOMPARE(fm.endLine, 4);
  QVERIFY(fm.fields.contains("title"));
  QCOMPARE(fm.fields["title"], QString("My Document"));
  QVERIFY(fm.fields.contains("date"));
  QCOMPARE(fm.fields["date"], QString("2024-01-15"));
  QVERIFY(!fm.rawContent.isEmpty());
}

void TestMarkdownTools::testParseFrontmatterMissing() {
  QString md = "# Just a heading\n\nSome content";
  MarkdownFrontmatter fm = MarkdownTools::parseFrontmatter(md);

  QCOMPARE(fm.startLine, -1);
  QCOMPARE(fm.endLine, -1);
  QVERIFY(fm.fields.isEmpty());
}

void TestMarkdownTools::testGenerateDocumentOutline() {
  QString md = "# Root\n\n## Child 1\n\n### Grandchild\n\n## Child 2";
  QList<MarkdownOutlineEntry> outline =
      MarkdownTools::generateDocumentOutline(md);

  QCOMPARE(outline.size(), 1);
  QCOMPARE(outline[0].text, QString("Root"));
  QCOMPARE(outline[0].children.size(), 2);
  QCOMPARE(outline[0].children[0].text, QString("Child 1"));
  QCOMPARE(outline[0].children[0].children.size(), 1);
  QCOMPARE(outline[0].children[0].children[0].text, QString("Grandchild"));
  QCOMPARE(outline[0].children[1].text, QString("Child 2"));
}

void TestMarkdownTools::testWordCount() {
  QString md = "# Title\n\nThis is a paragraph with seven words here.";
  int count = MarkdownTools::wordCount(md);

  QCOMPARE(count, 9);
}

void TestMarkdownTools::testWordCountIgnoresCodeBlocks() {
  QString md = "One two three\n\n```\nnot counted words here\n```\n\nFour five";
  int count = MarkdownTools::wordCount(md);

  QCOMPARE(count, 5);
}

void TestMarkdownTools::testReadingTimeMinutes() {

  QString md;
  for (int i = 0; i < 200; ++i) {
    md += "word ";
  }
  int time = MarkdownTools::readingTimeMinutes(md, 200);
  QCOMPARE(time, 1);

  int shortTime = MarkdownTools::readingTimeMinutes("Hello world", 200);
  QCOMPARE(shortTime, 1);
}

void TestMarkdownTools::testLintBrokenImagePaths() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  QString imgPath = tempDir.path() + "/real.png";
  QFile img(imgPath);
  QVERIFY(img.open(QIODevice::WriteOnly));
  img.write("PNG");
  img.close();

  QString mdPath = tempDir.path() + "/test.md";
  QString text = "![exists](real.png)\n![broken](nonexistent.png)";

  QList<LspDiagnostic> diags = MarkdownTools::lint(text, mdPath);

  bool foundBrokenImage = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MD041") {
      foundBrokenImage = true;
      QVERIFY(d.message.contains("nonexistent.png"));
      QCOMPARE(d.severity, LspDiagnosticSeverity::Warning);
      break;
    }
  }
  QVERIFY(foundBrokenImage);
}

void TestMarkdownTools::testLintMalformedLists() {
  QString md = "Some text\n- List without blank line before";
  QList<LspDiagnostic> diags = MarkdownTools::lint(md);

  bool foundMalformed = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MD032") {
      foundMalformed = true;
      QVERIFY(d.message.contains("blank lines"));
      break;
    }
  }
  QVERIFY(foundMalformed);
}

void TestMarkdownTools::testLintOverlongLines() {
  QString longLine(150, 'x');
  QString md = "Short line\n\n" + longLine;
  QList<LspDiagnostic> diags = MarkdownTools::lint(md);

  bool foundOverlong = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MD013") {
      foundOverlong = true;
      QVERIFY(d.message.contains("exceeds"));
      QCOMPARE(d.severity, LspDiagnosticSeverity::Hint);
      break;
    }
  }
  QVERIFY(foundOverlong);
}

void TestMarkdownTools::testLintOverlongLinesSkipsExceptions() {

  QString longHeading = "# " + QString(150, 'H');
  QString longTable = "| " + QString(150, 'T') + " |";
  QString longLink = "[text](" + QString(150, 'u') + ")";
  QString md = longHeading + "\n\n" + longTable + "\n\n" + longLink;
  QList<LspDiagnostic> diags = MarkdownTools::lint(md);

  for (const LspDiagnostic &d : diags) {
    QVERIFY(d.code != "MD013");
  }
}

QTEST_MAIN(TestMarkdownTools)
#include "test_markdowntools.moc"
