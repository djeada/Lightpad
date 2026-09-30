#include "database/databasemanager.h"
#include "settings/theme.h"
#include "ui/panels/databasepanel.h"
#include "ui/panels/dbinsightsview.h"
#include "ui/panels/dbresultview.h"
#include "ui/panels/sqlconsoleedit.h"

#include <QFile>
#include <QSqlDatabase>
#include <QTemporaryDir>
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

  void insightsListsEveryColumn();

  void consoleFindsStatementUnderCaret();
  void consoleRunShortcuts();
  void consoleHistoryShortcut();

  void panelRunsScriptAndShowsResults();
  void panelSurfacesErrorsInMessages();
  void panelBuildsSchemaTreeAndFilters();
  void panelRunsFromEditorWithConnectionDirective();
  void panelRecordsHistory();
  void panelReadOnlyConnectionRefusesWrites();
};

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

} // namespace

void TestDatabasePanel::panelRunsScriptAndShowsResults() {
  if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
    QSKIP("Qt SQLite driver not installed");
  }
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

void TestDatabasePanel::panelSurfacesErrorsInMessages() {
  if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
    QSKIP("Qt SQLite driver not installed");
  }
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
  if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
    QSKIP("Qt SQLite driver not installed");
  }
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
  if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
    QSKIP("Qt SQLite driver not installed");
  }
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
  if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
    QSKIP("Qt SQLite driver not installed");
  }
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
  if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
    QSKIP("Qt SQLite driver not installed");
  }
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

QTEST_MAIN(TestDatabasePanel)
#include "test_databasepanel.moc"
