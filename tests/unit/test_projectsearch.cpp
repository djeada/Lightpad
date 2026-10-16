#include "filetree/projectsearch.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

class TestProjectSearch : public QObject {
  Q_OBJECT
private:
  QTemporaryDir m_root;
  QString write(const QString &relative, const QByteArray &content = "text") {
    const QString path = m_root.filePath(relative);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(content) != content.size()) {
      qFatal("Could not create search fixture");
    }
    return path;
  }
  ProjectSearch::Options
  options(const QString &query,
          ProjectSearch::Mode mode = ProjectSearch::Mode::FileNames) {
    ProjectSearch::Options result;
    result.rootPath = m_root.path();
    result.query = query;
    result.mode = mode;
    return result;
  }

private slots:
  void deepNamesAndPaths() {
    write("deep/a/b/c/NeedleWidget.cpp");
    auto result = ProjectSearch::search(options("NeedleWidget"));
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first().relativePath,
             "deep/a/b/c/NeedleWidget.cpp");
    QVERIFY(ProjectSearch::fileNameScore("nwg", "NeedleWidget.cpp") >= 0);
    QVERIFY(ProjectSearch::fileNameScore("deep widget",
                                         "deep/a/NeedleWidget.cpp") >= 0);
    QVERIFY(ProjectSearch::fileNameScore("*.cpp", "deep/NeedleWidget.cpp") >=
            0);
    QCOMPARE(ProjectSearch::fileNameScore("*.h", "deep/NeedleWidget.cpp"), -1);
  }

  void filenameAndContentAreDifferentSearches() {
    write("mode-tests/needle.txt", "unrelated");
    write("mode-tests/other.txt", "needle");
    auto config = options("needle");
    config.scopePath = m_root.filePath("mode-tests");
    auto names = ProjectSearch::search(config);
    QCOMPARE(names.matches.size(), 1);
    QVERIFY(names.matches.first().filePath.endsWith("needle.txt"));
    config.mode = ProjectSearch::Mode::Contents;
    auto contents = ProjectSearch::search(config);
    QCOMPARE(contents.matches.size(), 1);
    QVERIFY(contents.matches.first().filePath.endsWith("other.txt"));
  }

  void ranksBestNameEvenWhenFoundAfterTheLimit() {
    for (int i = 0; i < 15; ++i) {
      write(QString("ranking/a-%1-target-extra.txt").arg(i));
    }
    write("ranking/z/target");
    auto config = options("target");
    config.scopePath = m_root.filePath("ranking");
    config.maxResults = 1;
    auto result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first().relativePath, "ranking/z/target");
    QVERIFY(result.truncated);
  }

  void contentSearchIncludesDotfilesAndExtensionlessFiles() {
    write("extensionless/Dockerfile", "FROM ubuntu\nRUN echo needle\n");
    write("extensionless/.env", "TOKEN=needle\r\n");
    auto config = options("needle", ProjectSearch::Mode::Contents);
    config.scopePath = m_root.filePath("extensionless");
    auto result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 2);
    const auto found = std::find_if(
        result.matches.cbegin(), result.matches.cend(), [](const auto &match) {
          return match.relativePath.endsWith("Dockerfile");
        });
    QVERIFY(found != result.matches.cend());
    QCOMPARE(found->line, 2);
    QCOMPARE(found->column, 10);
    QCOMPARE(found->length, 6);
  }

  void regexCaseAndUnicodeWordBoundaries() {
    write(
        "regex/text",
        QString::fromUtf8("cat catfish Cat écat caté\ncat42 cat7\n").toUtf8());
    auto config = options("cat", ProjectSearch::Mode::Contents);
    config.scopePath = m_root.filePath("regex");
    config.wholeWord = true;
    auto result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 2);
    config.caseSensitive = true;
    QCOMPARE(ProjectSearch::search(config).matches.size(), 1);
    config.wholeWord = false;
    config.regularExpression = true;
    config.query = "cat\\d+";
    QCOMPARE(ProjectSearch::search(config).matches.size(), 2);
    config.query = "[";
    result = ProjectSearch::search(config);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.matches.isEmpty());
  }

  void includesAndExcludesPruneFolders() {
    write("globs/src/main.cpp", "glob-needle");
    write("globs/src/lib/util.cpp", "glob-needle");
    write("globs/src/lib/util.h", "glob-needle");
    write("globs/build/generated.cpp", "glob-needle");
    auto config = options("glob-needle", ProjectSearch::Mode::Contents);
    config.scopePath = m_root.filePath("globs");
    config.includes = "globs/src/**/*.cpp";
    QCOMPARE(ProjectSearch::search(config).matches.size(), 2);
    config.excludes += ", globs/src/lib/**";
    auto result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first().relativePath, "globs/src/main.cpp");
    config.includes.clear();
    config.excludes.clear();
    QCOMPARE(ProjectSearch::search(config).matches.size(), 4);
  }

  void hiddenFilesAreOptional() {
    write("hidden/.config/hidden-token.txt");
    auto config = options("hidden-token");
    QCOMPARE(ProjectSearch::search(config).matches.size(), 1);
    config.includeHidden = false;
    QVERIFY(ProjectSearch::search(config).matches.isEmpty());
  }

  void scopeCannotEscapeProject() {
    QTemporaryDir outside;
    auto config = options("text");
    config.scopePath = outside.path();
    QVERIFY(!ProjectSearch::search(config).error.isEmpty());
#ifndef Q_OS_WIN
    QVERIFY(QFile::link(outside.path(), m_root.filePath("outside-link")));
    config.scopePath = m_root.filePath("outside-link");
    QVERIFY(!ProjectSearch::search(config).error.isEmpty());
#endif
    config.rootPath = m_root.filePath("missing-project");
    QVERIFY(!ProjectSearch::search(config).error.isEmpty());
  }

  void doesNotTraverseSymlinks() {
#ifdef Q_OS_WIN
    QSKIP("Creating symlinks may require elevation");
#endif
    QTemporaryDir outside;
    QFile external(outside.filePath("external-token.txt"));
    QVERIFY(external.open(QIODevice::WriteOnly));
    external.write("external-content-token");
    external.close();
    QDir().mkpath(m_root.filePath("links"));
    QVERIFY(QFile::link(outside.path(), m_root.filePath("links/outside")));
    QVERIFY(QFile::link(external.fileName(),
                        m_root.filePath("links/external-token.txt")));
    QVERIFY(QFile::link(m_root.path(), m_root.filePath("links/loop")));
    auto config = options("external-token");
    config.scopePath = m_root.filePath("links");
    QCOMPARE(ProjectSearch::search(config).matches.size(), 1);
    config.mode = ProjectSearch::Mode::Contents;
    config.query = "external-content-token";
    QVERIFY(ProjectSearch::search(config).matches.isEmpty());
  }

  void binaryAndLargeFilesAreSkipped() {
    write("binary/binary.dat", QByteArray("target\0more", 11));
    write("binary/large.txt", QByteArray(256, 'x') + "target");
    write("binary/small.txt", "target");
    auto config = options("target", ProjectSearch::Mode::Contents);
    config.scopePath = m_root.filePath("binary");
    config.maxFileBytes = 128;
    auto result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.skippedFiles, 2);
  }

  void resultLimitAndLongLinePreview() {
    write("limits/text", QByteArray(500, 'x') + " needle needle needle\n");
    auto config = options("needle", ProjectSearch::Mode::Contents);
    config.scopePath = m_root.filePath("limits");
    config.maxResults = 2;
    auto result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 2);
    QVERIFY(result.truncated);
    const auto &match = result.matches.first();
    QCOMPARE(match.column, 502);
    QVERIFY(match.preview.startsWith(QChar(0x2026)));
    QCOMPARE(match.preview.mid(match.previewStart, match.previewLength),
             "needle");
  }

  void cancellationStopsTraversal() {
    auto config = options("needle");
    auto result = ProjectSearch::search(config, [] { return true; });
    QVERIFY(result.cancelled);
    QCOMPARE(result.scannedFiles, 0);
    int calls = 0;
    result = ProjectSearch::search(config, [&calls] { return ++calls > 4; });
    QVERIFY(result.cancelled);
  }

  void literalWhitespaceAndBomColumns() {
    write("encoding/text", QByteArray::fromHex("efbbbf") + "needle one\r\n");
    auto config = options("needle", ProjectSearch::Mode::Contents);
    config.scopePath = m_root.filePath("encoding");
    auto result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first().column, 1);
    config.query = " ";
    result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first().column, 7);
  }

  void quickOpenCanRequestOnlyFiles() {
    write("only-files/folder/target.txt");
    auto config = options("*");
    config.scopePath = m_root.filePath("only-files");
    config.includeDirectories = false;
    auto result = ProjectSearch::search(config);
    QCOMPARE(result.matches.size(), 1);
    QVERIFY(!result.matches.first().isDirectory);
  }
};

QTEST_GUILESS_MAIN(TestProjectSearch)
#include "test_projectsearch.moc"
