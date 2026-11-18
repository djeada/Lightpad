#include "database/databasemanager.h"
#include "settings/theme.h"
#include "ui/dialogs/connectiondialog.h"
#include "ui/panels/databasepanel.h"
#include "ui/panels/dbinsightsview.h"
#include "ui/panels/dbresultview.h"
#include "ui/panels/sqlconsoleedit.h"

#include "ui/tabnavigation.h"
#include <QClipboard>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest/QtTest>

namespace {

DbResultSet makeResult(const QStringList &cols,
                       const QVector<QVector<QVariant>> &rows) {
  DbResultSet rs;
  for (const QString &c : cols) {
    rs.columns.append({c, QString()});
  }
  rs.rows = rows;
  rs.totalRows = rows.size();
  return rs;
}

Theme testTheme() {
  Theme t;
  t.backgroundColor = QColor("#101418");
  t.foregroundColor = QColor("#e6edf3");
  t.surfaceColor = QColor("#161b22");
  t.surfaceAltColor = QColor("#1c232c");
  t.borderColor = QColor("#30363d");
  t.hoverColor = QColor("#21262d");
  t.pressedColor = QColor("#2d333b");
  t.accentColor = QColor("#58a6ff");
  t.accentSoftColor = QColor("#1f3a5f");
  t.successColor = QColor("#3fb950");
  t.warningColor = QColor("#d29922");
  t.errorColor = QColor("#f85149");
  t.highlightColor = QColor("#264f78");
  return t;
}

bool waitUntil(const std::function<bool()> &condition, int ms = 15000) {
  QElapsedTimer timer;
  timer.start();
  while (!condition() && timer.elapsed() < ms) {
    QTest::qWait(25);
  }
  return condition();
}

} // namespace

class TestDatabasePanel : public QObject {
  Q_OBJECT

private slots:

  void resultViewShowsNullsAndAlignsNumbers();
  void resultViewSortsNumbersNumerically();
  void resultViewFiltersRows();
  void resultViewCopiesSelectionInFormats();
  void resultViewExportsVisibleRowsToFile();
  void resultViewReportsTruncation();
  void resultViewSavesBlobBytes();
  void panelBrowsesPagesAndFiltersWholeTable();
  void panelInspectsSqliteStructure();
  void panelOpensSqliteFileOnce();

  void insightsListsEveryColumn();

  void consoleIndentationAndSelection();
  void consoleCompletionDoesNotReplaceSelection();
  void consoleMultilineSyntax();
  void tabNavigationPreservesWidgets();
  void consoleFindsStatementUnderCaret();
  void consoleRunShortcuts();
  void consoleHistoryShortcut();
  void consoleCompletesCtesAndReplacesQuotedIdentifiers();

  void panelRunsScriptAndShowsResults();
  void panelRunScriptExecutesCitySetup();
  void panelSurfacesErrorsInMessages();
  void panelBuildsSchemaTreeAndFilters();
  void panelRunsFromEditorWithConnectionDirective();
  void panelRecordsHistory();
  void panelReadOnlyConnectionRefusesWrites();
  void largeFontReachesSqlTreeResultsAndDialogs();
  void storageAndContextActionsPreserveResults();

  void connectionDialogListsProjectContainersAndTemplates();
  void connectionDialogFillsFormFromUrl();
  void connectionDialogTemplatesShapeTheForm();
  void connectionDialogEditsWithoutChooser();
  void panelCreatesScratchConnectionFromTemplate();
  void panelReadsPasswordFromEnvironment();
};

void TestDatabasePanel::connectionDialogListsProjectContainersAndTemplates() {
  QTemporaryDir dir;
  QFile env(dir.filePath(".env"));
  QVERIFY(env.open(QIODevice::WriteOnly));
  env.write("DATABASE_URL=postgresql://app:pw@db.local:5432/shop\n");
  env.close();

  ConnectionDialog dlg;
  dlg.applyTheme(testTheme());
  dlg.setWorkspaceFolder(dir.path());
  DbConnectionProfile saved;
  saved.name = "Shop";
  saved.engine = DbEngine::PostgreSql;
  saved.host = "db.local";
  saved.user = "app";
  saved.database = "shop";
  dlg.setSavedProfiles({saved});
  QVERIFY(dlg.isChoosing());

  auto *choices = dlg.findChild<QTreeWidget *>("dbChoices");
  QVERIFY(choices);
  QStringList groups;
  for (int i = 0; i < choices->topLevelItemCount(); ++i) {
    groups << choices->topLevelItem(i)->text(0);
  }
  QCOMPARE(groups.first(), QString("Found in this project"));
  QVERIFY(groups.contains("Cloud"));
  QVERIFY(groups.contains("Files"));
  // The project's database is offered first and marked as already saved.
  QVERIFY(choices->currentItem());
  QVERIFY(choices->currentItem()->text(0).contains("DATABASE_URL"));
  QVERIFY(choices->currentItem()->text(0).contains("saved as Shop"));

  auto *search = dlg.findChild<QLineEdit *>("dbChooserSearch");
  search->setText("neon");
  QVERIFY(choices->currentItem());
  QCOMPARE(choices->currentItem()->text(0), QString("Neon"));
  search->setText("no such database anywhere");
  QVERIFY(!choices->currentItem());
  search->clear();

  // Continue on the detected entry fills the form with its password.
  for (int i = 0; i < choices->topLevelItemCount(); ++i) {
    if (choices->topLevelItem(i)->text(0) == "Found in this project") {
      choices->setCurrentItem(choices->topLevelItem(i)->child(0));
    }
  }
  dlg.findChild<QPushButton *>("dbChooserContinue")->click();
  QVERIFY(!dlg.isChoosing());
  const DbConnectionProfile p = dlg.profile();
  QCOMPARE(p.host, QString("db.local"));
  QCOMPARE(p.database, QString("shop"));
  QCOMPARE(dlg.password(), QString("pw"));
  QVERIFY(p.name.contains("DATABASE_URL"));
}

void TestDatabasePanel::connectionDialogFillsFormFromUrl() {
  ConnectionDialog dlg;
  dlg.applyTheme(testTheme());
  auto *chooserUrl = dlg.findChild<QLineEdit *>("dbChooserUrl");
  chooserUrl->setText("mysql://reporter:s3cret@mysql.internal:3307/sales"
                      "?ssl-mode=REQUIRED");
  QTest::keyClick(chooserUrl, Qt::Key_Return);
  QVERIFY(!dlg.isChoosing());
  DbConnectionProfile p = dlg.profile();
  QCOMPARE(p.engine, DbEngine::MySql);
  QCOMPARE(p.host, QString("mysql.internal"));
  QCOMPARE(p.port, 3307);
  QCOMPARE(p.user, QString("reporter"));
  QCOMPARE(p.database, QString("sales"));
  QCOMPARE(p.sslMode, QString("require"));
  QCOMPARE(dlg.password(), QString("s3cret"));
  QVERIFY(!p.name.isEmpty());

  // Typing a different URL into the form's URL field refills it, and the
  // fields write back to the URL (without the password).
  auto *url = dlg.findChild<QLineEdit *>("dbConnectionUrl");
  url->setFocus();
  url->selectAll();
  QTest::keyClicks(url, "postgres://ro@pg.example.com/analytics");
  p = dlg.profile();
  QCOMPARE(p.engine, DbEngine::PostgreSql);
  QCOMPARE(p.host, QString("pg.example.com"));
  QCOMPARE(p.database, QString("analytics"));
  QCOMPARE(p.port, 0);
  dlg.findChild<QLineEdit *>("dbDatabase")->setFocus();
  dlg.findChild<QLineEdit *>("dbDatabase")->setText("warehouse");
  QCOMPARE(url->text(),
           QString("postgresql://ro@pg.example.com:5432/warehouse"));
}

