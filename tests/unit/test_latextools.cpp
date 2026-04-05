#include "latex/latextools.h"
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>

class TestLatexTools : public QObject {
  Q_OBJECT

private slots:
  void testExtractSections();
  void testExtractSectionsStarred();
  void testExtractSectionsWithLabels();
  void testExtractSectionsIgnoresComments();
  void testExtractLabels();
  void testExtractLabelsIgnoresComments();
  void testExtractReferences();
  void testExtractReferencesSplitsCommaKeys();
  void testExtractCitations();
  void testExtractCitationsMultipleVariants();
  void testExtractIncludes();
  void testExtractIncludesBibResource();
  void testDetectDocumentClass();
  void testDetectDocumentClassWithOptions();
  void testDetectDocumentClassMissing();
  void testDetectPackages();
  void testDetectPackagesMultiple();
  void testDetectMainFile();
  void testParseLatexLogErrors();
  void testParseLatexLogWarnings();
  void testParseLatexLogOverfull();
  void testParseLatexLogPackageWarnings();
  void testLintUnclosedEnvironments();
  void testLintDuplicateLabels();
  void testLintUndefinedReferences();
  void testLintMissingDocumentClass();
  void testLintUnclosedMath();
  void testLintCommonTypos();
  void testLintCleanDocument();
  void testLintSkipsStyleFiles();
  void testIsLatexFile();
  void testGenerateOutline();
  void testGenerateOutlineMaxDepth();
  void testFindMatchingEnvironment();
  void testFindMatchingEnvironmentNested();
  void testResolveIncludePath();
  void testResolveIncludePathAddsExtension();
};

void TestLatexTools::testExtractSections() {
  QString tex = "\\section{Introduction}\n"
                "Some text here.\n"
                "\\subsection{Background}\n"
                "More text.\n"
                "\\section{Methods}\n";

  QList<LatexSection> sections = LatexTools::extractSections(tex);

  QCOMPARE(sections.size(), 3);
  QCOMPARE(sections[0].text, QString("Introduction"));
  QCOMPARE(sections[0].level, 2);
  QCOMPARE(sections[0].lineNumber, 0);
  QCOMPARE(sections[1].text, QString("Background"));
  QCOMPARE(sections[1].level, 3);
  QCOMPARE(sections[1].lineNumber, 2);
  QCOMPARE(sections[2].text, QString("Methods"));
  QCOMPARE(sections[2].level, 2);
  QCOMPARE(sections[2].lineNumber, 4);
}

void TestLatexTools::testExtractSectionsStarred() {
  QString tex = "\\section*{Unnumbered Section}\n";
  QList<LatexSection> sections = LatexTools::extractSections(tex);

  QCOMPARE(sections.size(), 1);
  QCOMPARE(sections[0].text, QString("Unnumbered Section"));
  QCOMPARE(sections[0].level, 2);
}

void TestLatexTools::testExtractSectionsWithLabels() {
  QString tex = "\\section{Introduction}\n"
                "\\label{sec:intro}\n"
                "Some text.\n";

  QList<LatexSection> sections = LatexTools::extractSections(tex);

  QCOMPARE(sections.size(), 1);
  QCOMPARE(sections[0].label, QString("sec:intro"));
}

void TestLatexTools::testExtractSectionsIgnoresComments() {
  QString tex = "\\section{Real Section}\n"
                "% \\section{Commented Section}\n";

  QList<LatexSection> sections = LatexTools::extractSections(tex);

  QCOMPARE(sections.size(), 1);
  QCOMPARE(sections[0].text, QString("Real Section"));
}

void TestLatexTools::testExtractLabels() {
  QString tex = "\\label{fig:plot}\n"
                "\\label{eq:energy}\n"
                "\\label{sec:intro}\n";

  QList<LatexLabel> labels = LatexTools::extractLabels(tex);

  QCOMPARE(labels.size(), 3);
  QCOMPARE(labels[0].name, QString("fig:plot"));
  QCOMPARE(labels[0].type, QString("label"));
  QCOMPARE(labels[0].lineNumber, 0);
  QCOMPARE(labels[1].name, QString("eq:energy"));
  QCOMPARE(labels[2].name, QString("sec:intro"));
}

void TestLatexTools::testExtractLabelsIgnoresComments() {
  QString tex = "\\label{real}\n"
                "% \\label{commented}\n";

  QList<LatexLabel> labels = LatexTools::extractLabels(tex);

  QCOMPARE(labels.size(), 1);
  QCOMPARE(labels[0].name, QString("real"));
}

