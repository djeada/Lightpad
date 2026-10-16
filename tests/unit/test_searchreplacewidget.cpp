#include "ui/panels/findreplacepanel.h"
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSyntaxHighlighter>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>
#include <QtTest>

class TestSearchReplaceWidget : public QObject {
  Q_OBJECT
  std::unique_ptr<QPlainTextEdit> editor;
  std::unique_ptr<FindReplacePanel> panel;
  QLineEdit *query() { return panel->findChild<QLineEdit *>("searchFind"); }
  QLineEdit *replacement() {
    return panel->findChild<QLineEdit *>("fieldReplace");
  }
  QLabel *status() { return panel->findChild<QLabel *>("searchStatus"); }
  QTreeWidget *tree() {
    return panel->findChild<QTreeWidget *>("searchResults");
  }
  QPushButton *apply() {
    return panel->findChild<QPushButton *>("applyReplacements");
  }
  void search(const QString &text) {
    QSignalSpy done(panel.get(), &FindReplacePanel::searchCompleted);
    panel->setSearchText(text);
    panel->refreshSearchResults();
    QVERIFY2(done.wait(3000), "Search did not finish");
  }
  void preview() {
    QSignalSpy done(panel.get(), &FindReplacePanel::searchCompleted);
    panel->previewReplacements();
    QVERIFY(done.wait(3000));
  }
  void applyBatch(bool success = true) {
    QSignalSpy done(panel.get(), &FindReplacePanel::replacementFinished);
    panel->applyReplacements();
    QVERIFY(done.wait(3000));
    QCOMPARE(done.first().first().toBool(), success);
  }
  static void write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
      qFatal("fixture");
    file.write(bytes);
  }
  static QByteArray read(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      qFatal("fixture");
    return file.readAll();
  }