void TestDatabasePanel::connectionDialogTemplatesShapeTheForm() {
  ConnectionDialog dlg;
  dlg.applyTheme(testTheme());
  QVERIFY(dlg.startFromTemplate("supabase"));
  DbConnectionProfile p = dlg.profile();
  QCOMPARE(p.engine, DbEngine::PostgreSql);
  QCOMPARE(p.sslMode, QString("require"));
  QVERIFY(p.host.isEmpty());
  QString why;
  QVERIFY(!p.isValid(&why));
  QVERIFY(why.contains("host"));
  auto *host = dlg.findChild<QLineEdit *>("dbHost");
  QVERIFY(host->placeholderText().contains("supabase.co"));
  // Saving without a host is refused and says why.
  dlg.findChild<QPushButton *>("dbSave")->click();
  QVERIFY(dlg.result() != QDialog::Accepted);
  QVERIFY(dlg.findChild<QLabel *>("dbTestStatus")->text().contains("host"));
  host->setText("db.abc.supabase.co");
  QCOMPARE(dlg.profile().name, QString("Supabase · db"));

  QVERIFY(dlg.startFromTemplate("sqlserver.docker"));
  p = dlg.profile();
  QCOMPARE(p.transport, DbTransport::Docker);
  QCOMPARE(p.user, QString("sa"));
  QVERIFY(p.sslMode.isEmpty());

  QVERIFY(dlg.startFromTemplate("cockroach.local"));
  p = dlg.profile();
  // Loading must not treat the template's "root" as MySQL's default user.
  QCOMPARE(p.user, QString("root"));
  QCOMPARE(p.port, 26257);
  QCOMPARE(p.passwordSource, DbPasswordSource::None);
  QVERIFY(dlg.password().isEmpty());

  // Switching engines by hand swaps the old engine's defaults.
  QVERIFY(dlg.startFromTemplate("postgres.local"));
  auto *engine = dlg.findChild<QComboBox *>("dbEngine");
  engine->setCurrentIndex(engine->findData(int(DbEngine::MySql)));
  p = dlg.profile();
  QCOMPARE(p.user, QString("root"));
  QCOMPARE(p.effectivePort(), 3306);
  QVERIFY(!dlg.startFromTemplate("missing.template"));
}

void TestDatabasePanel::connectionDialogEditsWithoutChooser() {
  ConnectionDialog dlg;
  dlg.applyTheme(testTheme());
  DbConnectionProfile p;
  p.id = "abc";
  p.name = "Prod";
  p.engine = DbEngine::PostgreSql;
  p.host = "db.prod";
  p.user = "app";
  p.sslMode = "verify-full";
  p.passwordSource = DbPasswordSource::Environment;
  p.passwordEnv = "PROD_PW";
  p.color = "#f85149";
  dlg.setProfile(p);
  dlg.show();
  QVERIFY(!dlg.isChoosing());
  QVERIFY(!dlg.findChild<QPushButton *>("dbBackToChooser")->isVisible());
  const DbConnectionProfile back = dlg.profile();
  QCOMPARE(back.id, p.id);
  QCOMPARE(back.name, p.name);
  QCOMPARE(back.sslMode, p.sslMode);
  QCOMPARE(back.passwordSource, p.passwordSource);
  QCOMPARE(back.passwordEnv, p.passwordEnv);
  QCOMPARE(back.color, p.color);
  QVERIFY(back.sameTargetAs(p));
  QVERIFY(dlg.findChild<QLineEdit *>("dbPasswordEnv")->isVisible());
  QVERIFY(!dlg.findChild<QLineEdit *>("dbPassword")->isVisible());
}

void TestDatabasePanel::panelCreatesScratchConnectionFromTemplate() {
  QTemporaryDir dir;
  DatabaseManager manager(dir.filePath("connections.json"),
                          dir.filePath("history.json"));
  DatabasePanel panel(&manager);
  panel.applyTheme(testTheme());
  panel.resize(1000, 600);
  panel.show();
  bool dialogSeen = false;
  QTimer::singleShot(0, this, [&]() {
    auto *dlg =
        qobject_cast<ConnectionDialog *>(QApplication::activeModalWidget());
    QVERIFY(dlg);
    dialogSeen = true;
    QVERIFY(!dlg->isChoosing());
    dlg->findChild<QPushButton *>("dbSaveConnect")->click();
  });
  panel.newConnectionFrom("sqlite.memory");
  QVERIFY(dialogSeen);
  QCOMPARE(manager.profiles().size(), 1);
  DbConnection *conn = manager.activeConnection();
  QVERIFY(conn);
  QCOMPARE(conn->profile().filePath, QString(":memory:"));
  QVERIFY(waitUntil([&]() { return conn->isConnected(); }));
  panel.runSql("CREATE TABLE notes(body TEXT); INSERT INTO notes VALUES ('hi');"
               " SELECT body FROM notes;");
  QVERIFY(waitUntil([&]() { return !panel.isRunning(); }));
  QVERIFY(panel.resultTabs()->count() >= 1);
}

void TestDatabasePanel::panelReadsPasswordFromEnvironment() {
  QTemporaryDir dir;
  QFile env(dir.filePath(".env"));
  QVERIFY(env.open(QIODevice::WriteOnly));
  env.write("LP_PANEL_TEST_PASSWORD=from-dotenv\n");
  env.close();
  DatabaseManager manager(dir.filePath("connections.json"),
                          dir.filePath("history.json"));
  DatabasePanel panel(&manager);
  panel.setWorkspaceFolder(dir.path());
  DbConnectionProfile p;
  p.name = "env";
  p.engine = DbEngine::PostgreSql;
  p.host = "127.0.0.1";
  p.port = 1; // nothing listens there; only the password lookup matters
  p.clientPath = "/bin/false";
  p.passwordSource = DbPasswordSource::Environment;
  p.passwordEnv = "LP_PANEL_TEST_PASSWORD";
  const QString id = manager.saveProfile(p);
  DbConnection *conn = manager.connection(id);
  // No prompt appears: the value comes from the project's .env.
  QTimer::singleShot(2000, this, []() {
    if (QWidget *modal = QApplication::activeModalWidget()) {
      modal->close();
      QFAIL("A password prompt appeared");
    }
  });
  panel.connectConnection(id, false);
  QVERIFY(conn->hasSessionPassword());
  QVERIFY(waitUntil([&]() {
    return conn->state() == DbConnectionState::Failed ||
           conn->state() == DbConnectionState::Connected;
  }));
  panel.setWorkspaceFolder(QString());
}

