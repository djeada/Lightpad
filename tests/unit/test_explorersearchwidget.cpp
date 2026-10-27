#include "filetree/explorersearchwidget.h"
#include "settings/theme.h"
#include "theme/themedefinition.h"
#include "ui/dialogs/filequickopen.h"
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>
#include <QtTest>

class TestExplorerSearchWidget : public QObject {
  Q_OBJECT
private:
  QTemporaryDir m_root;
  QString createFile(const QString &name, const QByteArray &content) {
    const QString path = m_root.filePath(name);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(content) != content.size()) {
      qFatal("Could not create explorer fixture");
    }
    return path;
  }
  QTreeWidget *results(ExplorerSearchWidget &widget) {
    return widget.findChild<QTreeWidget *>("explorerSearchResults");
  }
  QLineEdit *query(ExplorerSearchWidget &widget) {
    return widget.findChild<QLineEdit *>("treeFilterEdit");
  }
  QLabel *status(ExplorerSearchWidget &widget) {
    return widget.findChild<QLabel *>("explorerSearchStatus");
  }
  void show(ExplorerSearchWidget &widget) {
    widget.resize(340, 620);
    widget.setRootPath(m_root.path());
    widget.show();
  }

private slots:
  void initTestCase() {
    QVERIFY(m_root.isValid());
    createFile("nested/deep/Needle.cpp", "unrelated\n");
    createFile("other.cpp", "first line\n  Needle();\n");
    createFile("omega.txt", "last result");
  }

  void filenameAndContentModesNavigatePrecisely() {
    ExplorerSearchWidget widget;
    show(widget);
    query(widget)->setText("Needle");
    QTRY_COMPARE(results(widget)->topLevelItemCount(), 1);
    QCOMPARE(results(widget)->topLevelItem(0)->text(0), "Needle.cpp");
    widget.findChild<QToolButton *>("searchContentsMode")->click();
    QTRY_COMPARE(results(widget)->topLevelItemCount(), 1);
    QCOMPARE(results(widget)->topLevelItem(0)->text(0), "other.cpp");
    QCOMPARE(results(widget)->topLevelItem(0)->childCount(), 1);
    QSignalSpy navigate(&widget, &ExplorerSearchWidget::navigateRequested);
    query(widget)->setFocus();
    QTest::keyClick(query(widget), Qt::Key_Return);
    QCOMPARE(navigate.count(), 1);
    QCOMPARE(navigate.first().at(0).toString(), m_root.filePath("other.cpp"));
    QCOMPARE(navigate.first().at(1).toInt(), 2);
    QCOMPARE(navigate.first().at(2).toInt(), 3);
    QCOMPARE(navigate.first().at(3).toInt(), 6);
  }

  void rapidEditsNeverShowOldResults() {
    ExplorerSearchWidget widget;
    show(widget);
    query(widget)->setText("Needle");
    QTest::qWait(185);
    query(widget)->setText("omega");
    QTRY_COMPARE(results(widget)->topLevelItemCount(), 1);
    QCOMPARE(results(widget)->topLevelItem(0)->text(0), "omega.txt");
    QTest::qWait(80);
    QCOMPARE(results(widget)->topLevelItem(0)->text(0), "omega.txt");
  }

  void invalidRegexAndNoResultsHaveExplicitFeedback() {
    ExplorerSearchWidget widget;
    show(widget);
    auto options = widget.options();
    options.mode = ProjectSearch::Mode::Contents;
    options.regularExpression = true;
    options.query = "[";
    widget.setOptions(options);
    QTRY_VERIFY(status(widget)->text().contains("Invalid regular expression"));
    QCOMPARE(results(widget)->topLevelItemCount(), 0);
    options.query = "no-such-text-anywhere";
    widget.setOptions(options);
    QTRY_VERIFY(status(widget)->text().contains("No results"));
  }

  void escapeRestoresTheTreeAndThenClearsScope() {
    ExplorerSearchWidget widget;
    show(widget);
    widget.searchInFolder(m_root.filePath("nested"));
    query(widget)->setText("Needle");
    QSignalSpy active(&widget, &ExplorerSearchWidget::searchActiveChanged);
    QTest::keyClick(query(widget), Qt::Key_Escape);
    QVERIFY(query(widget)->text().isEmpty());
    QVERIFY(!results(widget)->isVisible());
    QVERIFY(!active.isEmpty());
    QCOMPARE(active.last().first().toBool(), false);
    QVERIFY(!widget.options().scopePath.isEmpty());
    QTest::keyClick(query(widget), Qt::Key_Escape);
    QVERIFY(widget.options().scopePath.isEmpty());
  }

  void hiddenWidgetsDeferWorkAndResumeOnShow() {
    ExplorerSearchWidget widget;
    widget.setRootPath(m_root.path());
    widget.setSearchText("omega");
    QTest::qWait(220);
    QCOMPARE(results(widget)->topLevelItemCount(), 0);
    show(widget);
    QTRY_COMPARE(results(widget)->topLevelItemCount(), 1);
    QCOMPARE(results(widget)->topLevelItem(0)->text(0), "omega.txt");
  }

  void folderRevealCanSynchronouslyClearResults() {
    ExplorerSearchWidget widget;
    show(widget);
    widget.setSearchText("nested");
    QTRY_VERIFY(results(widget)->topLevelItemCount() > 0);
    const QString expected = m_root.filePath("nested");
    bool revealed = false;
    connect(&widget, &ExplorerSearchWidget::revealRequested, &widget,
            [&](const QString &path) {
              widget.setSearchText({});
              QCOMPARE(path, expected);
              revealed = true;
            });
    QTest::keyClick(query(widget), Qt::Key_Return);
    QVERIFY(revealed);
  }

  void stopBeforeDebounceDoesNotStartAJob() {
    ExplorerSearchWidget widget;
    show(widget);
    widget.setSearchText("Needle");
    widget.findChild<QToolButton *>("explorerStopSearch")->click();
    QTest::qWait(230);
    QCOMPARE(results(widget)->topLevelItemCount(), 0);
    QVERIFY(status(widget)->text().contains("cancelled"));
    widget.refresh();
    QTRY_COMPARE(results(widget)->topLevelItemCount(), 1);
  }

  void renderSearchInDarkAndLightThemes() {
    ExplorerSearchWidget widget;
    show(widget);
    widget.focusSearch(ProjectSearch::Mode::Contents);
    widget.setSearchText("Needle");
    QTRY_COMPARE(results(widget)->topLevelItemCount(), 1);
    const QString output =
        qEnvironmentVariable("LIGHTPAD_EXPLORER_SCREENSHOT_DIR");
    for (const QString &name : {QString("dark"), QString("light")}) {
      const bool dark = name == "dark";
      const auto theme = ThemeDefinition::generateFromSeed(
          ThemeDefinition::fromClassicTheme(Theme()), name, "test",
          QColor(dark ? "#111827" : "#f8fafc"),
          QColor(dark ? "#e5e7eb" : "#172033"), QColor("#3b82f6"));
      widget.applyTheme(theme);
      QCoreApplication::processEvents();
      if (!output.isEmpty()) {
        QVERIFY(widget.grab().save(output + "/explorer-" + name + ".png"));
      }
    }
  }

  void quickOpenFindsDeepFilesAndEmitsTheirAbsolutePath() {
    FileQuickOpen dialog;
    dialog.setRootDirectory(m_root.path());
    dialog.showDialog();
    auto *edit = dialog.findChild<QLineEdit *>("quickOpenSearch");
    auto *list = dialog.findChild<QListWidget *>();
    edit->setText("Needle");
    QTRY_COMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toString(),
             "nested/deep/Needle.cpp");
    QSignalSpy selected(&dialog, &FileQuickOpen::fileSelected);
    QTest::keyClick(edit, Qt::Key_Return);
    QCOMPARE(selected.size(), 1);
    QCOMPARE(selected.first().first().toString(),
             m_root.filePath("nested/deep/Needle.cpp"));
    QVERIFY(!dialog.isVisible());
  }

  void quickOpenDoesNotOpenStaleResultsDuringTyping() {
    FileQuickOpen dialog;
    dialog.setRootDirectory(m_root.path());
    dialog.showDialog();
    auto *edit = dialog.findChild<QLineEdit *>("quickOpenSearch");
    auto *list = dialog.findChild<QListWidget *>();
    edit->setText("Needle");
    QTRY_COMPARE(list->count(), 1);
    QSignalSpy selected(&dialog, &FileQuickOpen::fileSelected);
    edit->setText("no-such-file");
    QTest::keyClick(edit, Qt::Key_Return);
    QCOMPARE(selected.size(), 0);
    QTRY_COMPARE(dialog.findChild<QLabel *>("quickOpenStatus")->text(),
                 "No matching files");
  }

  void quickOpenKeepsTheDialogOpenWhenAResultDisappears() {
    const QString path = createFile("disappearing-file.txt", "contents");
    FileQuickOpen dialog;
    dialog.setRootDirectory(m_root.path());
    dialog.showDialog();
    auto *edit = dialog.findChild<QLineEdit *>("quickOpenSearch");
    auto *list = dialog.findChild<QListWidget *>("quickOpenResults");
    edit->setText("disappearing-file");
    QTRY_COMPARE(list->count(), 1);
    QVERIFY(QFile::remove(path));
    QSignalSpy selected(&dialog, &FileQuickOpen::fileSelected);
    QTest::keyClick(edit, Qt::Key_Return);
    QCOMPARE(selected.count(), 0);
    QVERIFY(dialog.isVisible());
    QCOMPARE(list->count(), 0);
    QVERIFY(dialog.findChild<QLabel *>("quickOpenStatus")->text()
                .contains("no longer available"));
  }

  void quickOpenRefreshesWhenReopened() {
    FileQuickOpen dialog;
    dialog.setRootDirectory(m_root.path());
    dialog.showDialog();
    auto *edit = dialog.findChild<QLineEdit *>("quickOpenSearch");
    auto *list = dialog.findChild<QListWidget *>();
    edit->setText("omega");
    QTRY_COMPARE(list->count(), 1);
    dialog.hide();
    createFile("fresh-file.txt", "new");
    dialog.showDialog();
    edit->setText("fresh-file");
    QTRY_COMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toString(), "fresh-file.txt");
    dialog.hide();
  }

  void quickOpenDiscardsResultsWhenItsRootChanges() {
    FileQuickOpen dialog;
    dialog.setRootDirectory(m_root.path());
    dialog.showDialog();
    auto *edit = dialog.findChild<QLineEdit *>("quickOpenSearch");
    auto *list = dialog.findChild<QListWidget *>();
    edit->setText("omega");
    QTRY_COMPARE(list->count(), 1);
    QTemporaryDir other;
    QVERIFY(other.isValid());
    dialog.setRootDirectory(other.path());
    QSignalSpy selected(&dialog, &FileQuickOpen::fileSelected);
    QTest::keyClick(edit, Qt::Key_Return);
    QCOMPARE(selected.count(), 0);
    QTRY_COMPARE(dialog.findChild<QLabel *>("quickOpenStatus")->text(),
                 "No matching files");
    dialog.hide();
  }

  void cleanupTestCase() { QTest::qWait(100); }
};

QTEST_MAIN(TestExplorerSearchWidget)
#include "test_explorersearchwidget.moc"
