#include "core/documentrecovery.h"
#include "ui/dialogs/documentrecoverydialog.h"
#include "ui/dialogs/themedmessagebox.h"
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSyntaxHighlighter>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>

class TestDocumentRecovery : public QObject {
  Q_OBJECT
  static void flush(DocumentRecovery &manager) {
    QSignalSpy done(&manager, &DocumentRecovery::checkpointed);
    manager.checkpoint();
    QVERIFY(done.wait(3000));
  }
  static Recovery::Draft orphan(const QString &root,
                                const QString &text = "recovered text") {
    Recovery::Store store(root);
    if (!store.start())
      qFatal("fixture");
    Recovery::Draft draft;
    draft.id = Recovery::newId();
    draft.title = "work.cpp";
    draft.text = text;
    draft.originalPath = "/missing/work.cpp";
    draft.updated = QDateTime::currentDateTimeUtc();
    if (!store.save(draft))
      qFatal("fixture");
    return draft;
  }
  static QVector<Recovery::Draft> records() {
    auto make = [](const QString &name, const QString &text) {
      Recovery::Draft draft;
      draft.id = Recovery::newId();
      draft.sessionId = Recovery::newId();
      draft.title = name;
      draft.text = text;
      draft.updated = QDateTime::currentDateTimeUtc();
      return draft;
    };
    return {make("a.cpp", "first <tag> 😀"), make("b.py", "second draft")};
  }
private slots:
  void untitledEditsCheckpointWithoutAutosave() {
    QTemporaryDir root;
    QPlainTextEdit editor;
    auto manager = std::make_unique<DocumentRecovery>(nullptr, root.path());
    manager->track(&editor, {}, "Untitled");
    editor.insertPlainText("unsaved new document");
    flush(*manager);
    manager.reset();
    DocumentRecovery reader(nullptr, root.path());
    QTRY_COMPARE(reader.recoverable().drafts.size(), 1);
    QCOMPARE(reader.recoverable().drafts.first().text, "unsaved new document");
    QVERIFY(reader.recoverable().drafts.first().originalPath.isEmpty());
  }
  void savedBaselineUndoAndRedoUpdateRecovery() {
    QTemporaryDir root;
    QPlainTextEdit editor;
    editor.setPlainText("saved");
    auto manager = std::make_unique<DocumentRecovery>(nullptr, root.path());
    manager->track(&editor, "source.cpp", "source.cpp");
    editor.moveCursor(QTextCursor::End);
    editor.insertPlainText(" edit");
    flush(*manager);
    editor.undo();
    QVERIFY(!editor.document()->isModified());
    editor.redo();
    QVERIFY(editor.document()->isModified());
    QTest::qWait(60);
    flush(*manager);
    manager.reset();
    DocumentRecovery reader(nullptr, root.path());
    QTRY_COMPARE(reader.recoverable().drafts.size(), 1);
    QCOMPARE(reader.recoverable().drafts.first().text, "saved edit");
  }
  void savingDeletesTheRecoveryCopy() {
    QTemporaryDir root;
    QPlainTextEdit editor;
    auto manager = std::make_unique<DocumentRecovery>(nullptr, root.path());
    manager->track(&editor, "a.cpp", "a.cpp");
    editor.insertPlainText("changed");
    flush(*manager);
    editor.document()->setModified(false);
    manager.reset();
    DocumentRecovery reader(nullptr, root.path());
    QTest::qWait(100);
    QVERIFY(reader.recoverable().drafts.isEmpty());
  }
  void discardBeforeWorkerStartsCannotResurrectDraft() {
    QTemporaryDir root;
    QPlainTextEdit editor;
    auto manager = std::make_unique<DocumentRecovery>(nullptr, root.path());
    manager->track(&editor, {}, "Untitled");
    editor.insertPlainText("do not recover");
    manager->checkpoint();
    manager->forget(&editor);
    manager.reset();
    DocumentRecovery reader(nullptr, root.path());
    QTest::qWait(150);
    QVERIFY(reader.recoverable().drafts.isEmpty());
  }
  void normalCloseCancelsQueuedWritesAndRemovesOnlyOwnCopies() {
    QTemporaryDir root;
    orphan(root.path(), "keep deferred draft");
    QPlainTextEdit editor;
    DocumentRecovery manager(nullptr, root.path());
    manager.track(&editor, {}, "Closing");
    editor.insertPlainText("discard approved");
    manager.checkpoint();
    QVERIFY(manager.finishSession());
    QTest::qWait(100);
    DocumentRecovery reader(nullptr, root.path());
    const auto recovered = reader.recoverable();
    QCOMPARE(recovered.drafts.size(), 1);
    QCOMPARE(recovered.drafts.first().text, "keep deferred draft");
  }
  void cleanDocumentAndSyntaxFormattingDoNotCreateDrafts() {
    class Highlighter : public QSyntaxHighlighter {
    public:
      using QSyntaxHighlighter::QSyntaxHighlighter;
      void highlightBlock(const QString &text) override {
        setFormat(0, text.size(), QColor(Qt::red));
      }
    };
    QTemporaryDir root;
    QPlainTextEdit editor;
    editor.setPlainText("saved");
    auto manager = std::make_unique<DocumentRecovery>(nullptr, root.path());
    manager->track(&editor, "a.cpp", "a.cpp");
    Highlighter highlighter(editor.document());
    highlighter.rehighlight();
    manager->checkpoint();
    QTest::qWait(50);
    manager.reset();
    DocumentRecovery reader(nullptr, root.path());
    QVERIFY(reader.recoverable().drafts.isEmpty());
  }
  void multipleEditorsKeepIndependentDrafts() {
    QTemporaryDir root;
    QPlainTextEdit first, second;
    auto manager = std::make_unique<DocumentRecovery>(nullptr, root.path());
    manager->track(&first, "a.cpp", "a.cpp");
    manager->track(&second, "a.cpp", "a.cpp");
    first.insertPlainText("view one");
    second.insertPlainText("view two");
    QSignalSpy done(manager.get(), &DocumentRecovery::checkpointed);
    manager->checkpoint();
    QTRY_COMPARE(done.count(), 2);
    manager.reset();
    DocumentRecovery reader(nullptr, root.path());
    QTRY_COMPARE(reader.recoverable().drafts.size(), 2);
  }
  void highlightingAnAlreadyCheckpointedDirtyDocumentDoesNotRewriteIt() {
    class Highlighter : public QSyntaxHighlighter {
    public:
      using QSyntaxHighlighter::QSyntaxHighlighter;
      void highlightBlock(const QString &text) override {
        setFormat(0, text.size(), QColor(Qt::red));
      }
    };
    QTemporaryDir root;
    QPlainTextEdit editor;
    DocumentRecovery manager(nullptr, root.path());
    manager.track(&editor, {}, "Untitled");
    editor.insertPlainText("dirty text");
    flush(manager);
    Highlighter highlighter(editor.document());
    highlighter.rehighlight();
    QSignalSpy done(&manager, &DocumentRecovery::checkpointed);
    manager.checkpoint();
    QTest::qWait(60);
    QCOMPARE(done.count(), 0);
  }
  void adoptedDraftSurvivesAnotherCrash() {
    QTemporaryDir root;
    orphan(root.path());
    QPlainTextEdit editor;
    auto manager = std::make_unique<DocumentRecovery>(nullptr, root.path());
    const auto draft = manager->recoverable().drafts.first();
    manager->track(&editor, {}, "Recovered");
    editor.setPlainText(draft.text);
    editor.document()->setModified(true);
    QVERIFY(manager->adopt(draft, &editor));
    QVERIFY(manager->recoverable().drafts.isEmpty());
    manager.reset();
    DocumentRecovery reader(nullptr, root.path());
    QTRY_COMPARE(reader.recoverable().drafts.size(), 1);
    QCOMPARE(reader.recoverable().drafts.first().text, draft.text);
  }
  void initializationFailureLeavesEditingAvailable() {
    QTemporaryDir root;
    const auto path = root.filePath("not-a-directory");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    DocumentRecovery manager(nullptr, path);
    QVERIFY(!manager.initializationError().isEmpty());
    QPlainTextEdit editor;
    manager.track(&editor, {}, "Untitled");
    editor.insertPlainText("still editable");
    manager.checkpoint();
    QCOMPARE(editor.toPlainText(), "still editable");
    QVERIFY(editor.document()->isModified());
  }
  void dialogPreviewsPlainTextAndRestoresOnlyCheckedRows() {
    auto drafts = records();
    DocumentRecoveryDialog dialog;
    QVector<QString> restored;
    dialog.setActions(
        [&]() { return Recovery::Scan{drafts, {}}; },
        [&](const Recovery::Draft &draft, QString *) {
          restored.append(draft.id);
          drafts.erase(
              std::remove_if(drafts.begin(), drafts.end(),
                             [&](const auto &d) { return d.id == draft.id; }),
              drafts.end());
          return true;
        },
        {});
    dialog.refresh();
    dialog.show();
    auto *list = dialog.findChild<QTreeWidget *>("recoveryDocuments");
    QCOMPARE(
        dialog.findChild<QPlainTextEdit *>("recoveryPreview")->toPlainText(),
        QString::fromUtf8("first <tag> 😀"));
    list->topLevelItem(1)->setCheckState(0, Qt::Unchecked);
    dialog.findChild<QPushButton *>("restoreRecovery")->click();
    QCOMPARE(restored.size(), 1);
    QCOMPARE(drafts.size(), 1);
    QCOMPARE(drafts.first().title, "b.py");
    QCOMPARE(list->topLevelItemCount(), 1);
    QCOMPARE(list->topLevelItem(0)->checkState(0), Qt::Unchecked);
  }
  void dialogFilterDoesNotRestoreHiddenRows() {
    auto drafts = records();
    QStringList restored;
    DocumentRecoveryDialog dialog;
    dialog.setActions([&]() { return Recovery::Scan{drafts, {}}; },
                      [&](const Recovery::Draft &draft, QString *) {
                        restored.append(draft.title);
                        return true;
                      },
                      {});
    dialog.refresh();
    dialog.findChild<QLineEdit *>("recoveryFilter")->setText("b.py");
    dialog.findChild<QPushButton *>("restoreRecovery")->click();
    QCOMPARE(restored, QStringList{"b.py"});
  }
  void laterKeepsCopiesAndDiscardRequiresConfirmation() {
    auto drafts = records();
    DocumentRecoveryDialog dialog;
    int discarded = 0;
    dialog.setActions([&]() { return Recovery::Scan{drafts, {}}; }, {},
                      [&](const Recovery::Draft &, QString *) {
                        ++discarded;
                        return true;
                      });
    dialog.refresh();
    dialog.show();
    dialog.findChild<QPushButton *>("recoverLater")->click();
    QCOMPARE(drafts.size(), 2);
    QCOMPARE(discarded, 0);
    dialog.show();
    QTimer::singleShot(0, []() {
      for (auto *widget : QApplication::topLevelWidgets())
        if (auto *box = qobject_cast<ThemedMessageBox *>(widget)) {
          for (auto *button : box->findChildren<QPushButton *>())
            if (button->text() == "Cancel")
              button->click();
        }
    });
    dialog.findChild<QPushButton *>("discardRecovery")->click();
    QCOMPARE(discarded, 0);
    QTimer::singleShot(0, []() {
      for (auto *widget : QApplication::topLevelWidgets())
        if (auto *box = qobject_cast<ThemedMessageBox *>(widget)) {
          for (auto *button : box->findChildren<QPushButton *>())
            if (button->text() == "Discard")
              button->click();
        }
    });
    dialog.findChild<QPushButton *>("discardRecovery")->click();
    QCOMPARE(discarded, 2);
  }
  void largePreviewIsBoundedAndRestoreReceivesFullText() {
    auto drafts = records();
    drafts[0].text = QString(300000, 'x');
    drafts.resize(1);
    int restoredCharacters = 0;
    DocumentRecoveryDialog dialog;
    dialog.setActions(
        [&]() { return Recovery::Scan{drafts, {"Unreadable copy retained"}}; },
        [&](const Recovery::Draft &draft, QString *) {
          restoredCharacters = draft.text.size();
          return true;
        },
        {});
    dialog.refresh();
    QCOMPARE(dialog.findChild<QPlainTextEdit *>("recoveryPreview")
                 ->toPlainText()
                 .size(),
             256 * 1024);
    dialog.findChild<QPushButton *>("restoreRecovery")->click();
    QCOMPARE(restoredCharacters, 300000);
    QVERIFY(!dialog.findChild<QPushButton *>("recoveryWarnings")->isHidden());
  }
};
QTEST_MAIN(TestDocumentRecovery)
#include "test_documentrecovery.moc"
