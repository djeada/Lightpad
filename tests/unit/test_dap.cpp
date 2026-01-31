#include <QtTest/QtTest>
#include <QSignalSpy>

#include "dap/dapclient.h"
#include "dap/breakpointmanager.h"
#include "dap/debugadapterregistry.h"

/**
 * @brief Unit tests for DAP (Debug Adapter Protocol) components
 * 
 * Tests the DAP client infrastructure, breakpoint manager,
 * and debug adapter registry.
 */
class TestDap : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // DapClient tests
    void testDapClientInitialState();
    void testDapClientStateEnum();
    
    // BreakpointManager tests
    void testBreakpointManagerSingleton();
    void testAddBreakpoint();
    void testRemoveBreakpoint();
    void testToggleBreakpoint();
    void testBreakpointCondition();
    void testBreakpointLogpoint();
    void testBreakpointPersistence();
    void testClearBreakpoints();
    
    // DebugAdapterRegistry tests
    void testRegistrySingleton();
    void testBuiltinAdapters();
    void testAdapterLookupByFile();
    void testAdapterLookupByLanguage();
    
    // Data structures tests
    void testDapBreakpointFromJson();
    void testDapStackFrameFromJson();
    void testDapVariableFromJson();
    void testDapStoppedEventFromJson();
    
private:
    void cleanupBreakpoints();
};

void TestDap::initTestCase()
{
    // Clean up any existing breakpoints
    cleanupBreakpoints();
}

void TestDap::cleanupTestCase()
{
    cleanupBreakpoints();
}

void TestDap::cleanupBreakpoints()
{
    BreakpointManager::instance().clearAll();
}

// =============================================================================
// DapClient Tests
// =============================================================================

void TestDap::testDapClientInitialState()
{
    DapClient client;
    
    QCOMPARE(client.state(), DapClient::State::Disconnected);
    QVERIFY(!client.isReady());
    QVERIFY(!client.isDebugging());
    QCOMPARE(client.currentThreadId(), 0);
}

void TestDap::testDapClientStateEnum()
{
    // Test that all states are distinct
    QVERIFY(DapClient::State::Disconnected != DapClient::State::Connecting);
    QVERIFY(DapClient::State::Ready != DapClient::State::Running);
    QVERIFY(DapClient::State::Running != DapClient::State::Stopped);
    QVERIFY(DapClient::State::Stopped != DapClient::State::Terminated);
    QVERIFY(DapClient::State::Error != DapClient::State::Ready);
}

// =============================================================================
// BreakpointManager Tests
// =============================================================================

void TestDap::testBreakpointManagerSingleton()
{
    BreakpointManager& bm1 = BreakpointManager::instance();
    BreakpointManager& bm2 = BreakpointManager::instance();
    
    QCOMPARE(&bm1, &bm2);
}

void TestDap::testAddBreakpoint()
{
    cleanupBreakpoints();
    
    BreakpointManager& bm = BreakpointManager::instance();
    QSignalSpy addedSpy(&bm, &BreakpointManager::breakpointAdded);
    
    Breakpoint bp;
    bp.filePath = "/test/file.cpp";
    bp.line = 42;
    bp.enabled = true;
    
    int id = bm.addBreakpoint(bp);
    
    QVERIFY(id > 0);
    QCOMPARE(addedSpy.count(), 1);
    
    Breakpoint retrieved = bm.breakpoint(id);
    QCOMPARE(retrieved.filePath, bp.filePath);
    QCOMPARE(retrieved.line, bp.line);
    QCOMPARE(retrieved.enabled, bp.enabled);
    
    QVERIFY(bm.hasBreakpoint("/test/file.cpp", 42));
    QVERIFY(!bm.hasBreakpoint("/test/file.cpp", 43));
}

void TestDap::testRemoveBreakpoint()
{
    cleanupBreakpoints();
    
    BreakpointManager& bm = BreakpointManager::instance();
    
    Breakpoint bp;
    bp.filePath = "/test/remove.cpp";
    bp.line = 10;
    
    int id = bm.addBreakpoint(bp);
    QVERIFY(bm.hasBreakpoint("/test/remove.cpp", 10));
    
    QSignalSpy removedSpy(&bm, &BreakpointManager::breakpointRemoved);
    bm.removeBreakpoint(id);
    
    QCOMPARE(removedSpy.count(), 1);
    QVERIFY(!bm.hasBreakpoint("/test/remove.cpp", 10));
    
    // Removing non-existent breakpoint should be safe
    bm.removeBreakpoint(99999);  // Should not crash
}

void TestDap::testToggleBreakpoint()
{
    cleanupBreakpoints();
    
    BreakpointManager& bm = BreakpointManager::instance();
    
    // First toggle - should add
    QVERIFY(!bm.hasBreakpoint("/test/toggle.cpp", 5));
    bm.toggleBreakpoint("/test/toggle.cpp", 5);
    QVERIFY(bm.hasBreakpoint("/test/toggle.cpp", 5));
    
    // Second toggle - should remove
    bm.toggleBreakpoint("/test/toggle.cpp", 5);
    QVERIFY(!bm.hasBreakpoint("/test/toggle.cpp", 5));
}