void TestDatabasePanel::largeFontReachesSqlTreeResultsAndDialogs() {
  const QString originalStyle = qApp->styleSheet();
  const auto restoreStyle =
      qScopeGuard([&]() { qApp->setStyleSheet(originalStyle); });
  qApp->setStyleSheet("QWidget { font-size: 12px; } QHeaderView::section { "
                      "font-size: 10px; } QTabBar::tab { font-size: 11px; }");
  QTemporaryDir dir;
  DatabaseManager manager(dir.filePath("connections.json"),
                          dir.filePath("history.json"));
  DatabasePanel panel(&manager);
  panel.applyTheme(testTheme());
  QFont font("monospace", 24);
  panel.setContentFont(font);
  panel.resize(1200, 700);
  panel.show();
  QCOMPARE(panel.console()->font().pointSizeF(), 24.0);
  QCOMPARE(panel.tree()->font().pointSizeF(), 24.0);
  QFile databaseFile(dir.filePath("readability.db"));
  QVERIFY(databaseFile.open(QIODevice::WriteOnly));
  databaseFile.close();
  panel.openSqliteFile(databaseFile.fileName());
  QVERIFY(waitUntil([&]() {
    return manager.activeConnection() &&
           manager.activeConnection()->isConnected();
  }));
  panel.runSql("SELECT 42 AS id, 'A readable result' AS title;");
  QVERIFY(waitUntil([&]() { return panel.resultTabs()->count() == 1; }));
  auto *result = qobject_cast<DbResultView *>(panel.resultTabs()->widget(0));
  QVERIFY(result);
  QCOMPARE(result->table()->font().pointSizeF(), 24.0);
  QVERIFY(result->table()->verticalHeader()->defaultSectionSize() >=
          result->table()->fontMetrics().height() + 10);
  QVERIFY(result->table()->horizontalHeader()->height() >=
          result->table()->fontMetrics().height());
  panel.applyTheme(testTheme());
  QCOMPARE(result->table()->font().pointSizeF(), 24.0);
  const auto index = result->proxy()->index(0, 1);
  QVERIFY(QMetaObject::invokeMethod(result, "showCellValue",
                                    Q_ARG(QModelIndex, index)));
  auto *dialog = result->findChild<QDialog *>();
  QVERIFY(dialog);
  auto *text = dialog->findChild<QPlainTextEdit *>();
  QVERIFY(text);
  QCOMPARE(text->font().pointSizeF(), 24.0);
  font.setPointSize(18);
  panel.setContentFont(font);
  QCOMPARE(panel.console()->font().pointSizeF(), 18.0);
  QCOMPARE(result->table()->font().pointSizeF(), 18.0);
  QCOMPARE(text->font().pointSizeF(), 18.0);
  dialog->close();
}

void TestDatabasePanel::resultViewShowsNullsAndAlignsNumbers() {
  DbResultView view;
  view.applyTheme(testTheme());
  view.setResult(makeResult({"id", "name"}, {{1, "Ann"}, {2, QVariant()}}),
                 DbEngine::Sqlite);
  auto *m = view.model();
  QCOMPARE(m->rowCount(), 2);
  QCOMPARE(m->index(1, 1).data(Qt::DisplayRole).toString(), QString("NULL"));
  QVERIFY(m->index(1, 1).data(DbResultModel::IsNullRole).toBool());
  QVERIFY(m->index(1, 1).data(Qt::FontRole).value<QFont>().italic());
  QVERIFY(m->isNumericColumn(0));
  QVERIFY(!m->isNumericColumn(1));
  QVERIFY(m->index(0, 0).data(Qt::TextAlignmentRole).toInt() & Qt::AlignRight);

  view.setResult(makeResult({"t"}, {{"a\nb"}}), DbEngine::Sqlite);
  QVERIFY(!m->index(0, 0).data(Qt::DisplayRole).toString().contains('\n'));
  QCOMPARE(m->index(0, 0).data(DbResultModel::RawValueRole).toString(),
           QString("a\nb"));
}

void TestDatabasePanel::resultViewSortsNumbersNumerically() {
  DbResultView view;
  view.setResult(
      makeResult({"n"}, {{"10"}, {"9"}, {"100"}, {QVariant()}, {"2"}}),
      DbEngine::Sqlite);
  view.table()->sortByColumn(0, Qt::AscendingOrder);
  QStringList order;
  for (int r = 0; r < view.proxy()->rowCount(); ++r) {
    order << view.proxy()->index(r, 0).data(Qt::DisplayRole).toString();
  }
  QCOMPARE(order, (QStringList{"NULL", "2", "9", "10", "100"}));
  view.table()->sortByColumn(0, Qt::DescendingOrder);
  QCOMPARE(view.proxy()->index(0, 0).data(Qt::DisplayRole).toString(),
           QString("100"));
}

void TestDatabasePanel::resultViewFiltersRows() {
  DbResultView view;
  view.setResult(makeResult({"name", "country"},
                            {{"Ann", "US"}, {"Bo", "CN"}, {"Cy", QVariant()}}),
                 DbEngine::Sqlite);
  view.proxy()->setFilterText("us");
  QCOMPARE(view.proxy()->rowCount(), 1);
  view.proxy()->setFilterText("null");
  QCOMPARE(view.proxy()->rowCount(), 1);
  view.proxy()->setFilterText("zzz");
  QCOMPARE(view.proxy()->rowCount(), 0);
  view.proxy()->setFilterText("");
  QCOMPARE(view.proxy()->rowCount(), 3);
}

void TestDatabasePanel::resultViewCopiesSelectionInFormats() {
  DbResultView view;
  view.resize(600, 300);
  view.setResult(
      makeResult({"id", "note"}, {{1, "a,b"}, {2, "plain"}, {3, QVariant()}}),
      DbEngine::Sqlite, "notes");
  view.table()->selectAll();
  QCOMPARE(view.selectionAs(ResultExporter::Format::Csv, true),
           QString("id,note\n1,\"a,b\"\n2,plain\n3,\n"));
  QCOMPARE(view.selectionAs(ResultExporter::Format::Tsv, false),
           QString("1\ta,b\n2\tplain\n3\t\n"));
  QVERIFY(
      view.selectionAs(ResultExporter::Format::SqlInsert, true)
          .contains("INSERT INTO notes (\"id\", \"note\") VALUES (1, 'a,b');"));

  view.table()->clearSelection();
  view.table()->selectionModel()->select(view.proxy()->index(1, 1),
                                         QItemSelectionModel::Select);
  QCOMPARE(view.selectionAs(ResultExporter::Format::Csv, false),
           QString("plain\n"));
}

void TestDatabasePanel::resultViewExportsVisibleRowsToFile() {
  QTemporaryDir dir;
  DbResultView view;
  view.setResult(makeResult({"name"}, {{"Ann"}, {"Bo"}, {"Cy"}}),
                 DbEngine::Sqlite);
  view.proxy()->setFilterText("b");
  const QString path = dir.filePath("out.csv");
  QVERIFY(view.exportToFile(path, ResultExporter::Format::Csv));
  QFile f(path);
  QVERIFY(f.open(QIODevice::ReadOnly));
  QCOMPARE(QString::fromUtf8(f.readAll()), QString("name\nBo\n"));

  view.proxy()->setFilterText("zzz");
  QVERIFY(view.exportToFile(path, ResultExporter::Format::Csv));
  QFile g(path);
  QVERIFY(g.open(QIODevice::ReadOnly));
  QCOMPARE(QString::fromUtf8(g.readAll()), QString("name\n"));
  QVERIFY(QMetaObject::invokeMethod(
      &view, "copyAs", Qt::DirectConnection,
      Q_ARG(ResultExporter::Format, ResultExporter::Format::Csv)));
  QCOMPARE(QApplication::clipboard()->text(), QString("name\n"));
  QVERIFY(!view.exportToFile(dir.filePath("missing/dir/out.csv"),
                             ResultExporter::Format::Csv));
}

