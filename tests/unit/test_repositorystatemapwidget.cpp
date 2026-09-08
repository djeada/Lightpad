#include "ui/widgets/repositorystatemapwidget.h"
#include <QApplication>
#include <QSignalSpy>
#include <QToolButton>
#include <QtTest/QtTest>

using Layer = RepositoryStateMapWidget::Layer;
using CollapseMode = RepositoryStateMapWidget::CollapseMode;

class TestRepositoryStateMapWidget : public QObject {
  Q_OBJECT

private slots:
  void testNoRepositoryShowsNoChips();
  void testLayerChipsExist();
  void testChipsSpellOutCounts();
  void testChipsAreKeyboardReachable();
  void testDetachedHeadIsUnmistakable();
  void testUpstreamChipShowsDivergence();
  void testNoUpstreamChip();
  void testStashChipOnlyWhenStashed();
  void testConflictChipOnlyWhenConflicted();
  void testOperationChipVisibility();
  void testActivationSignals();
  void testConflictChipActivatesWorkingTree();
  void testCollapseModes();
  void testCollapsedButtonCarriesSummary();

private:
  static GitRepositoryState busyState();
  static QToolButton *chip(const RepositoryStateMapWidget &map,
                           const QString &name);
};

GitRepositoryState TestRepositoryStateMapWidget::busyState() {
  GitRepositoryState state;
  state.valid = true;
  state.branch = "feature/login";
  state.headShortHash = "a1b2c3d";
  state.headSubject = "Add login redirect";
  state.upstream = "origin/feature/login";
  state.hasUpstream = true;
  state.ahead = 2;
  state.behind = 1;
  state.stagedCount = 2;
  state.modifiedCount = 3;
  state.untrackedCount = 1;
  return state;
}

QToolButton *
TestRepositoryStateMapWidget::chip(const RepositoryStateMapWidget &map,
                                   const QString &name) {
  return map.findChild<QToolButton *>(name);
}

void TestRepositoryStateMapWidget::testNoRepositoryShowsNoChips() {
  RepositoryStateMapWidget map;
  map.setState(GitRepositoryState());

  QVERIFY(!chip(map, "stateMapWorkingTree"));
  QVERIFY(!chip(map, "stateMapBranch"));
  QVERIFY(!chip(map, "stateMapOperation")->isVisible());
}

void TestRepositoryStateMapWidget::testLayerChipsExist() {
  RepositoryStateMapWidget map;
  map.setState(busyState());

  QVERIFY(chip(map, "stateMapWorkingTree"));
  QVERIFY(chip(map, "stateMapIndex"));
  QVERIFY(chip(map, "stateMapHead"));
  QVERIFY(chip(map, "stateMapBranch"));
  QVERIFY(chip(map, "stateMapUpstream"));
}

void TestRepositoryStateMapWidget::testChipsSpellOutCounts() {
  RepositoryStateMapWidget map;
  map.setState(busyState());

  QVERIFY2(chip(map, "stateMapWorkingTree")->text().contains("4"),
           qPrintable(chip(map, "stateMapWorkingTree")->text()));
  QVERIFY2(chip(map, "stateMapIndex")->text().contains("2"),
           qPrintable(chip(map, "stateMapIndex")->text()));
  QVERIFY(chip(map, "stateMapHead")->text().contains("a1b2c3d"));
  QVERIFY(chip(map, "stateMapBranch")->text().contains("feature/login"));
}

void TestRepositoryStateMapWidget::testChipsAreKeyboardReachable() {
  RepositoryStateMapWidget map;
  map.setState(busyState());

  const QStringList names{"stateMapWorkingTree", "stateMapIndex",
                          "stateMapHead", "stateMapBranch", "stateMapUpstream"};
  for (const QString &name : names) {
    QToolButton *button = chip(map, name);
    QVERIFY2(button, qPrintable(name));
    QCOMPARE(button->focusPolicy(), Qt::StrongFocus);
    QVERIFY2(!button->accessibleName().isEmpty(), qPrintable(name));
    QVERIFY2(!button->toolTip().isEmpty(), qPrintable(name));
  }
}

void TestRepositoryStateMapWidget::testDetachedHeadIsUnmistakable() {
  GitRepositoryState state;
  state.valid = true;
  state.detachedHead = true;
  state.headShortHash = "a1b2c3d";

  RepositoryStateMapWidget map;
  map.setState(state);

  const QString text = chip(map, "stateMapBranch")->text();
  QVERIFY2(text.contains("DETACHED"), qPrintable(text));
  QVERIFY(chip(map, "stateMapBranch")->accessibleName().contains("Detached"));
}

void TestRepositoryStateMapWidget::testUpstreamChipShowsDivergence() {
  RepositoryStateMapWidget map;
  map.setState(busyState());

  const QString text = chip(map, "stateMapUpstream")->text();
  QVERIFY2(text.contains(QString::fromUtf8("↑2")), qPrintable(text));
  QVERIFY2(text.contains(QString::fromUtf8("↓1")), qPrintable(text));
}