void TestDap::testBreakpointCondition()
{
    cleanupBreakpoints();
    
    BreakpointManager& bm = BreakpointManager::instance();
    
    Breakpoint bp;
    bp.filePath = "/test/condition.cpp";
    bp.line = 20;
    
    int id = bm.addBreakpoint(bp);
    
    QSignalSpy changedSpy(&bm, &BreakpointManager::breakpointChanged);
    bm.setCondition(id, "x > 10");
    
    QCOMPARE(changedSpy.count(), 1);
    
    Breakpoint updated = bm.breakpoint(id);
    QCOMPARE(updated.condition, QString("x > 10"));
    QVERIFY(!updated.isLogpoint);
}

void TestDap::testBreakpointLogpoint()
{
    cleanupBreakpoints();
    
    BreakpointManager& bm = BreakpointManager::instance();
    
    Breakpoint bp;
    bp.filePath = "/test/logpoint.cpp";
    bp.line = 30;
    
    int id = bm.addBreakpoint(bp);
    bm.setLogMessage(id, "Value is {x}");
    
    Breakpoint updated = bm.breakpoint(id);
    QCOMPARE(updated.logMessage, QString("Value is {x}"));
    QVERIFY(updated.isLogpoint);
    
    // Clear log message
    bm.setLogMessage(id, "");
    updated = bm.breakpoint(id);
    QVERIFY(!updated.isLogpoint);
}

void TestDap::testBreakpointPersistence()
{
    cleanupBreakpoints();
    
    BreakpointManager& bm = BreakpointManager::instance();
    
    // Add some breakpoints
    Breakpoint bp1;
    bp1.filePath = "/test/persist1.cpp";
    bp1.line = 10;
    bp1.condition = "a == b";
    bm.addBreakpoint(bp1);
    
    Breakpoint bp2;
    bp2.filePath = "/test/persist2.cpp";
    bp2.line = 20;
    bp2.logMessage = "debug: {value}";
    bp2.isLogpoint = true;
    bm.addBreakpoint(bp2);
    
    // Save to JSON
    QJsonObject json = bm.saveToJson();
    
    // Clear and reload
    bm.clearAll();
    QCOMPARE(bm.allBreakpoints().count(), 0);
    
    bm.loadFromJson(json);
    
    // Verify restored
    QCOMPARE(bm.allBreakpoints().count(), 2);
    
    QList<Breakpoint> restored = bm.breakpointsForFile("/test/persist1.cpp");
    QCOMPARE(restored.count(), 1);
    QCOMPARE(restored.first().condition, QString("a == b"));
}

void TestDap::testClearBreakpoints()
{
    cleanupBreakpoints();
    
    BreakpointManager& bm = BreakpointManager::instance();
    
    // Add breakpoints to multiple files
    Breakpoint bp1;
    bp1.filePath = "/test/clear1.cpp";
    bp1.line = 1;
    bm.addBreakpoint(bp1);
    
    Breakpoint bp2;
    bp2.filePath = "/test/clear1.cpp";
    bp2.line = 2;
    bm.addBreakpoint(bp2);
    
    Breakpoint bp3;
    bp3.filePath = "/test/clear2.cpp";
    bp3.line = 1;
    bm.addBreakpoint(bp3);
    
    QCOMPARE(bm.allBreakpoints().count(), 3);
    
    // Clear single file
    bm.clearFile("/test/clear1.cpp");
    QCOMPARE(bm.allBreakpoints().count(), 1);
    QCOMPARE(bm.breakpointsForFile("/test/clear1.cpp").count(), 0);
    QCOMPARE(bm.breakpointsForFile("/test/clear2.cpp").count(), 1);
    
    // Clear all
    QSignalSpy clearedSpy(&bm, &BreakpointManager::allBreakpointsCleared);
    bm.clearAll();
    QCOMPARE(clearedSpy.count(), 1);
    QCOMPARE(bm.allBreakpoints().count(), 0);
}

// =============================================================================
// DebugAdapterRegistry Tests
// =============================================================================

void TestDap::testRegistrySingleton()
{
    DebugAdapterRegistry& reg1 = DebugAdapterRegistry::instance();
    DebugAdapterRegistry& reg2 = DebugAdapterRegistry::instance();
    
    QCOMPARE(&reg1, &reg2);
}

