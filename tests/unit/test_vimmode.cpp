#include <QtTest/QtTest>
#include <QPlainTextEdit>
#include "editor/vimmode.h"

class TestVimMode : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testEnableDisable();
    void testModeNames();
    void testNormalToInsertMode();
    void testInsertToNormalMode();
    void testBasicMotions();
    void testDeleteOperator();
    void testVisualMode();

private:
    QPlainTextEdit* m_editor;
    VimMode* m_vim;
};

void TestVimMode::initTestCase()
{
    m_editor = new QPlainTextEdit();
    m_vim = new VimMode(m_editor);
}

void TestVimMode::cleanupTestCase()
{
    delete m_vim;
    delete m_editor;
}

void TestVimMode::testEnableDisable()
{
    QVERIFY(!m_vim->isEnabled());
    
    m_vim->setEnabled(true);
    QVERIFY(m_vim->isEnabled());
    QCOMPARE(m_vim->mode(), VimEditMode::Normal);
    
    m_vim->setEnabled(false);
    QVERIFY(!m_vim->isEnabled());
}

void TestVimMode::testModeNames()
{
    m_vim->setEnabled(true);
    
    QCOMPARE(m_vim->modeName(), QString("NORMAL"));
}

void TestVimMode::testNormalToInsertMode()
{
    m_vim->setEnabled(true);
    QCOMPARE(m_vim->mode(), VimEditMode::Normal);
    
    // Press 'i' to enter insert mode
    QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
    bool handled = m_vim->processKeyEvent(&keyEvent);
    
    QVERIFY(handled);
    QCOMPARE(m_vim->mode(), VimEditMode::Insert);
}

void TestVimMode::testInsertToNormalMode()
{
    m_vim->setEnabled(true);
    
    // First go to insert mode
    QKeyEvent iKey(QEvent::KeyPress, Qt::Key_I, Qt::NoModifier, "i");
    m_vim->processKeyEvent(&iKey);
    QCOMPARE(m_vim->mode(), VimEditMode::Insert);
    
    // Press Escape to return to normal mode
    QKeyEvent escKey(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    bool handled = m_vim->processKeyEvent(&escKey);
    
    QVERIFY(handled);
    QCOMPARE(m_vim->mode(), VimEditMode::Normal);
}

void TestVimMode::testBasicMotions()
{
    m_vim->setEnabled(true);
    m_editor->setPlainText("Hello World\nSecond Line\nThird Line");
    
    // Move cursor to start
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_editor->setTextCursor(cursor);
    
    // Press 'l' to move right
    QKeyEvent lKey(QEvent::KeyPress, Qt::Key_L, Qt::NoModifier, "l");
    m_vim->processKeyEvent(&lKey);
    
    cursor = m_editor->textCursor();
    QCOMPARE(cursor.position(), 1);
    
    // Press 'j' to move down
    QKeyEvent jKey(QEvent::KeyPress, Qt::Key_J, Qt::NoModifier, "j");
    m_vim->processKeyEvent(&jKey);
    
    cursor = m_editor->textCursor();
    QVERIFY(cursor.blockNumber() == 1);
}

void TestVimMode::testDeleteOperator()
{
    m_vim->setEnabled(true);
    m_editor->setPlainText("Hello World");
    
    // Move cursor to start
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    m_editor->setTextCursor(cursor);
    
    // Press 'x' to delete character
    QKeyEvent xKey(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, "x");
    m_vim->processKeyEvent(&xKey);
    
    QCOMPARE(m_editor->toPlainText(), QString("ello World"));
}

void TestVimMode::testVisualMode()
{
    m_vim->setEnabled(true);
    
    // Press 'v' to enter visual mode
    QKeyEvent vKey(QEvent::KeyPress, Qt::Key_V, Qt::NoModifier, "v");
    m_vim->processKeyEvent(&vKey);
    
    QCOMPARE(m_vim->mode(), VimEditMode::Visual);
    
    // Press Escape to exit
    QKeyEvent escKey(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    m_vim->processKeyEvent(&escKey);
    
    QCOMPARE(m_vim->mode(), VimEditMode::Normal);
}

QTEST_MAIN(TestVimMode)
#include "test_vimmode.moc"