void TestRepositoryStateMapWidget::testNoUpstreamChip() {
  GitRepositoryState state;
  state.valid = true;
  state.branch = "wip";

  RepositoryStateMapWidget map;
  map.setState(state);

  QVERIFY(chip(map, "stateMapUpstream")->text().contains("No upstream"));
}

void TestRepositoryStateMapWidget::testStashChipOnlyWhenStashed() {
  RepositoryStateMapWidget map;
  map.setState(busyState());
  QVERIFY(!chip(map, "stateMapStash"));

  GitRepositoryState stashed = busyState();
  stashed.stashCount = 3;
  map.setState(stashed);
  QVERIFY(chip(map, "stateMapStash"));
  QVERIFY(chip(map, "stateMapStash")->text().contains("3"));
}

void TestRepositoryStateMapWidget::testConflictChipOnlyWhenConflicted() {
  RepositoryStateMapWidget map;
  map.setState(busyState());
  QVERIFY(!chip(map, "stateMapConflicts"));

  GitRepositoryState conflicted = busyState();
  conflicted.conflictedCount = 2;
  map.setState(conflicted);
  QVERIFY(chip(map, "stateMapConflicts"));
  QVERIFY(chip(map, "stateMapConflicts")->text().contains("2"));
}

void TestRepositoryStateMapWidget::testOperationChipVisibility() {
  RepositoryStateMapWidget map;
  map.setCollapseMode(CollapseMode::Expanded);
  map.show();

  map.setState(busyState());
  QVERIFY(!chip(map, "stateMapOperation")->isVisible());

  GitRepositoryState rebasing = busyState();
  rebasing.operation = GitOperation::Rebase;
  rebasing.operationDetail = "3 of 7";
  map.setState(rebasing);

  QToolButton *operation = chip(map, "stateMapOperation");
  QVERIFY(operation->isVisible());
  QVERIFY2(operation->text().contains("Rebase"), qPrintable(operation->text()));
  QVERIFY2(operation->text().contains("3 of 7"), qPrintable(operation->text()));
  QVERIFY(operation->toolTip().contains("git rebase --continue"));
}

void TestRepositoryStateMapWidget::testActivationSignals() {
  RepositoryStateMapWidget map;
  GitRepositoryState state = busyState();
  state.stashCount = 1;
  state.operation = GitOperation::Merge;
  map.setState(state);

  QSignalSpy spy(&map, &RepositoryStateMapWidget::layerActivated);

  const QVector<QPair<QString, Layer>> expected{
      {"stateMapWorkingTree", Layer::WorkingTree},
      {"stateMapIndex", Layer::Index},
      {"stateMapHead", Layer::Head},
      {"stateMapBranch", Layer::Branch},
      {"stateMapUpstream", Layer::Upstream},
      {"stateMapStash", Layer::Stash},
      {"stateMapOperation", Layer::Operation},
  };

  for (const auto &entry : expected) {
    spy.clear();
    chip(map, entry.first)->click();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).value<Layer>(), entry.second);
  }
}

void TestRepositoryStateMapWidget::testConflictChipActivatesWorkingTree() {
  GitRepositoryState state = busyState();
  state.conflictedCount = 1;

  RepositoryStateMapWidget map;
  map.setState(state);

  QSignalSpy spy(&map, &RepositoryStateMapWidget::layerActivated);
  chip(map, "stateMapConflicts")->click();
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.takeFirst().at(0).value<Layer>(), Layer::WorkingTree);
}

void TestRepositoryStateMapWidget::testCollapseModes() {
  RepositoryStateMapWidget map;
  map.setState(busyState());
  map.resize(600, 120);
  map.show();

  map.setCollapseMode(CollapseMode::Expanded);
  QVERIFY(!map.isCollapsed());
  QVERIFY(chip(map, "stateMapWorkingTree")->isVisible());
  QVERIFY(!chip(map, "stateMapCollapsed")->isVisible());

  map.setCollapseMode(CollapseMode::Collapsed);
  QVERIFY(map.isCollapsed());
  QVERIFY(!chip(map, "stateMapWorkingTree")->isVisible());
  QVERIFY(chip(map, "stateMapCollapsed")->isVisible());

  map.setCollapseMode(CollapseMode::Auto);
  map.resize(600, 120);
  QVERIFY(!map.isCollapsed());
  map.resize(140, 120);
  QVERIFY(map.isCollapsed());
}

void TestRepositoryStateMapWidget::testCollapsedButtonCarriesSummary() {
  RepositoryStateMapWidget map;
  map.setState(busyState());
  map.resize(600, 80);
  map.show();
  map.setCollapseMode(CollapseMode::Collapsed);

  QToolButton *collapsed = chip(map, "stateMapCollapsed");
  QVERIFY(!collapsed->text().isEmpty());

  QVERIFY2(collapsed->text().startsWith("feature/login"),
           qPrintable(collapsed->text()));
  QCOMPARE(collapsed->toolTip(), gitRepositoryStateSummary(map.state()));
  QVERIFY(!collapsed->accessibleDescription().isEmpty());
  QCOMPARE(collapsed->focusPolicy(), Qt::StrongFocus);
}

QTEST_MAIN(TestRepositoryStateMapWidget)
#include "test_repositorystatemapwidget.moc"