void TestDatabasePanel::resultViewReportsTruncation() {
  DbResultView view;
  view.applyTheme(testTheme());
  DbResultSet rs = makeResult({"n"}, {{1}, {2}});
  rs.truncated = true;
  rs.totalRows = 50000;
  view.setResult(rs, DbEngine::Sqlite, QString(), 12);
  const auto *label = view.findChild<QLabel *>("dbResultSummary");
  QVERIFY(label);
  QVERIFY(label->text().contains("50000"));
  QVERIFY(label->text().contains("12 ms"));
}

void TestDatabasePanel::insightsListsEveryColumn() {
  DbInsightsView view;
  view.applyTheme(testTheme());
  view.setResult(makeResult({"id", "name", "born"}, {{1, "Ann", "1990-01-02"},
                                                     {2, "Bo", QVariant()},
                                                     {3, "Cy", "1985-05-06"}}),
                 "people");
  QCOMPARE(view.table()->rowCount(), 3);
  QCOMPARE(view.profiles().size(), 3);
  QCOMPARE(view.profiles()[0].kind, ColumnKind::Number);
  QCOMPARE(view.profiles()[1].kind, ColumnKind::Text);
  QCOMPARE(view.profiles()[2].kind, ColumnKind::Temporal);
  QCOMPARE(view.profiles()[2].nulls, qint64(1));
  view.clear();
  QCOMPARE(view.table()->rowCount(), 0);
}

void TestDatabasePanel::consoleFindsStatementUnderCaret() {
  SqlConsoleEdit edit;
  edit.setEngine(DbEngine::PostgreSql);
  edit.setPlainText("SELECT 1;\n\nSELECT 2 FROM t;\nSELECT 3;");
  QTextCursor c = edit.textCursor();
  c.setPosition(edit.toPlainText().indexOf("2 FROM"));
  edit.setTextCursor(c);
  QCOMPARE(edit.selectionOrCurrentStatement(), QString("SELECT 2 FROM t"));

  c.setPosition(0);
  c.setPosition(8, QTextCursor::KeepAnchor);
  edit.setTextCursor(c);
  QCOMPARE(edit.selectionOrCurrentStatement(), QString("SELECT 1"));
}

void TestDatabasePanel::consoleIndentationAndSelection() {
  SqlConsoleEdit edit;
  edit.setTabWidth(2);
  edit.show();
  edit.setFocus();
  edit.setPlainText("SELECT a,\n       b\nFROM items;");
  QTextCursor c = edit.textCursor();
  c.setPosition(19); // start of FROM, which must not be indented
  c.setPosition(0, QTextCursor::KeepAnchor);
  edit.setTextCursor(c);
  const QString before = edit.toPlainText();
  QTest::keyClick(&edit, Qt::Key_Tab);
  QCOMPARE(edit.toPlainText(), QString("  SELECT a,\n         b\nFROM items;"));
  QVERIFY(edit.textCursor().hasSelection());
  QVERIFY(edit.textCursor().position() < edit.textCursor().anchor());
  QTest::keyClick(&edit, Qt::Key_Backtab);
  QCOMPARE(edit.toPlainText(), before);
  edit.undo();
  QCOMPARE(edit.toPlainText(), QString("  SELECT a,\n         b\nFROM items;"));
  edit.setPlainText("  SELECT (");
  edit.moveCursor(QTextCursor::End);
  QTest::keyClick(&edit, Qt::Key_Return);
  QCOMPARE(edit.toPlainText(), QString("  SELECT (\n    "));
  QTest::keyClick(&edit, Qt::Key_Backspace);
  QCOMPARE(edit.toPlainText(), QString("  SELECT (\n  "));
  edit.setReadOnly(true);
  QTest::keyClick(&edit, Qt::Key_Tab);
  QCOMPARE(edit.toPlainText(), QString("  SELECT (\n  "));
}

void TestDatabasePanel::consoleCompletionDoesNotReplaceSelection() {
  SqlConsoleEdit edit;
  edit.setTabWidth(4);
  edit.show();
  edit.setFocus();
  edit.setPlainText("SEL");
  edit.moveCursor(QTextCursor::End);
  QTest::keyClick(&edit, Qt::Key_Space, Qt::ControlModifier);
  auto *completer = edit.findChild<QCompleter *>();
  QVERIFY(completer);
  QTRY_VERIFY(completer->popup()->isVisible());
  edit.selectAll();
  QVERIFY(!completer->popup()->isVisible());
  QTest::keyClick(&edit, Qt::Key_Tab);
  QCOMPARE(edit.toPlainText(), QString("    SEL"));
  QSignalSpy run(&edit, &SqlConsoleEdit::runStatementRequested);
  const QString selection = edit.selectionOrCurrentStatement();
  QTest::keyClick(&edit, Qt::Key_Return, Qt::ControlModifier);
  QCOMPARE(run.count(), 1);
  QCOMPARE(edit.selectionOrCurrentStatement(), selection);
}

void TestDatabasePanel::consoleMultilineSyntax() {
  QTextDocument document;
  SqlHighlighter highlighter(&document);
  const Theme theme = testTheme();
  highlighter.setTheme(theme);
  document.setPlainText(
      "SELECT 'hello\nSELECT -- still string\nworld'; SELECT 1;\n"
      "SELECT [a]]b\nFROM still identifier];");
  highlighter.rehighlight();
  const auto colorAt = [&](int block, int column) {
    for (const auto &range :
         document.findBlockByNumber(block).layout()->formats())
      if (column >= range.start && column < range.start + range.length)
        return range.format.foreground().color();
    return QColor();
  };
  QCOMPARE(colorAt(1, 0), theme.quotationFormat);
  QCOMPARE(colorAt(2, 8), theme.keywordFormat_0);
  QCOMPARE(colorAt(4, 0), theme.classFormat);
  QCOMPARE(document.lastBlock().userState(), 0);
}

