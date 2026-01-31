#include <QtTest/QtTest>
#include <QTreeWidget>
#include "ui/panels/todopanel.h"

class TestTodoPanel : public QObject {
    Q_OBJECT

private slots:
    void testCountsAndTree();
    void testFilterAndSearch();
};

void TestTodoPanel::testCountsAndTree()
{
    TodoPanel panel;
    const QString content = "TODO: First item\n"
                            "Line 2\n"
                            "// FIXME: Fix this\n"
                            "NOTE: Remember to update docs\n";

    panel.setTodos("/tmp/sample.cpp", content);

    QCOMPARE(panel.totalCount(), 3);
    QCOMPARE(panel.todoCount(), 1);
    QCOMPARE(panel.fixmeCount(), 1);
    QCOMPARE(panel.noteCount(), 1);

    auto tree = panel.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);
    QCOMPARE(tree->topLevelItemCount(), 1);
    QCOMPARE(tree->topLevelItem(0)->childCount(), 3);
    QCOMPARE(tree->topLevelItem(0)->child(0)->text(1), QString("[1]"));
    QCOMPARE(tree->topLevelItem(0)->child(1)->text(1), QString("[3]"));
    QCOMPARE(tree->topLevelItem(0)->child(2)->text(1), QString("[4]"));
}

void TestTodoPanel::testFilterAndSearch()
{
    TodoPanel panel;
    const QString content = "TODO: Refactor\n"
                            "FIXME: Crash\n"
                            "NOTE: Review later\n";
    panel.setTodos("/tmp/sample.cpp", content);

    auto filter = panel.findChild<QComboBox*>();
    auto tree = panel.findChild<QTreeWidget*>();
    auto search = panel.findChild<QLineEdit*>();
    QVERIFY(filter != nullptr);
    QVERIFY(tree != nullptr);
    QVERIFY(search != nullptr);

    filter->setCurrentIndex(2);
    QCOMPARE(tree->topLevelItem(0)->childCount(), 1);

    filter->setCurrentIndex(0);
    search->setText("review");
    QCOMPARE(tree->topLevelItem(0)->childCount(), 1);

    search->setText("missing");
    QCOMPARE(tree->topLevelItemCount(), 0);
}

QTEST_MAIN(TestTodoPanel)
#include "test_todopanel.moc"