void TestLatexTools::testExtractReferences() {
  QString tex = "See \\ref{fig:plot} and \\eqref{eq:energy}.\n"
                "Also \\pageref{sec:intro}.\n";

  QList<LatexLabel> refs = LatexTools::extractReferences(tex);

  QCOMPARE(refs.size(), 3);
  QCOMPARE(refs[0].name, QString("fig:plot"));
  QCOMPARE(refs[0].type, QString("ref"));
  QCOMPARE(refs[1].name, QString("eq:energy"));
  QCOMPARE(refs[1].type, QString("eqref"));
  QCOMPARE(refs[2].name, QString("sec:intro"));
  QCOMPARE(refs[2].type, QString("pageref"));
}

void TestLatexTools::testExtractReferencesSplitsCommaKeys() {
  QString tex = "\\cite{knuth1984,lamport1994}\n";

  QList<LatexLabel> refs = LatexTools::extractReferences(tex);

  QCOMPARE(refs.size(), 2);
  QCOMPARE(refs[0].name, QString("knuth1984"));
  QCOMPARE(refs[1].name, QString("lamport1994"));
}

void TestLatexTools::testExtractCitations() {
  QString tex = "\\cite{knuth1984} and \\citep{lamport1994}\n";

  QList<LatexLabel> cites = LatexTools::extractCitations(tex);

  QCOMPARE(cites.size(), 2);
  QCOMPARE(cites[0].name, QString("knuth1984"));
  QCOMPARE(cites[0].type, QString("cite"));
  QCOMPARE(cites[1].name, QString("lamport1994"));
}

void TestLatexTools::testExtractCitationsMultipleVariants() {
  QString tex = "\\citeauthor{author2020}\n"
                "\\textcite{text2021}\n"
                "\\autocite{auto2022}\n";

  QList<LatexLabel> cites = LatexTools::extractCitations(tex);

  QCOMPARE(cites.size(), 3);
  QCOMPARE(cites[0].name, QString("author2020"));
  QCOMPARE(cites[1].name, QString("text2021"));
  QCOMPARE(cites[2].name, QString("auto2022"));
}

void TestLatexTools::testExtractIncludes() {
  QString tex = "\\input{chapters/intro}\n"
                "\\include{chapters/methods}\n"
                "\\includegraphics[width=0.5\\textwidth]{fig/plot.png}\n";

  QList<LatexInclude> includes = LatexTools::extractIncludes(tex);

  QCOMPARE(includes.size(), 3);
  QCOMPARE(includes[0].path, QString("chapters/intro"));
  QCOMPARE(includes[0].type, QString("input"));
  QCOMPARE(includes[1].path, QString("chapters/methods"));
  QCOMPARE(includes[1].type, QString("include"));
  QCOMPARE(includes[2].path, QString("fig/plot.png"));
  QCOMPARE(includes[2].type, QString("includegraphics"));
}

void TestLatexTools::testExtractIncludesBibResource() {
  QString tex = "\\addbibresource{references.bib}\n";

  QList<LatexInclude> includes = LatexTools::extractIncludes(tex);

  QCOMPARE(includes.size(), 1);
  QCOMPARE(includes[0].path, QString("references.bib"));
  QCOMPARE(includes[0].type, QString("addbibresource"));
}

void TestLatexTools::testDetectDocumentClass() {
  QString tex = "% Comment\n"
                "\\documentclass{article}\n"
                "\\begin{document}\n";

  QCOMPARE(LatexTools::detectDocumentClass(tex), QString("article"));
}

void TestLatexTools::testDetectDocumentClassWithOptions() {
  QString tex = "\\documentclass[12pt,a4paper]{report}\n";
  QCOMPARE(LatexTools::detectDocumentClass(tex), QString("report"));
}

void TestLatexTools::testDetectDocumentClassMissing() {
  QString tex = "\\begin{document}\nHello\n\\end{document}\n";
  QVERIFY(LatexTools::detectDocumentClass(tex).isEmpty());
}

void TestLatexTools::testDetectPackages() {
  QString tex = "\\usepackage{amsmath}\n"
                "\\usepackage[utf8]{inputenc}\n"
                "\\usepackage{graphicx,hyperref}\n";

  QStringList packages = LatexTools::detectPackages(tex);

  QVERIFY(packages.contains("amsmath"));
  QVERIFY(packages.contains("inputenc"));
  QVERIFY(packages.contains("graphicx"));
  QVERIFY(packages.contains("hyperref"));
}

void TestLatexTools::testDetectPackagesMultiple() {
  QString tex = "\\usepackage{amssymb, amsfonts, amsthm}\n";
  QStringList packages = LatexTools::detectPackages(tex);

  QCOMPARE(packages.size(), 3);
  QVERIFY(packages.contains("amssymb"));
  QVERIFY(packages.contains("amsfonts"));
  QVERIFY(packages.contains("amsthm"));
}