void TestDatabasePanel::tabNavigationPreservesWidgets() {
  QTabWidget tabs;
  auto *first = new QWidget;
  auto *second = new QWidget;
  auto *sentinel = new QWidget;
  tabs.addTab(first, "one");
  tabs.addTab(second, "two");
  tabs.addTab(sentinel, "hidden");
  tabs.setTabEnabled(2, false);
  tabs.tabBar()->setTabVisible(2, false);
  tabs.setCurrentWidget(first);
  TabNavigation::step(&tabs, 1, true);
  QCOMPARE(tabs.widget(1), first);
  QCOMPARE(tabs.currentWidget(), first);
  QCOMPARE(tabs.tabText(1), QString("one"));
  TabNavigation::step(&tabs, 1, true);
  QCOMPARE(tabs.widget(2), sentinel);
  TabNavigation::step(&tabs, -1, false);
  QCOMPARE(tabs.currentWidget(), second);
  TabNavigation::install(&tabs);
  tabs.show();
  tabs.activateWindow();
  tabs.setFocus();
  QTest::qWait(20);
  QTest::keyClick(&tabs, Qt::Key_PageDown, Qt::ControlModifier);
  QTRY_COMPARE(tabs.currentWidget(), first);
  QTest::keyClick(&tabs, Qt::Key_PageUp,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QTRY_COMPARE(tabs.widget(0), first);
  QCOMPARE(tabs.currentWidget(), first);
}

void TestDatabasePanel::consoleRunShortcuts() {
  SqlConsoleEdit edit;
  edit.show();
  QSignalSpy stmt(&edit, &SqlConsoleEdit::runStatementRequested);
  QSignalSpy script(&edit, &SqlConsoleEdit::runScriptRequested);
  edit.setFocus();
  QTest::keyClick(&edit, Qt::Key_Return, Qt::ControlModifier);
  QCOMPARE(stmt.count(), 1);
  QCOMPARE(script.count(), 0);
  QTest::keyClick(&edit, Qt::Key_Return,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(script.count(), 1);

  QTest::keyClick(&edit, Qt::Key_Return);
  QCOMPARE(stmt.count(), 1);
  QCOMPARE(edit.toPlainText(), QString("\n"));
}

void TestDatabasePanel::consoleHistoryShortcut() {
  SqlConsoleEdit edit;
  edit.show();
  QSignalSpy history(&edit, &SqlConsoleEdit::historyStep);
  edit.setFocus();
  QTest::keyClick(&edit, Qt::Key_Up, Qt::AltModifier);
  QTest::keyClick(&edit, Qt::Key_Down, Qt::AltModifier);
  QCOMPARE(history.count(), 2);
  QCOMPARE(history[0][0].toInt(), -1);
  QCOMPARE(history[1][0].toInt(), 1);
}

namespace {

struct PanelFixture {
  QTemporaryDir dir;
  std::unique_ptr<DatabaseManager> manager;
  std::unique_ptr<DatabasePanel> panel;
  QString sqliteId;

  bool init(bool readOnly = false) {
    const QString dbFile = dir.filePath("test.db");
    QFile f(dbFile);
    if (!f.open(QIODevice::WriteOnly)) {
      return false;
    }
    f.close();
    manager = std::make_unique<DatabaseManager>(dir.filePath("c.json"),
                                                dir.filePath("h.json"));
    DbConnectionProfile p;
    p.name = "Test SQLite";
    p.engine = DbEngine::Sqlite;
    p.filePath = dbFile;
    p.readOnly = readOnly;
    p.confirmDestructive = false;
    sqliteId = manager->saveProfile(p);
    manager->setActive(sqliteId);
    panel = std::make_unique<DatabasePanel>(manager.get());
    panel->applyTheme(testTheme());
    panel->resize(1100, 700);
    panel->show();
    return true;
  }
  bool connectPanel() {
    panel->connectConnection(sqliteId, false);
    return waitUntil(
        [&]() { return manager->connection(sqliteId)->isConnected(); });
  }
};

QStringList treeTexts(QTreeWidget *tree) {
  QStringList out;
  QTreeWidgetItemIterator it(tree);
  while (*it) {
    out << (*it)->text(0);
    ++it;
  }
  return out;
}

bool useContextAction(DatabasePanel *panel, const std::function<void()> &open,
                      const QString &name, QStringList *labels = nullptr,
                      qreal *fontSize = nullptr) {
  bool found = false;
  QTimer::singleShot(0, panel, [&] {
    auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    if (!menu)
      return;
    if (fontSize)
      *fontSize = menu->font().pointSizeF();
    for (auto *action : menu->actions()) {
      if (labels)
        *labels << action->text();
      if (action->text().startsWith(name) && !name.isEmpty()) {
        found = true;
        action->trigger();
        break;
      }
    }
    menu->close();
  });
  open();
  return found;
}

} // namespace

void TestDatabasePanel::consoleCompletesCtesAndReplacesQuotedIdentifiers() {
  SqlConsoleEdit edit;
  edit.applyTheme(testTheme());
  edit.setFont(QFont("monospace", 24));
  edit.resize(1000, 400);
  edit.show();
  edit.setFocus();
  const QString sql = "SELECT 1;\n\n  WITH recent AS (SELECT 'x' AS \"Total "
                      "Amount\") SELECT r.\"Total A\" FROM recent r";
  edit.setPlainText(sql);
  auto cursor = edit.textCursor();
  cursor.setPosition(sql.lastIndexOf("Total A") + 7);
  edit.setTextCursor(cursor);
  QTest::keyClick(&edit, Qt::Key_Space, Qt::ControlModifier);
  auto *completer = edit.findChild<QCompleter *>();
  QVERIFY(completer);
  QVERIFY(completer->popup()->isVisible());
  QCOMPARE(completer->popup()->font().pointSizeF(), 24.0);
  QVERIFY(completer->popup()->sizeHintForRow(0) >=
          edit.fontMetrics().height() + 10);
  QCOMPARE(completer->completionModel()->rowCount(), 1);
  QTest::keyClick(&edit, Qt::Key_Tab);
  QCOMPARE(edit.toPlainText(),
           QString(sql).replace("r.\"Total A\"", "r.\"Total Amount\""));
  QVERIFY(!completer->popup()->isVisible());

  edit.setPlainText("SELECT cou(*)");
  cursor = edit.textCursor();
  cursor.setPosition(10);
  edit.setTextCursor(cursor);
  QTest::keyClick(&edit, Qt::Key_Space, Qt::ControlModifier);
  QVERIFY(completer->popup()->isVisible());
  QTest::keyClick(&edit, Qt::Key_Tab);
  QCOMPARE(edit.toPlainText(), QString("SELECT count(*)"));

  edit.setPlainText("SELECT 'FROM recent r");
  edit.moveCursor(QTextCursor::End);
  QTest::keyClick(&edit, Qt::Key_Space, Qt::ControlModifier);
  QVERIFY(!completer->popup()->isVisible());
}

void TestDatabasePanel::storageAndContextActionsPreserveResults() {
  PanelFixture fx;
  QVERIFY(fx.init());
  QVERIFY(fx.connectPanel());
  fx.panel->setContentFont(QFont("monospace", 24));
  fx.panel->runSql("CREATE TABLE things(id INTEGER PRIMARY KEY, name TEXT);"
                   "CREATE INDEX things_name ON things(name);"
                   "CREATE VIEW things_view AS SELECT name FROM things;"
                   "INSERT INTO things(name) VALUES ('a'), ('b'), ('c');"
                   "SELECT 'keep this result' AS note;");
  auto *connection = fx.manager->activeConnection();
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() && !connection->schemaLoading() &&
           !connection->statisticsLoading();
  }));
  QCOMPARE(fx.panel->resultTabs()->count(), 1);
  QPointer<QWidget> result = fx.panel->resultTabs()->widget(0);
  auto *tree = fx.panel->tree();
  QVERIFY(!tree->isHeaderHidden());
  QCOMPARE(tree->columnCount(), 2);
  QTreeWidgetItem *table = nullptr;
  for (QTreeWidgetItemIterator it(tree); *it; ++it)
    if ((*it)->text(0) == "things")
      table = *it;
  QVERIFY(table);
  QVERIFY(table->text(1).contains("KiB"));
  tree->scrollToItem(table);
  tree->setCurrentItem(table);
  tree->setFocus();
  qreal menuFont = 0;
  QStringList menuLabels;
  QVERIFY(useContextAction(
      fx.panel.get(),
      [&] { QTest::keyClick(tree, Qt::Key_F10, Qt::ShiftModifier); },
      "Storage and row count", &menuLabels, &menuFont));
  QCOMPARE(menuFont, 24.0);
  QVERIFY(menuLabels.contains("Browse data…"));
  QCOMPARE(fx.panel->outputTabs()->currentWidget(), fx.panel->storage());
  auto *storageTree =
      fx.panel->storage()->findChild<QTreeWidget *>("dbStorageTree");
  QVERIFY(storageTree);
  QCOMPARE(storageTree->font().pointSizeF(), 24.0);
  QVERIFY(storageTree->currentItem());
  QCOMPARE(storageTree->currentItem()->text(0), QString("things"));
  QCOMPARE(storageTree->currentItem()->text(1), QString::fromUtf8("—"));
  auto *count = fx.panel->storage()->findChild<QPushButton *>("dbStorageCount");
  QVERIFY(count && count->isEnabled());
  count->click();
  QVERIFY(!count->isEnabled());
  QVERIFY(waitUntil([&] { return !fx.panel->isRunning(); }));
  QVERIFY(count->isEnabled());
  QCOMPARE(connection->statistics().table("main", "things")->rows, qint64(3));
  QCOMPARE(storageTree->currentItem()->text(1), QString("3"));
  storageTree->sortItems(4, Qt::DescendingOrder);
  auto *database = storageTree->topLevelItem(0);
  QCOMPARE(database->childCount(), 2);
  QCOMPARE(database->child(0)->text(0), QString("things"));
  QCOMPARE(database->child(1)->text(0), QString("things_view (view)"));
  storageTree->sortItems(1, Qt::AscendingOrder);
  QCOMPARE(database->child(0)->text(0), QString("things"));
  storageTree->sortItems(1, Qt::DescendingOrder);
  QCOMPARE(database->child(0)->text(0), QString("things"));
  QCOMPARE(fx.panel->outputTabs()->currentWidget(), fx.panel->storage());
  QCOMPARE(fx.panel->resultTabs()->count(), 1);
  QCOMPARE(fx.panel->resultTabs()->widget(0), result.data());

  auto *actions = fx.panel->findChild<QToolButton *>("dbObjectActions");
  QVERIFY(actions && actions->isEnabled());
  QVERIFY(useContextAction(
      fx.panel.get(), [&] { actions->click(); }, "Copy qualified name"));
  QCOMPARE(QApplication::clipboard()->text(), QString("\"main\".\"things\""));
  QVERIFY(useContextAction(
      fx.panel.get(), [&] { QTest::keyClick(tree, Qt::Key_Menu); },
      "Refresh sizes"));
  QVERIFY(waitUntil([&] { return !connection->statisticsLoading(); }));
  QCOMPARE(tree->currentItem(), table);
  QCOMPARE(fx.panel->resultTabs()->widget(0), result.data());
  table->setExpanded(true);
  QVERIFY(table->childCount() > 0);
  tree->setCurrentItem(table->child(0));
  QVERIFY(useContextAction(
      fx.panel.get(), [&] { actions->click(); }, "Table storage"));
  QCOMPARE(fx.panel->outputTabs()->currentWidget(), fx.panel->storage());
  QCOMPARE(storageTree->currentItem()->text(0), QString("things"));
}