private slots:
  void init() {
    editor = std::make_unique<QPlainTextEdit>();
    panel = std::make_unique<FindReplacePanel>(false);
    panel->setEditor(editor.get());
    panel->resize(850, 420);
    panel->show();
  }
  void cleanup() {
    panel.reset();
    editor.reset();
    QCoreApplication::processEvents();
  }
  void nextPreviousAndWrap() {
    editor->setPlainText("one two one");
    search("one");
    panel->findNext();
    QCOMPARE(editor->textCursor().selectionStart(), 0);
    panel->findNext();
    QCOMPARE(editor->textCursor().selectionStart(), 8);
    panel->findNext();
    QCOMPARE(editor->textCursor().selectionStart(), 0);
    QVERIFY(status()->text().contains("Wrapped"));
    panel->findPrevious();
    QCOMPARE(editor->textCursor().selectionStart(), 8);
    panel->findChild<QCheckBox *>("wrapSearch")->setChecked(false);
    panel->findNext();
    QCOMPARE(editor->textCursor().selectionStart(), 8);
    QVERIFY(status()->text().contains("Wrap around is off"));
  }
  void returnAndShiftReturnNavigate() {
    editor->setPlainText("one two one");
    search("one");
    query()->setFocus();
    QTest::keyClick(query(), Qt::Key_Return);
    QCOMPARE(editor->textCursor().selectionStart(), 0);
    QTest::keyClick(query(), Qt::Key_Return);
    QCOMPARE(editor->textCursor().selectionStart(), 8);
    QTest::keyClick(query(), Qt::Key_Return, Qt::ShiftModifier);
    QCOMPARE(editor->textCursor().selectionStart(), 0);
    QTest::keyClick(query(), Qt::Key_F3);
    QCOMPARE(editor->textCursor().selectionStart(), 8);
  }
  void debounceDiscardsStaleResults() {
    editor->setPlainText(QString(500000, 'a') + "needle");
    panel->setSearchText("a");
    panel->refreshSearchResults();
    panel->setSearchText("needle");
    panel->refreshSearchResults();
    QTRY_VERIFY(status()->text().startsWith("1 match"));
    panel->findNext();
    QCOMPARE(editor->textCursor().selectedText(), "needle");
    QCOMPARE(editor->textCursor().selectionStart(), 500000);
  }
  void invalidRegexCannotApply() {
    editor->setPlainText("hello");
    panel->findChild<QToolButton *>("btnRegex")->setChecked(true);
    search("[");
    QVERIFY(status()->text().contains("Invalid regular expression"));
    preview();
    QVERIFY(!apply()->isVisible());
  }
  void previewDeselectionAndOneUndoBlock() {
    editor->setPlainText("cat cat cat");
    replacement()->setText("dog");
    search("cat");
    preview();
    QVERIFY(apply()->isEnabled());
    QCOMPARE(tree()->topLevelItemCount(), 1);
    tree()->topLevelItem(0)->child(1)->setCheckState(0, Qt::Unchecked);
    QVERIFY(apply()->text().contains("2"));
    applyBatch();
    QCOMPARE(editor->toPlainText(), "dog cat dog");
    QVERIFY(editor->document()->isModified());
    editor->undo();
    QCOMPARE(editor->toPlainText(), "cat cat cat");
  }
  void editsInvalidatePreview() {
    editor->setPlainText("cat cat");
    replacement()->setText("dog");
    search("cat");
    preview();
    QVERIFY(apply()->isVisible());
    editor->insertPlainText("changed ");
    QVERIFY(!apply()->isVisible());
    QTRY_VERIFY(status()->text().contains("2 matches"));
    preview();
    replacement()->setText("bird");
    QVERIFY(!apply()->isVisible());
  }
  void capturedSelectionSurvivesNavigation() {
    editor->setPlainText("cat cat cat");
    auto cursor = editor->textCursor();
    cursor.setPosition(4);
    cursor.setPosition(7, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    panel->setScope(SearchReplace::Scope::Selection);
    replacement()->setText("dog");
    search("cat");
    panel->findNext();
    QCOMPARE(editor->textCursor().selectionStart(), 4);
    preview();
    applyBatch();
    QCOMPARE(editor->toPlainText(), "cat dog cat");
  }
  void noSelectionHasUsefulStatus() {
    editor->setPlainText("cat");
    panel->setScope(SearchReplace::Scope::Selection);
    panel->setSearchText("cat");
    panel->refreshSearchResults();
    QVERIFY(status()->text().contains("Select text"));
    QVERIFY(!apply()->isVisible());
  }
  void openFilesIncludesUntitledAndMirrors() {
    QPlainTextEdit second, mirror;
    editor->setPlainText("cat");
    second.setPlainText("cat cat");
    mirror.setPlainText("cat");
    panel->setDocumentProvider([&]() {
      return QVector<FindReplacePanel::OpenDocument>{
          {"buffer:first", {}, "First", editor.get()},
          {"buffer:second", {}, "Second", &second},
          {"buffer:first", {}, "Mirror", &mirror}};
    });
    panel->setScope(SearchReplace::Scope::OpenFiles);
    replacement()->setText("dog");
    search("cat");

    QVERIFY(status()->text().startsWith("3 matches"));
    preview();
    applyBatch();
    QCOMPARE(editor->toPlainText(), "dog");
    QCOMPARE(second.toPlainText(), "dog dog");
    QCOMPARE(mirror.toPlainText(), "dog");
    QSignalSpy undone(panel.get(), &FindReplacePanel::replacementFinished);
    panel->undoReplacementBatch();
    QVERIFY(undone.wait(3000));
    QCOMPARE(editor->toPlainText(), "cat");
    QCOMPARE(second.toPlainText(), "cat cat");
    QCOMPARE(mirror.toPlainText(), "cat");
  }
  void projectApplyUsesDirtyBufferAndClosedFiles() {
    QTemporaryDir dir;
    const auto first = dir.filePath("a.txt"), second = dir.filePath("b.txt");
    write(first, "saved cat");
    write(second, "cat\r\ncat\r\n");
    editor->setPlainText("dirty cat");
    panel->setEditor(editor.get(), first);
    panel->setProjectPath(dir.path());
    panel->setGlobalMode(true);
    replacement()->setText("dog");
    search("cat");
    preview();
    applyBatch();
    QCOMPARE(editor->toPlainText(), "dirty dog");
    QCOMPARE(read(first), "saved cat");
    QCOMPARE(read(second), "dog\r\ndog\r\n");
    QSignalSpy undone(panel.get(), &FindReplacePanel::replacementFinished);
    panel->undoReplacementBatch();
    QVERIFY(undone.wait(3000));
    QCOMPARE(undone.first().first().toBool(), true);
    QCOMPARE(editor->toPlainText(), "dirty cat");
    QCOMPARE(read(second), "cat\r\ncat\r\n");
  }
  void staleDiskPreventsBufferEdits() {
    QTemporaryDir dir;
    const auto first = dir.filePath("a"), second = dir.filePath("b");
    write(first, "cat");
    write(second, "cat");
    editor->setPlainText("cat");
    panel->setEditor(editor.get(), first);
    panel->setProjectPath(dir.path());
    panel->setGlobalMode(true);
    replacement()->setText("dog");
    search("cat");
    preview();
    write(second, "external");
    applyBatch(false);
    QCOMPARE(editor->toPlainText(), "cat");
    QCOMPARE(read(second), "external");
    QVERIFY(status()->text().contains("changed"));
  }
  void undoRefusesLaterEditorChanges() {
    editor->setPlainText("cat");
    replacement()->setText("dog");
    search("cat");
    preview();
    applyBatch();
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText(" later");
    panel->undoReplacementBatch();
    QVERIFY(!panel->isBusy());
    QCOMPARE(editor->toPlainText(), "dog later");
    QVERIFY(status()->text().contains("cannot"));
  }
  void syntaxHighlightingDoesNotInvalidateBatchUndo() {
    class Highlighter : public QSyntaxHighlighter {
    public:
      using QSyntaxHighlighter::QSyntaxHighlighter;
      void highlightBlock(const QString &text) override {
        setFormat(0, text.size(), QColor(Qt::red));
      }
    };
    editor->setPlainText("cat");
    replacement()->setText("dog");
    search("cat");
    preview();
    applyBatch();
    Highlighter highlighter(editor->document());
    highlighter.rehighlight();
    QSignalSpy done(panel.get(), &FindReplacePanel::replacementFinished);
    panel->undoReplacementBatch();
    QVERIFY(done.wait(3000));
    QVERIFY(done.first().first().toBool());
    QCOMPARE(editor->toPlainText(), "cat");
  }
  void programmaticEditDuringCommitRollsBackDiskHalf() {
    QTemporaryDir dir;
    const auto first = dir.filePath("a"), second = dir.filePath("b");
    write(first, "cat");
    write(second, "cat");
    editor->setPlainText("cat");
    panel->setEditor(editor.get(), first);
    panel->setProjectPath(dir.path());
    panel->setGlobalMode(true);
    replacement()->setText("dog");
    search("cat");
    preview();
    QSignalSpy done(panel.get(), &FindReplacePanel::replacementFinished);
    panel->applyReplacements();
    QVERIFY(panel->isBusy());
    QVERIFY(editor->isReadOnly());
    editor->insertPlainText("external ");
    QVERIFY(done.wait(3000));
    QVERIFY(!done.first().first().toBool());
    QCOMPARE(editor->toPlainText(), "external cat");
    QCOMPARE(read(second), "cat");
    QVERIFY(!editor->isReadOnly());
  }
  void cancellingBeforeTheWorkerStartsLeavesFilesUntouched() {
    QTemporaryDir dir;
    const auto path = dir.filePath("a");
    write(path, "cat");
    panel->setEditor(nullptr);
    panel->setProjectPath(dir.path());
    panel->setGlobalMode(true);
    replacement()->setText("dog");
    search("cat");
    preview();
    QSignalSpy done(panel.get(), &FindReplacePanel::replacementFinished);
    panel->applyReplacements();
    panel->findChild<QToolButton *>("stopSearch")->click();
    QVERIFY(done.wait(3000));
    QVERIFY(!done.first().first().toBool());
    QCOMPARE(read(path), "cat");
    QVERIFY(!panel->isBusy());
  }
  void undoMirrorsAClosedFileOpenedAfterApply() {
    QTemporaryDir dir;
    const auto path = dir.filePath("a");
    write(path, "cat");
    QPlainTextEdit opened;
    QVector<FindReplacePanel::OpenDocument> docs;
    panel->setDocumentProvider([&]() { return docs; });
    panel->setEditor(nullptr);
    panel->setProjectPath(dir.path());
    panel->setGlobalMode(true);
    replacement()->setText("dog");
    search("cat");
    preview();
    applyBatch();
    opened.setPlainText("dog");
    opened.document()->setModified(false);
    docs.append({{}, path, "a", &opened});
    QSignalSpy done(panel.get(), &FindReplacePanel::replacementFinished);
    panel->undoReplacementBatch();
    QVERIFY(done.wait(3000));
    QVERIFY(done.first().first().toBool());
    QCOMPARE(read(path), "cat");
    QCOMPARE(opened.toPlainText(), "cat");
    QVERIFY(!opened.document()->isModified());
  }
  void replaceSingleNeedsTheSelectedMatchAndUsesOneUndoBlock() {
    editor->setPlainText("cat cat");
    replacement()->setText("dog");
    search("cat");
    auto *button = panel->findChild<QPushButton *>("replaceSingle");
    button->click();
    QCOMPARE(editor->toPlainText(), "cat cat");
    QCOMPARE(editor->textCursor().selectedText(), "cat");
    QSignalSpy done(panel.get(), &FindReplacePanel::replacementFinished);
    button->click();
    QVERIFY(done.wait(3000));
    QCOMPARE(editor->toPlainText(), "dog cat");
    editor->undo();
    QCOMPARE(editor->toPlainText(), "cat cat");
  }
  void undoRefusesLaterDiskChanges() {
    QTemporaryDir dir;
    const auto path = dir.filePath("a");
    write(path, "cat");
    panel->setEditor(nullptr);
    panel->setProjectPath(dir.path());
    panel->setGlobalMode(true);
    replacement()->setText("dog");
    search("cat");
    preview();
    applyBatch();
    write(path, "external");
    QSignalSpy done(panel.get(), &FindReplacePanel::replacementFinished);
    panel->undoReplacementBatch();
    QVERIFY(done.wait(3000));
    QCOMPARE(done.first().first().toBool(), false);
    QCOMPARE(read(path), "external");
  }
  void paginatedCheckboxesSelectWholeFile() {
    editor->setPlainText(QString("cat ").repeated(450));
    replacement()->setText("dog");
    search("cat");
    preview();
    QCOMPARE(tree()->topLevelItem(0)->childCount(), 200);
    tree()->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
    QVERIFY(!apply()->isEnabled());
    panel->findChild<QToolButton *>("nextResultsPage")->click();
    QCOMPARE(tree()->topLevelItem(0)->childCount(), 200);
    QCOMPARE(tree()->topLevelItem(0)->child(0)->checkState(0), Qt::Unchecked);
    tree()->topLevelItem(0)->child(0)->setCheckState(0, Qt::Checked);
    QVERIFY(apply()->text().contains("1 replacement"));
    applyBatch();
    QCOMPARE(editor->toPlainText().count("dog"), 1);
    QCOMPARE(editor->toPlainText().count("cat"), 449);
  }
  void closedFileOpenedSincePreviewBecomesUnsavedEdit() {
    QTemporaryDir dir;
    const auto path = dir.filePath("a");
    write(path, "cat");
    QPlainTextEdit opened;
    QVector<FindReplacePanel::OpenDocument> docs;
    panel->setDocumentProvider([&]() { return docs; });
    panel->setEditor(nullptr);
    panel->setProjectPath(dir.path());
    panel->setGlobalMode(true);
    replacement()->setText("dog");
    search("cat");
    preview();
    opened.setPlainText("cat");
    docs.append({{}, path, "a", &opened});
    applyBatch();
    QCOMPARE(opened.toPlainText(), "dog");
    QCOMPARE(read(path), "cat");
    QVERIFY(opened.document()->isModified());
  }
  void zeroLengthMatchNavigatesAndReplaces() {
    editor->setPlainText("ab");
    replacement()->setText("_");
    panel->findChild<QToolButton *>("btnRegex")->setChecked(true);
    search("(?=.)|$");
    panel->findNext();
    QCOMPARE(editor->textCursor().position(), 0);
    panel->findNext();
    QCOMPARE(editor->textCursor().position(), 1);
    preview();
    applyBatch();
    QCOMPARE(editor->toPlainText(), "_a_b_");
  }
  void loadingAResultDoesNotInvalidateItsPreview() {
    QTemporaryDir dir;
    const auto path = dir.filePath("a");
    write(path, "cat");
    QPlainTextEdit opened;
    QVector<FindReplacePanel::OpenDocument> docs;
    panel->setDocumentProvider([&]() { return docs; });
    panel->setEditor(nullptr);
    panel->setProjectPath(dir.path());
    panel->setGlobalMode(true);
    replacement()->setText("dog");
    search("cat");
    preview();
    connect(panel.get(), &FindReplacePanel::navigateToFile, panel.get(),
            [&](const QString &, int, int, int) {
              docs.append({{}, path, "a", &opened});
              panel->setEditor(&opened, path);
              opened.setPlainText("cat");
              opened.document()->setModified(false);
            });
    panel->findNext();
    QVERIFY(apply()->isVisible());
    QCOMPARE(opened.textCursor().selectedText(), "cat");
    applyBatch();
    QCOMPARE(opened.toPlainText(), "dog");
    QCOMPARE(read(path), "cat");
  }
  void escapeClosesPanelAndVimRestoresSearch() {
    editor->setPlainText("cat");
    search("cat");
    panel->setScope(SearchReplace::Scope::OpenFiles);
    panel->setVimCommandMode(true);
    panel->setSearchPrefix(":");
    panel->setSearchText("w");
    QVERIFY(!tree()->isVisible());
    QVERIFY(!replacement()->isVisible());
    panel->setVimCommandMode(false);
    QCOMPARE(query()->text(), "cat");
    QCOMPARE(panel->scope(), SearchReplace::Scope::OpenFiles);
    QTest::keyClick(query(), Qt::Key_Escape);
    QVERIFY(!panel->isVisible());
  }
};
QTEST_MAIN(TestSearchReplaceWidget)
#include "test_searchreplacewidget.moc"