void TestLatexTools::testDetectMainFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QFile main(dir.filePath("main.tex"));
  QVERIFY(main.open(QIODevice::WriteOnly));
  main.write("\\documentclass{article}\n\\begin{document}\n\\end{document}\n");
  main.close();

  QFile helper(dir.filePath("chapter1.tex"));
  QVERIFY(helper.open(QIODevice::WriteOnly));
  helper.write("\\section{Chapter}\nContent here.\n");
  helper.close();

  QString result = LatexTools::detectMainFile(dir.path());
  QVERIFY(!result.isEmpty());
  QVERIFY(result.endsWith("main.tex"));
}

void TestLatexTools::testParseLatexLogErrors() {
  QString log = "(/usr/share/texlive/texmf-dist/tex/latex/base/article.cls)\n"
                "! LaTeX Error: File `missing.sty' not found.\n"
                "\n"
                "l.3 \\usepackage{missing}\n";

  QList<LatexLogEntry> entries = LatexTools::parseLatexLog(log);

  bool foundError = false;
  for (const LatexLogEntry &e : entries) {
    if (e.severity == 1 && e.message.contains("missing.sty")) {
      foundError = true;
      break;
    }
  }
  QVERIFY2(foundError, "Should parse LaTeX error from log");
}

void TestLatexTools::testParseLatexLogWarnings() {
  QString log = "LaTeX Warning: Reference `sec:missing' on page 1 undefined on "
                "input line 15.\n";

  QList<LatexLogEntry> entries = LatexTools::parseLatexLog(log);

  QVERIFY(!entries.isEmpty());
  QCOMPARE(entries[0].severity, 2);
  QVERIFY(entries[0].message.contains("Reference"));
  QCOMPARE(entries[0].lineNumber, 15);
}

void TestLatexTools::testParseLatexLogOverfull() {
  QString log =
      "Overfull \\hbox (42.0pt too wide) in paragraph at lines 10--20\n";

  QList<LatexLogEntry> entries = LatexTools::parseLatexLog(log);

  QVERIFY(!entries.isEmpty());
  QCOMPARE(entries[0].severity, 3);
  QVERIFY(entries[0].message.contains("Overfull"));
}

void TestLatexTools::testParseLatexLogPackageWarnings() {
  QString log = "Package hyperref Warning: Token not allowed in a PDF string\n";

  QList<LatexLogEntry> entries = LatexTools::parseLatexLog(log);

  QVERIFY(!entries.isEmpty());
  QCOMPARE(entries[0].severity, 2);
  QVERIFY(entries[0].message.contains("hyperref"));
}

void TestLatexTools::testLintUnclosedEnvironments() {
  QString tex = "\\begin{document}\n"
                "\\begin{itemize}\n"
                "\\item First\n"
                "\\end{document}\n";

  QList<LspDiagnostic> diags = LatexTools::lint(tex);

  bool foundUnclosed = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "ENV001" || d.code == "ENV002") {
      foundUnclosed = true;
      break;
    }
  }
  QVERIFY2(foundUnclosed, "Should detect unclosed environments");
}

void TestLatexTools::testLintDuplicateLabels() {
  QString tex = "\\label{fig:one}\n"
                "\\label{fig:one}\n";

  QList<LspDiagnostic> diags = LatexTools::lint(tex);

  bool foundDuplicate = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "LBL001") {
      foundDuplicate = true;
      break;
    }
  }
  QVERIFY2(foundDuplicate, "Should detect duplicate labels");
}

void TestLatexTools::testLintUndefinedReferences() {
  QString tex = "\\label{fig:exists}\n"
                "See \\ref{fig:exists} and \\ref{fig:missing}.\n";

  QList<LspDiagnostic> diags = LatexTools::lint(tex);

  bool foundUndefined = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "REF001" && d.message.contains("fig:missing")) {
      foundUndefined = true;
      break;
    }
  }
  QVERIFY2(foundUndefined, "Should detect undefined references");

  bool falsePositive = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "REF001" && d.message.contains("fig:exists")) {
      falsePositive = true;
      break;
    }
  }
  QVERIFY2(!falsePositive, "Should not report existing labels as undefined");
}

void TestLatexTools::testLintMissingDocumentClass() {
  QString tex = "\\begin{document}\nHello\n\\end{document}\n";

  QList<LspDiagnostic> diags = LatexTools::lint(tex, "/tmp/test.tex");

  bool foundMissing = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "DOC001") {
      foundMissing = true;
      break;
    }
  }
  QVERIFY2(foundMissing, "Should warn about missing \\documentclass");
}

void TestLatexTools::testLintUnclosedMath() {
  QString tex = "The value is $x + y and not complete.\n";

  QList<LspDiagnostic> diags = LatexTools::lint(tex);

  bool foundMath = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "MATH001") {
      foundMath = true;
      break;
    }
  }
  QVERIFY2(foundMath, "Should detect unclosed inline math");
}