void TestDatabasePanel::panelRunsScriptAndShowsResults() {
  PanelFixture fx;
  QVERIFY(fx.init());
  QVERIFY(fx.connectPanel());
  fx.panel->runSql("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT); "
                   "INSERT INTO t (name) VALUES ('a'), ('b'), ('c'); "
                   "SELECT * FROM t ORDER BY id; SELECT COUNT(*) AS n FROM t;");
  QVERIFY(waitUntil([&]() {
    return fx.panel->resultTabs()->count() == 2 && !fx.panel->isRunning();
  }));
  auto *first = qobject_cast<DbResultView *>(fx.panel->resultTabs()->widget(0));
  auto *second =
      qobject_cast<DbResultView *>(fx.panel->resultTabs()->widget(1));
  QVERIFY(first && second);
  QCOMPARE(first->result().rows.size(), 3);
  QCOMPARE(second->result().rows[0][0].toInt(), 3);
  QVERIFY(fx.panel->outputTabs()->tabText(0).contains("2"));

  first->insightsRequested();
  QCOMPARE(fx.panel->outputTabs()->currentWidget(), fx.panel->insights());
  QCOMPARE(fx.panel->insights()->table()->rowCount(), 2);
}

void TestDatabasePanel::panelRunScriptExecutesCitySetup() {
  PanelFixture fx;
  QVERIFY(fx.init());
  QVERIFY(fx.connectPanel());
  const QString script = QStringLiteral(R"SQL(DROP TABLE IF EXISTS CITY;
CREATE TABLE CITY (
    ID INTEGER PRIMARY KEY,
    NAME VARCHAR(50),
    COUNTRYCODE CHAR(3),
    DISTRICT VARCHAR(30),
    POPULATION INTEGER
);
INSERT INTO CITY VALUES
    (1, 'Oakland', 'USA', 'California', 110000),
    (2, 'Fresno', 'USA', 'California', 130001),
    (3, 'Salinas', 'USA', 'California', 100000),
    (4, 'Concord', 'USA', 'California', 120000),
    (5, 'Albany', 'USA', 'New York', 90000),
    (1661, 'Tokyo', 'JPN', 'Tokyo', 8310000),
    (1662, 'Osaka', 'JPN', 'Osaka', 2600000),
    (6, 'Cairo', 'EGY', 'Cairo', 9000000),
    (7, 'Nairobi', 'KEN', 'Nairobi', 3500000),
    (8, 'Paris', 'FRA', 'Ile-de-France', 2200000),
    (9, 'Sydney', 'AUS', 'New South Wales', 5000000);)SQL");
  fx.panel->setConsoleText(script);
  fx.panel->console()->moveCursor(QTextCursor::End);
  auto *toolbar = fx.panel->findChild<QToolBar *>("dbToolbar");
  auto *statementAction = fx.panel->findChild<QAction *>("dbRunStatement");
  auto *scriptAction = fx.panel->findChild<QAction *>("dbRunScript");
  QVERIFY(toolbar && statementAction && scriptAction);
  auto *statementButton =
      qobject_cast<QToolButton *>(toolbar->widgetForAction(statementAction));
  auto *scriptButton =
      qobject_cast<QToolButton *>(toolbar->widgetForAction(scriptAction));
  QVERIFY(statementButton && scriptButton);

  // Running the statement at the caret reproduces the missing-table error.
  QTest::mouseClick(statementButton, Qt::LeftButton);
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() &&
           fx.panel->messages()->toPlainText().contains("no such table: CITY");
  }));
  QVERIFY(!fx.manager->activeConnection()->schema().findByName("CITY"));

  // Run script must include setup statements even when only INSERT is selected.
  QTextCursor cursor = fx.panel->console()->textCursor();
  cursor.setPosition(script.indexOf("INSERT INTO"));
  cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  fx.panel->console()->setTextCursor(cursor);
  QCOMPARE(statementButton->text(), QString("Run selection"));
  QTest::mouseClick(scriptButton, Qt::LeftButton);
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() &&
           fx.manager->activeConnection()->schema().findByName("CITY");
  }));
  fx.panel->runSql(
      "SELECT COUNT(*) FROM CITY; SELECT NAME FROM CITY WHERE ID = 1661;");
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() && fx.panel->resultTabs()->count() == 2;
  }));
  auto *count = qobject_cast<DbResultView *>(fx.panel->resultTabs()->widget(0));
  auto *city = qobject_cast<DbResultView *>(fx.panel->resultTabs()->widget(1));
  QVERIFY(count && city);
  QCOMPARE(count->result().rows[0][0].toInt(), 11);
  QCOMPARE(city->result().rows[0][0].toString(), QString("Tokyo"));
}

