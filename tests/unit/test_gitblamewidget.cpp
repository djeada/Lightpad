#include "ui/widgets/gitblamewidget.h"
#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

class TestGitBlameWidget : public QObject {
  Q_OBJECT
private:
  const QString firstHash = QString(40, 'a');
  const QString secondHash = QString(40, 'b');
  QMap<int, GitBlameLineInfo> data() const {
    QMap<int, GitBlameLineInfo> result;
    for (int line = 1; line <= 7; ++line) {
      GitBlameLineInfo info;
      info.lineNumber = line;
      info.commitHash = line <= 3 ? firstHash : secondHash;
      info.shortHash = info.commitHash.left(7);
      info.author = line <= 3 ? "Alex Morgan" : "Jordan Chen";
      info.authorEmail = "author@example.invalid";
      info.date = "2026-10-20T12:30:00Z";
      info.relativeDate = line <= 3 ? "2 days ago" : "1 week ago";
      info.summary = line <= 3 ? "Handle retries without losing pending work"
                               : "Add order validation and a readable error";
      if (line == 7) {
        info.commitHash = QString(40, '0');
        info.shortHash = QString(7, '0');
      }
      result[line] = info;
    }
    return result;
  }
  void show(GitBlameWidget &widget) {
    widget.resize(256, 250);
    widget.setData(data());
    QVector<GitBlameWidget::VisibleLine> lines;
    for (int line = 1; line <= 7; ++line)
      lines.append({line, (line - 1) * 26, 26});
    widget.setVisibleLines(lines, 1);
    widget.show();
  }
  QFrame *details(GitBlameWidget &widget) {
    return widget.findChild<QFrame *>("gitBlameDetails");
  }

private slots:
  void clickingAGroupOpensTheCorrectFullCommit() {
    GitBlameWidget widget;
    show(widget);
    QSignalSpy opened(&widget, &GitBlameWidget::openCommitRequested);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(70, 38));
    auto *popup = details(widget);
    QVERIFY(popup);
    QVERIFY(popup->isVisible());
    QCOMPARE(popup->findChild<QLabel *>("gitBlameRegion")->text(),
             QString::fromUtf8("LAST CHANGE · LINES 1–3"));
    QCOMPARE(popup->findChild<QLabel *>("gitBlameSummary")->text(),
             data().value(2).summary);
    popup->findChild<QPushButton *>("gitBlameOpenCommit")->click();
    QCOMPARE(opened.count(), 1);
    QCOMPARE(opened.first().first().toString(), firstHash);
    QVERIFY(!popup->isVisible());
  }

  void uncommittedLinesHaveNoCommitActions() {
    GitBlameWidget widget;
    show(widget);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(70, 169));
    auto *popup = details(widget);
    QVERIFY(popup);
    QCOMPARE(popup->findChild<QLabel *>("gitBlameSummary")->text(),
             QString("Uncommitted changes"));
    QVERIFY(
        !popup->findChild<QPushButton *>("gitBlameOpenCommit")->isEnabled());
    QVERIFY(!popup->findChild<QPushButton *>("gitBlameCopySha")->isEnabled());
    QSignalSpy history(&widget, &GitBlameWidget::fileHistoryRequested);
    popup->findChild<QPushButton *>("gitBlameFileHistory")->click();
    QCOMPARE(history.count(), 1);
    QCOMPARE(history.first().first().toInt(), 7);
  }

  void replacingOrClearingDataDismissesOldDetails() {
    GitBlameWidget widget;
    show(widget);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(70, 12));
    QPointer<QFrame> popup = details(widget);
    QVERIFY(popup && popup->isVisible());
    widget.setData({});
    QVERIFY(!widget.hasData());
    QVERIFY(!popup || !popup->isVisible());
    widget.setFocus();
    QTest::keyClick(&widget, Qt::Key_Return);
    QVERIFY(!popup || !popup->isVisible());
  }

  void keyboardNavigationSelectsAndInspectsLines() {
    GitBlameWidget widget;
    show(widget);
    widget.activateWindow();
    widget.setFocus();
    QSignalSpy selected(&widget, &GitBlameWidget::lineSelected);
    QTest::keyClick(&widget, Qt::Key_Down);
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.first().first().toInt(), 2);
    QTest::keyClick(&widget, Qt::Key_Return);
    QPointer<QFrame> popup = details(widget);
    QVERIFY(popup && popup->isVisible());
    QTest::keyClick(popup, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QVERIFY(!popup || !popup->isVisible());
  }

  void editingInvalidatesAttributionWithoutCollapsingTheColumn() {
    GitBlameWidget widget;
    show(widget);
    const int previousWidth = widget.width();
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(70, 12));
    QPointer<QFrame> popup = details(widget);
    widget.markOutdated();
    QVERIFY(!widget.hasData());
    QVERIFY(widget.hasContent());
    QCOMPARE(widget.width(), previousWidth);
    QVERIFY(!popup || !popup->isVisible());
    QSignalSpy opened(&widget, &GitBlameWidget::openCommitRequested);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(70, 12));
    QCOMPARE(opened.count(), 0);
    widget.setData(data());
    QVERIFY(widget.hasData());
  }

  void wrappedAndFoldedGeometrySelectsTheSourceLine() {
    GitBlameWidget widget;
    show(widget);
    widget.setVisibleLines({{1, -10, 26}, {3, 16, 52}, {6, 68, 26}}, 1);
    QSignalSpy selected(&widget, &GitBlameWidget::lineSelected);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(70, 55));
    QCOMPARE(selected.first().first().toInt(), 3);
  }

  void renderThemes_data() {
    QTest::addColumn<bool>("dark");
    QTest::addColumn<int>("width");
    QTest::newRow("dark") << true << 256;
    QTest::newRow("light") << false << 256;
    QTest::newRow("narrow") << true << 150;
  }

  void renderThemes() {
    QFETCH(bool, dark);
    QFETCH(int, width);
    GitBlameWidget widget;
    show(widget);
    Theme theme;
    if (!dark) {
      theme.backgroundColor = QColor("#f8fafc");
      theme.foregroundColor = QColor("#172033");
      theme.surfaceColor = QColor("#f1f5f9");
      theme.surfaceAltColor = QColor("#e8edf3");
      theme.borderColor = QColor("#d9e1ed");
      theme.accentColor = QColor("#2563eb");
      theme.accentSoftColor = QColor("#dbeafe");
      theme.hoverColor = QColor("#e5eaf3");
    }
    widget.applyTheme(theme);
    widget.resize(width, 250);
    const QImage image = widget.grab().toImage();
    QVERIFY(!image.isNull());
    const QString output =
        qEnvironmentVariable("LIGHTPAD_BLAME_SCREENSHOT_DIR");
    if (!output.isEmpty()) {
      QVERIFY(
          image.save(output + "/blame-" + QTest::currentDataTag() + ".png"));
      QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier,
                        QPoint(50, 38));
      QVERIFY(details(widget)->grab().save(output + "/details-" +
                                           QTest::currentDataTag() + ".png"));
    }
  }
};

QTEST_MAIN(TestGitBlameWidget)
#include "test_gitblamewidget.moc"