void TestLatexTools::testLintCommonTypos() {
  QString tex = "\\being{document}\n";

  QList<LspDiagnostic> diags = LatexTools::lint(tex);

  bool foundTypo = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "TYPO001") {
      foundTypo = true;
      break;
    }
  }
  QVERIFY2(foundTypo, "Should detect \\being typo");
}

void TestLatexTools::testLintCleanDocument() {
  QString tex = "\\documentclass{article}\n"
                "\\begin{document}\n"
                "\\section{Hello}\n"
                "\\label{sec:hello}\n"
                "See Section~\\ref{sec:hello}.\n"
                "Inline math $x = 1$.\n"
                "\\end{document}\n";

  QList<LspDiagnostic> diags = LatexTools::lint(tex, "/tmp/test.tex");
  QVERIFY2(diags.isEmpty(), "Clean document should produce no diagnostics");
}

void TestLatexTools::testLintSkipsStyleFiles() {
  QString tex = "\\ProvidesPackage{mypackage}\n";

  QList<LspDiagnostic> diags = LatexTools::lint(tex, "/tmp/mypackage.sty");

  bool foundDocClass = false;
  for (const LspDiagnostic &d : diags) {
    if (d.code == "DOC001") {
      foundDocClass = true;
      break;
    }
  }
  QVERIFY2(!foundDocClass,
           "Should not warn about missing \\documentclass in .sty files");
}

void TestLatexTools::testIsLatexFile() {
  QVERIFY(LatexTools::isLatexFile("tex"));
  QVERIFY(LatexTools::isLatexFile("TEX"));
  QVERIFY(LatexTools::isLatexFile("sty"));
  QVERIFY(LatexTools::isLatexFile("cls"));
  QVERIFY(LatexTools::isLatexFile("bib"));
  QVERIFY(LatexTools::isLatexFile("dtx"));
  QVERIFY(LatexTools::isLatexFile("ins"));
  QVERIFY(LatexTools::isLatexFile("ltx"));

  QVERIFY(!LatexTools::isLatexFile("txt"));
  QVERIFY(!LatexTools::isLatexFile("cpp"));
  QVERIFY(!LatexTools::isLatexFile("md"));
}

void TestLatexTools::testGenerateOutline() {
  QString tex = "\\chapter{Introduction}\n"
                "\\section{Background}\n"
                "\\subsection{Details}\n"
                "\\section{Motivation}\n";

  QString outline = LatexTools::generateOutline(tex);

  QVERIFY(outline.contains("Introduction"));
  QVERIFY(outline.contains("Background"));
  QVERIFY(outline.contains("Details"));
  QVERIFY(outline.contains("Motivation"));
}

void TestLatexTools::testGenerateOutlineMaxDepth() {
  QString tex = "\\section{Top}\n"
                "\\subsection{Mid}\n"
                "\\subsubsection{Deep}\n";

  QString outline = LatexTools::generateOutline(tex, 3);

  QVERIFY(outline.contains("Top"));
  QVERIFY(outline.contains("Mid"));
  QVERIFY(!outline.contains("Deep"));
}

void TestLatexTools::testFindMatchingEnvironment() {
  QString tex = "\\begin{document}\n"
                "Hello World\n"
                "\\end{document}\n";

  QPair<int, int> result = LatexTools::findMatchingEnvironment(tex, 0);

  QVERIFY(result.first >= 0);
  QVERIFY(result.second > result.first);
}

void TestLatexTools::testFindMatchingEnvironmentNested() {
  QString tex = "\\begin{document}\n"
                "\\begin{itemize}\n"
                "\\item Hello\n"
                "\\end{itemize}\n"
                "\\end{document}\n";

  QPair<int, int> result = LatexTools::findMatchingEnvironment(tex, 0);

  QVERIFY(result.first == 0);
  QVERIFY(result.second > 0);
}

void TestLatexTools::testResolveIncludePath() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QFile f(dir.filePath("chapter1.tex"));
  QVERIFY(f.open(QIODevice::WriteOnly));
  f.write("content");
  f.close();

  QString resolved = LatexTools::resolveIncludePath("chapter1", dir.path());
  QVERIFY(resolved.endsWith("chapter1.tex"));

  QString resolved2 =
      LatexTools::resolveIncludePath("chapter1.tex", dir.path());
  QVERIFY(resolved2.endsWith("chapter1.tex"));
}

void TestLatexTools::testResolveIncludePathAddsExtension() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QString resolved = LatexTools::resolveIncludePath("nonexistent", dir.path());
  QVERIFY(resolved.endsWith("nonexistent.tex"));
}

QTEST_MAIN(TestLatexTools)
#include "test_latextools.moc"