void TestDatabasePanel::panelSurfacesErrorsInMessages() {
  PanelFixture fx;
  QVERIFY(fx.init());
  QVERIFY(fx.connectPanel());
  fx.panel->runSql("SELECT * FROM does_not_exist");
  QVERIFY(waitUntil([&]() {
    return !fx.panel->isRunning() &&
           fx.panel->messages()->toPlainText().contains("does_not_exist");
  }));
  QCOMPARE(fx.panel->resultTabs()->count(), 0);
  QCOMPARE(fx.panel->outputTabs()->currentWidget(), fx.panel->messages());
  QVERIFY(fx.panel->outputTabs()->tabText(1).contains("error"));
}

void TestDatabasePanel::panelBuildsSchemaTreeAndFilters() {
  PanelFixture fx;
  QVERIFY(fx.init());
  QVERIFY(fx.connectPanel());
  fx.panel->runSql(
      "CREATE TABLE customers (id INTEGER PRIMARY KEY, email TEXT); "
      "CREATE TABLE orders (id INTEGER PRIMARY KEY, customer_id INT, total "
      "REAL);");
  QVERIFY(waitUntil([&]() {
    const QStringList t = treeTexts(fx.panel->tree());
    return t.contains("customers") && t.contains("orders");
  }));
  QVERIFY(treeTexts(fx.panel->tree()).contains("Test SQLite"));

  auto *filter = fx.panel->findChild<QLineEdit *>("dbFilter");
  QVERIFY(filter);
  filter->setText("email");
  QVERIFY(waitUntil(
      [&]() { return !treeTexts(fx.panel->tree()).contains("orders"); }));
  QVERIFY(treeTexts(fx.panel->tree()).contains("customers"));
  filter->setText("nothing matches this");
  QVERIFY(waitUntil(
      [&]() { return !treeTexts(fx.panel->tree()).contains("customers"); }));
  filter->clear();
  QVERIFY(waitUntil(
      [&]() { return treeTexts(fx.panel->tree()).contains("orders"); }));
}

void TestDatabasePanel::panelRunsFromEditorWithConnectionDirective() {
  PanelFixture fx;
  QVERIFY(fx.init());

  const QString otherFile = fx.dir.filePath("other.db");
  QFile f(otherFile);
  QVERIFY(f.open(QIODevice::WriteOnly));
  f.close();
  DbConnectionProfile other;
  other.name = "Other DB";
  other.engine = DbEngine::Sqlite;
  other.filePath = otherFile;
  other.confirmDestructive = false;
  const QString otherId = fx.manager->saveProfile(other);
  QCOMPARE(fx.manager->activeId(), fx.sqliteId);

  const QString script =
      "-- connection: Other DB\nSELECT 'from other' AS src;\nSELECT 2;";
  QCOMPARE(fx.panel->connectionForScript(script),
           fx.manager->connection(otherId));
  QCOMPARE(fx.panel->connectionForScript("SELECT 1"),
           fx.manager->connection(fx.sqliteId));

  fx.panel->runFromEditor(script, script.indexOf("'from"), QString(), false);
  QVERIFY(waitUntil([&]() {
    return fx.panel->resultTabs()->count() == 1 && !fx.panel->isRunning();
  }));
  auto *view = qobject_cast<DbResultView *>(fx.panel->resultTabs()->widget(0));
  QCOMPARE(view->result().rows[0][0].toString(), QString("from other"));
  QVERIFY(fx.manager->connection(otherId)->isConnected());
  QVERIFY(!fx.manager->connection(fx.sqliteId)->isConnected());

  fx.panel->runFromEditor(script, 0, QString(), true);
  QVERIFY(waitUntil([&]() {
    return fx.panel->resultTabs()->count() == 2 && !fx.panel->isRunning();
  }));
  fx.panel->runFromEditor(script, 0, "SELECT 42 AS answer", false);
  QVERIFY(waitUntil([&]() {
    if (fx.panel->isRunning() || fx.panel->resultTabs()->count() != 1) {
      return false;
    }
    auto *v = qobject_cast<DbResultView *>(fx.panel->resultTabs()->widget(0));
    return v && v->result().columns.value(0).name == "answer";
  }));
}

void TestDatabasePanel::panelRecordsHistory() {
  PanelFixture fx;
  QVERIFY(fx.init());
  QVERIFY(fx.connectPanel());
  fx.panel->runSql("SELECT 1 AS one");
  QVERIFY(waitUntil([&]() {
    return !fx.panel->isRunning() && !fx.manager->history().entries().isEmpty();
  }));
  const auto entry = fx.manager->history().entries().first();
  QCOMPARE(entry.sql, QString("SELECT 1 AS one"));
  QCOMPARE(entry.connection, QString("Test SQLite"));
  QVERIFY(entry.ok);
  QCOMPARE(entry.rows, qint64(1));
  fx.panel->runSql("SELECT nope FROM nowhere");
  QVERIFY(waitUntil([&]() {
    return !fx.panel->isRunning() &&
           fx.manager->history().entries().size() == 2;
  }));
  QVERIFY(!fx.manager->history().entries().first().ok);
  QVERIFY(!fx.manager->history().entries().first().error.isEmpty());
}

void TestDatabasePanel::panelReadOnlyConnectionRefusesWrites() {
  PanelFixture fx;
  QVERIFY(fx.init(true));
  QVERIFY(fx.connectPanel());
  fx.panel->runSql("CREATE TABLE nope (a INT)");
  QVERIFY(waitUntil([&]() {
    return fx.panel->messages()->toPlainText().contains("read-only");
  }));
  fx.panel->runSql("SELECT 1");
  QVERIFY(waitUntil([&]() { return fx.panel->resultTabs()->count() == 1; }));
}

void TestDatabasePanel::resultViewSavesBlobBytes() {
  QTemporaryDir dir;
  DbResultView view;
  view.applyTheme(testTheme());
  const QByteArray bytes = QByteArray::fromHex("000102ff000a");
  view.setResult(makeResult({"payload"}, {{bytes}}), DbEngine::Sqlite, "items");
  QCOMPARE(
      view.model()->index(0, 0).data(DbResultModel::RawValueRole).toByteArray(),
      bytes);
  QVERIFY(view.model()->index(0, 0).data().toString().contains("BLOB"));
  QVERIFY(
      view.saveCellValue(view.proxy()->index(0, 0), dir.filePath("value.bin")));
  QFile file(dir.filePath("value.bin"));
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), bytes);
  view.table()->selectAll();
  QVERIFY(view.selectionAs(ResultExporter::Format::SqlInsert, true)
              .contains("X'000102ff000a'"));
  QVERIFY(view.selectionAs(ResultExporter::Format::Json, true)
              .contains("0x000102ff000a"));
  QVERIFY(QMetaObject::invokeMethod(
      &view, "showCellValue", Q_ARG(QModelIndex, view.proxy()->index(0, 0))));
  auto *tabs = view.findChild<QTabWidget *>();
  QVERIFY(tabs);
  QCOMPARE(tabs->count(), 2);
}