void TestDap::testBuiltinAdapters()
{
    DebugAdapterRegistry& reg = DebugAdapterRegistry::instance();
    
    QList<std::shared_ptr<IDebugAdapter>> adapters = reg.allAdapters();
    
    // Should have at least the built-in adapters
    QVERIFY(adapters.count() >= 4);  // Python, Node.js, GDB, LLDB
    
    // Check for Python adapter
    auto pythonAdapter = reg.adapter("python-debugpy");
    QVERIFY(pythonAdapter != nullptr);
    QCOMPARE(pythonAdapter->config().name, QString("Python (debugpy)"));
    
    // Check for Node.js adapter
    auto nodeAdapter = reg.adapter("node-debug");
    QVERIFY(nodeAdapter != nullptr);
    QVERIFY(nodeAdapter->supportsLanguage("javascript"));
}

void TestDap::testAdapterLookupByFile()
{
    DebugAdapterRegistry& reg = DebugAdapterRegistry::instance();
    
    // Python files
    QList<std::shared_ptr<IDebugAdapter>> pyAdapters = reg.adaptersForFile("/test/script.py");
    QVERIFY(!pyAdapters.isEmpty());
    QVERIFY(pyAdapters.first()->config().id.contains("python"));
    
    // JavaScript files
    QList<std::shared_ptr<IDebugAdapter>> jsAdapters = reg.adaptersForFile("/test/app.js");
    QVERIFY(!jsAdapters.isEmpty());
    
    // C++ files
    QList<std::shared_ptr<IDebugAdapter>> cppAdapters = reg.adaptersForFile("/test/main.cpp");
    QVERIFY(!cppAdapters.isEmpty());
    
    // Unknown extension
    QList<std::shared_ptr<IDebugAdapter>> unknownAdapters = reg.adaptersForFile("/test/file.xyz");
    QVERIFY(unknownAdapters.isEmpty());
}

void TestDap::testAdapterLookupByLanguage()
{
    DebugAdapterRegistry& reg = DebugAdapterRegistry::instance();
    
    QList<std::shared_ptr<IDebugAdapter>> pythonAdapters = reg.adaptersForLanguage("python");
    QVERIFY(!pythonAdapters.isEmpty());
    
    QList<std::shared_ptr<IDebugAdapter>> cppAdapters = reg.adaptersForLanguage("cpp");
    QVERIFY(!cppAdapters.isEmpty());
}

// =============================================================================
// Data Structure Tests
// =============================================================================

void TestDap::testDapBreakpointFromJson()
{
    QJsonObject json;
    json["id"] = 42;
    json["verified"] = true;
    json["message"] = "";
    json["line"] = 100;
    json["column"] = 5;
    
    QJsonObject source;
    source["name"] = "test.py";
    source["path"] = "/home/user/test.py";
    json["source"] = source;
    
    DapBreakpoint bp = DapBreakpoint::fromJson(json);
    
    QCOMPARE(bp.id, 42);
    QVERIFY(bp.verified);
    QCOMPARE(bp.line, 100);
    QCOMPARE(bp.column, 5);
    QCOMPARE(bp.source.name, QString("test.py"));
    QCOMPARE(bp.source.path, QString("/home/user/test.py"));
}

void TestDap::testDapStackFrameFromJson()
{
    QJsonObject json;
    json["id"] = 1;
    json["name"] = "main";
    json["line"] = 50;
    json["column"] = 0;
    json["presentationHint"] = "normal";
    
    QJsonObject source;
    source["name"] = "main.cpp";
    source["path"] = "/project/main.cpp";
    json["source"] = source;
    
    DapStackFrame frame = DapStackFrame::fromJson(json);
    
    QCOMPARE(frame.id, 1);
    QCOMPARE(frame.name, QString("main"));
    QCOMPARE(frame.line, 50);
    QCOMPARE(frame.source.name, QString("main.cpp"));
    QCOMPARE(frame.presentationHint, QString("normal"));
}

void TestDap::testDapVariableFromJson()
{
    QJsonObject json;
    json["name"] = "counter";
    json["value"] = "42";
    json["type"] = "int";
    json["variablesReference"] = 0;
    json["evaluateName"] = "counter";
    
    DapVariable var = DapVariable::fromJson(json);
    
    QCOMPARE(var.name, QString("counter"));
    QCOMPARE(var.value, QString("42"));
    QCOMPARE(var.type, QString("int"));
    QCOMPARE(var.variablesReference, 0);
}

void TestDap::testDapStoppedEventFromJson()
{
    QJsonObject json;
    json["reason"] = "breakpoint";
    json["threadId"] = 1;
    json["allThreadsStopped"] = true;
    json["description"] = "Paused on breakpoint";
    
    QJsonArray hitBps;
    hitBps.append(5);
    hitBps.append(10);
    json["hitBreakpointIds"] = hitBps;
    
    DapStoppedEvent evt = DapStoppedEvent::fromJson(json);
    
    QCOMPARE(evt.reason, DapStoppedReason::Breakpoint);
    QCOMPARE(evt.threadId, 1);
    QVERIFY(evt.allThreadsStopped);
    QCOMPARE(evt.hitBreakpointIds.count(), 2);
    QCOMPARE(evt.hitBreakpointIds.at(0), 5);
}

QTEST_MAIN(TestDap)
#include "test_dap.moc"