void TestDatabasePanel::panelBrowsesPagesAndFiltersWholeTable() {
  PanelFixture fx;
  QVERIFY(fx.init());
  QVERIFY(fx.connectPanel());
  fx.panel->runSql("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT); "
                   "WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM "
                   "n WHERE x<501) "
                   "INSERT INTO people SELECT x, CASE x WHEN 501 THEN "
                   "'last_row_%' ELSE 'person_'||x END FROM n;");
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() &&
           !fx.manager->activeConnection()->schemaLoading();
  }));
  QTreeWidgetItem *table = nullptr;
  for (QTreeWidgetItemIterator it(fx.panel->tree()); *it; ++it)
    if ((*it)->text(0) == "people")
      table = *it;
  QVERIFY(table);
  fx.panel->tree()->itemDoubleClicked(table, 0);
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() && fx.panel->resultTabs()->count() == 1;
  }));
  auto *view = qobject_cast<DbResultView *>(fx.panel->resultTabs()->widget(0));
  QVERIFY(view && view->isTableBrowser());
  QCOMPARE(view->model()->rowCount(), 200);
  auto *next = view->findChild<QToolButton *>("dbNextPage");
  auto *previous = view->findChild<QToolButton *>("dbPreviousPage");
  QVERIFY(next->isEnabled());
  QVERIFY(!previous->isEnabled());
  next->click();
  QVERIFY(waitUntil([&] { return !fx.panel->isRunning(); }));
  QCOMPARE(view->result().rows[0][0].toInt(), 201);
  next->click();
  QVERIFY(waitUntil([&] { return !fx.panel->isRunning(); }));
  QCOMPARE(view->result().rows.size(), 101);
  QCOMPARE(view->result().rows[0][0].toInt(), 401);
  QVERIFY(!next->isEnabled());
  auto *filter = view->findChild<QLineEdit *>("dbResultFilter");
  filter->setText("last_row_%");
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() && view->model()->rowCount() == 1;
  }));
  QCOMPARE(view->result().rows[0][0].toInt(), 501);
  filter->setText("' OR 1=1 --");
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() && view->model()->rowCount() == 0;
  }));
  filter->clear();
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() && view->model()->rowCount() == 200;
  }));
  auto *header = view->table()->horizontalHeader();
  const QPoint columnCenter(header->sectionViewportPosition(0) +
                                header->sectionSize(0) / 2,
                            header->height() / 2);
  QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
                    columnCenter);
  const auto firstOrder = header->sortIndicatorOrder();
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() &&
           view->result().rows[0][0].toInt() ==
               (firstOrder == Qt::AscendingOrder ? 1 : 501);
  }));
  QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
                    columnCenter);
  QVERIFY(header->sortIndicatorOrder() != firstOrder);
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() &&
           view->result().rows[0][0].toInt() ==
               (header->sortIndicatorOrder() == Qt::AscendingOrder ? 1 : 501);
  }));
  if (header->sortIndicatorOrder() != Qt::DescendingOrder) {
    QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
                      columnCenter);
    QVERIFY(waitUntil([&] {
      return !fx.panel->isRunning() && view->result().rows[0][0].toInt() == 501;
    }));
  }
  QCOMPARE(view->result().rows.last()[0].toInt(), 302);
  // A smaller session cap must not skip rows when moving to the next page.
  fx.manager->activeConnection()->setMaxRows(50);
  view->reloadPage();
  QVERIFY(waitUntil([&] { return !fx.panel->isRunning(); }));
  QCOMPARE(view->result().rows.size(), 50);
  next->click();
  QVERIFY(waitUntil([&] { return !fx.panel->isRunning(); }));
  QCOMPARE(view->result().rows[0][0].toInt(), 451);
}

void TestDatabasePanel::panelInspectsSqliteStructure() {
  PanelFixture fx;
  QVERIFY(fx.init());
  QVERIFY(fx.connectPanel());
  fx.panel->runSql("CREATE TABLE things (id INTEGER PRIMARY KEY, n TEXT "
                   "DEFAULT 'x', upper_n TEXT GENERATED ALWAYS AS (upper(n)) "
                   "STORED, parent INTEGER REFERENCES things(id));"
                   "CREATE INDEX things_n ON things(n);"
                   "CREATE TRIGGER things_trigger AFTER INSERT ON things BEGIN "
                   "SELECT 1; END;");
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() &&
           !fx.manager->activeConnection()->schemaLoading();
  }));
  QTreeWidgetItem *table = nullptr;
  for (QTreeWidgetItemIterator it(fx.panel->tree()); *it; ++it)
    if ((*it)->text(0) == "things")
      table = *it;
  QVERIFY(table);
  fx.panel->tree()->scrollToItem(table);
  const QPoint pos = fx.panel->tree()->visualItemRect(table).center();
  QTimer::singleShot(0, fx.panel.get(), [] {
    auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    if (!menu)
      return;
    for (auto *action : menu->actions()) {
      if (action->text().startsWith("Inspect structure")) {
        action->trigger();
        menu->close();
        return;
      }
    }
    menu->close();
  });
  QVERIFY(QMetaObject::invokeMethod(fx.panel.get(), "onTreeContextMenu",
                                    Q_ARG(QPoint, pos)));
  QVERIFY(waitUntil([&] {
    return !fx.panel->isRunning() && fx.panel->resultTabs()->count() == 5;
  }));
  QVERIFY(fx.panel->resultTabs()->tabText(0).startsWith("Columns"));
  QVERIFY(fx.panel->resultTabs()->tabText(1).startsWith("Indexes"));
  QVERIFY(fx.panel->resultTabs()->tabText(2).startsWith("Foreign keys"));
  QVERIFY(fx.panel->resultTabs()->tabText(3).startsWith("Triggers"));
  QVERIFY(fx.panel->resultTabs()->tabText(4).startsWith("Creation SQL"));
  QCOMPARE(qobject_cast<DbResultView *>(fx.panel->resultTabs()->widget(0))
               ->result()
               .rows.size(),
           4);
}

void TestDatabasePanel::panelOpensSqliteFileOnce() {
  PanelFixture fx;
  QVERIFY(fx.init());
  const QString path = fx.dir.filePath("second.db");
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();
  fx.panel->openSqliteFile(path);
  QVERIFY(
      waitUntil([&] { return fx.manager->activeConnection()->isConnected(); }));
  const QString id = fx.manager->activeId();
  fx.panel->openSqliteFile(path);
  QCOMPARE(fx.manager->activeId(), id);
  QCOMPARE(fx.manager->profiles().size(), 2);
}

QTEST_MAIN(TestDatabasePanel)
#include "test_databasepanel.moc"
