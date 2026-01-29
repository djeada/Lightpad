#include "vimmode.h"
#include "../core/logging/logger.h"

#include <QTextBlock>
#include <QApplication>
#include <QClipboard>

// Constants
static const int VIM_PAGE_SIZE = 20;

VimMode::VimMode(QPlainTextEdit* editor, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
    , m_enabled(false)
    , m_mode(VimEditMode::Normal)
    , m_pendingOperator(VimOperator::None)
    , m_count(0)
{
}

void VimMode::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (enabled) {
            setMode(VimEditMode::Normal);
            m_editor->setCursorWidth(m_editor->fontMetrics().horizontalAdvance('M'));
        } else {
            setMode(VimEditMode::Insert);
            m_editor->setCursorWidth(1);
        }
        LOG_INFO(QString("VIM mode %1").arg(enabled ? "enabled" : "disabled"));
    }
}

bool VimMode::isEnabled() const
{
    return m_enabled;
}

VimEditMode VimMode::mode() const
{
    return m_mode;
}

QString VimMode::modeName() const
{
    switch (m_mode) {
    case VimEditMode::Normal:
        return "NORMAL";
    case VimEditMode::Insert:
        return "INSERT";
    case VimEditMode::Visual:
        return "VISUAL";
    case VimEditMode::VisualLine:
        return "V-LINE";
    case VimEditMode::VisualBlock:
        return "V-BLOCK";
    case VimEditMode::Command:
        return "COMMAND";
    default:
        return "";
    }
}

QString VimMode::commandBuffer() const
{
    return m_commandBuffer;
}

bool VimMode::processKeyEvent(QKeyEvent* event)
{
    if (!m_enabled) {
        return false;
    }

    switch (m_mode) {
    case VimEditMode::Normal:
        return handleNormalMode(event);
    case VimEditMode::Insert:
        return handleInsertMode(event);
    case VimEditMode::Visual:
    case VimEditMode::VisualLine:
    case VimEditMode::VisualBlock:
        return handleVisualMode(event);
    case VimEditMode::Command:
        return handleCommandMode(event);
    default:
        return false;
    }
}

bool VimMode::handleNormalMode(QKeyEvent* event)
{
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();
    QString text = event->text();
    
    // Handle count prefix
    if (!text.isEmpty() && text[0].isDigit() && (m_count > 0 || text[0] != '0')) {
        m_count = m_count * 10 + text[0].digitValue();
        return true;
    }
    
    int count = qMax(1, m_count);
    m_count = 0;

    // Handle pending operator
    if (m_pendingOperator != VimOperator::None) {
        VimMotion motion = VimMotion::None;
        
        // Double operator (dd, yy, cc) operates on line
        if ((key == Qt::Key_D && m_pendingOperator == VimOperator::Delete) ||
            (key == Qt::Key_Y && m_pendingOperator == VimOperator::Yank) ||
            (key == Qt::Key_C && m_pendingOperator == VimOperator::Change)) {
            // Operate on current line
            QTextCursor cursor = m_editor->textCursor();
            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor); // Include newline
            
            m_registerBuffer = cursor.selectedText();
            
            if (m_pendingOperator == VimOperator::Delete || 
                m_pendingOperator == VimOperator::Change) {
                cursor.removeSelectedText();
            }
            
            if (m_pendingOperator == VimOperator::Change) {
                setMode(VimEditMode::Insert);
            }
            
            m_pendingOperator = VimOperator::None;
            return true;
        }
        
        // Motion for operator
        switch (key) {
        case Qt::Key_W: motion = VimMotion::WordForward; break;
        case Qt::Key_B: motion = VimMotion::WordBack; break;
        case Qt::Key_E: motion = VimMotion::WordEnd; break;
        case Qt::Key_H: motion = VimMotion::Left; break;
        case Qt::Key_L: motion = VimMotion::Right; break;
        case Qt::Key_J: motion = VimMotion::Down; break;
        case Qt::Key_K: motion = VimMotion::Up; break;
        case Qt::Key_0: motion = VimMotion::LineStart; break;
        case Qt::Key_Dollar: motion = VimMotion::LineEnd; break;
        default:
            m_pendingOperator = VimOperator::None;
            return false;
        }
        
        executeOperator(m_pendingOperator, motion, count);
        m_pendingOperator = VimOperator::None;
        return true;
    }

    // Regular key handling
    switch (key) {
    // Mode switching
    case Qt::Key_I:
        if (mods & Qt::ShiftModifier) {
            // I - Insert at beginning of line
            moveCursor(QTextCursor::StartOfLine);
            // Move to first non-space
            QTextCursor cursor = m_editor->textCursor();
            QString line = cursor.block().text();
            int pos = 0;
            while (pos < line.length() && line[pos].isSpace()) pos++;
            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos);
            m_editor->setTextCursor(cursor);
        }
        setMode(VimEditMode::Insert);
        return true;
        
    case Qt::Key_A:
        if (mods & Qt::ShiftModifier) {
            // A - Append at end of line
            moveCursor(QTextCursor::EndOfLine);
        } else {
            // a - Append after cursor
            moveCursor(QTextCursor::Right);
        }
        setMode(VimEditMode::Insert);
        return true;
        
    case Qt::Key_O:
        insertNewLine(mods & Qt::ShiftModifier);
        setMode(VimEditMode::Insert);
        return true;
        
    case Qt::Key_V:
        if (mods & Qt::ShiftModifier) {
            setMode(VimEditMode::VisualLine);
        } else if (mods & Qt::ControlModifier) {
            setMode(VimEditMode::VisualBlock);
        } else {
            setMode(VimEditMode::Visual);
        }
        return true;
        
    case Qt::Key_Colon:
        setMode(VimEditMode::Command);
        m_commandBuffer = "";
        return true;

    // Motions
    case Qt::Key_H:
    case Qt::Key_Left:
        executeMotion(VimMotion::Left, count);
        return true;
        
    case Qt::Key_L:
    case Qt::Key_Right:
        executeMotion(VimMotion::Right, count);
        return true;
        
    case Qt::Key_J:
        if (mods & Qt::ShiftModifier) {
            // Shift+J - Join lines
            joinLines();
        } else {
            // j - Move down
            executeMotion(VimMotion::Down, count);
        }
        return true;
        
    case Qt::Key_Down:
        executeMotion(VimMotion::Down, count);
        return true;
        
    case Qt::Key_K:
    case Qt::Key_Up:
        executeMotion(VimMotion::Up, count);
        return true;
        
    case Qt::Key_W:
        executeMotion(VimMotion::WordForward, count);
        return true;
        
    case Qt::Key_B:
        executeMotion(VimMotion::WordBack, count);
        return true;
        
    case Qt::Key_E:
        executeMotion(VimMotion::WordEnd, count);
        return true;
        
    case Qt::Key_0:
        executeMotion(VimMotion::LineStart);
        return true;
        
    case Qt::Key_Dollar:
        executeMotion(VimMotion::LineEnd);
        return true;
        
    case Qt::Key_AsciiCircum:
        executeMotion(VimMotion::FirstNonSpace);
        return true;
        
    case Qt::Key_G:
        if (mods & Qt::ShiftModifier) {
            executeMotion(VimMotion::FileEnd);
        } else {
            // Wait for second g
            m_commandBuffer = "g";
        }
        return true;

    // Operators
    case Qt::Key_D:
        if (mods & Qt::ShiftModifier) {
            // D - Delete to end of line
            deleteText(VimMotion::LineEnd);
        } else {
            m_pendingOperator = VimOperator::Delete;
        }
        return true;
        
    case Qt::Key_C:
        if (mods & Qt::ShiftModifier) {
            // C - Change to end of line
            changeText(VimMotion::LineEnd);
        } else {
            m_pendingOperator = VimOperator::Change;
        }
        return true;
        
    case Qt::Key_Y:
        if (mods & Qt::ShiftModifier) {
            // Y - Yank line
            yankText(VimMotion::LineEnd);
        } else {
            m_pendingOperator = VimOperator::Yank;
        }
        return true;

    // Other commands
    case Qt::Key_X:
        deleteText(VimMotion::Right);
        return true;
        
    case Qt::Key_S:
        deleteText(VimMotion::Right);
        setMode(VimEditMode::Insert);
        return true;
        
    case Qt::Key_P:
        if (!m_registerBuffer.isEmpty()) {
            QTextCursor cursor = m_editor->textCursor();
            if (mods & Qt::ShiftModifier) {
                // P - Paste before
                cursor.insertText(m_registerBuffer);
            } else {
                // p - Paste after
                cursor.movePosition(QTextCursor::Right);
                cursor.insertText(m_registerBuffer);
            }
            m_editor->setTextCursor(cursor);
        }
        return true;
        
    case Qt::Key_U:
        if (mods & Qt::ControlModifier) {
            // Ctrl+U - Page up
            executeMotion(VimMotion::PageUp, count);
        } else {
            // u - Undo
            m_editor->undo();
        }
        return true;
        
    case Qt::Key_R:
        if (mods & Qt::ControlModifier) {
            // Ctrl+R - Redo
            m_editor->redo();
        } else {
            // r - Replace mode - wait for next char
            m_commandBuffer = "r";
        }
        return true;

    default:
        // Handle pending commands
        if (m_commandBuffer == "g" && key == Qt::Key_G) {
            executeMotion(VimMotion::FileStart);
            m_commandBuffer.clear();
            return true;
        }
        if (m_commandBuffer == "r" && !text.isEmpty()) {
            replaceChar(text[0]);
            m_commandBuffer.clear();
            return true;
        }
        break;
    }
    
    return false;
}

bool VimMode::handleInsertMode(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape || 
        (event->key() == Qt::Key_BracketLeft && event->modifiers() & Qt::ControlModifier)) {
        setMode(VimEditMode::Normal);
        // Move cursor left (VIM behavior)
        moveCursor(QTextCursor::Left);
        return true;
    }
    
    // Let the editor handle the key
    return false;
}

bool VimMode::handleVisualMode(QKeyEvent* event)
{
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();
    
    if (key == Qt::Key_Escape) {
        setMode(VimEditMode::Normal);
        return true;
    }
    
    int count = qMax(1, m_count);
    m_count = 0;
    
    QTextCursor cursor = m_editor->textCursor();
    
    switch (key) {
    // Motions (extend selection)
    case Qt::Key_H:
    case Qt::Key_Left:
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, count);
        m_editor->setTextCursor(cursor);
        return true;
        
    case Qt::Key_L:
    case Qt::Key_Right:
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, count);
        m_editor->setTextCursor(cursor);
        return true;
        
    case Qt::Key_J:
    case Qt::Key_Down:
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, count);
        m_editor->setTextCursor(cursor);
        return true;
        
    case Qt::Key_K:
    case Qt::Key_Up:
        cursor.movePosition(QTextCursor::Up, QTextCursor::KeepAnchor, count);
        m_editor->setTextCursor(cursor);
        return true;
        
    case Qt::Key_W:
        cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor, count);
        m_editor->setTextCursor(cursor);
        return true;
        
    case Qt::Key_B:
        cursor.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor, count);
        m_editor->setTextCursor(cursor);
        return true;

    // Operations on selection
    case Qt::Key_D:
    case Qt::Key_X:
        m_registerBuffer = cursor.selectedText();
        cursor.removeSelectedText();
        setMode(VimEditMode::Normal);
        return true;
        
    case Qt::Key_Y:
        m_registerBuffer = cursor.selectedText();
        cursor.clearSelection();
        m_editor->setTextCursor(cursor);
        setMode(VimEditMode::Normal);
        emit statusMessage("Yanked");
        return true;
        
    case Qt::Key_C:
        m_registerBuffer = cursor.selectedText();
        cursor.removeSelectedText();
        setMode(VimEditMode::Insert);
        return true;
        
    case Qt::Key_V:
        if (mods & Qt::ShiftModifier) {
            setMode(VimEditMode::VisualLine);
        } else {
            setMode(VimEditMode::Normal);
        }
        return true;
        
    default:
        break;
    }
    
    return false;
}

bool VimMode::handleCommandMode(QKeyEvent* event)
{
    int key = event->key();
    QString text = event->text();
    
    if (key == Qt::Key_Escape) {
        m_commandBuffer.clear();
        setMode(VimEditMode::Normal);
        return true;
    }
    
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        executeCommand(m_commandBuffer);
        m_commandBuffer.clear();
        setMode(VimEditMode::Normal);
        return true;
    }
    
    if (key == Qt::Key_Backspace) {
        if (!m_commandBuffer.isEmpty()) {
            m_commandBuffer.chop(1);
        } else {
            setMode(VimEditMode::Normal);
        }
        return true;
    }
    
    if (!text.isEmpty()) {
        m_commandBuffer += text;
    }
    
    return true;
}

void VimMode::setMode(VimEditMode mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        
        // Adjust cursor style
        if (mode == VimEditMode::Insert) {
            m_editor->setCursorWidth(1);
        } else {
            m_editor->setCursorWidth(m_editor->fontMetrics().horizontalAdvance('M'));
        }
        
        emit modeChanged(mode);
        LOG_DEBUG(QString("VIM mode changed to: %1").arg(modeName()));
    }
}

void VimMode::executeMotion(VimMotion motion, int count)
{
    QTextCursor cursor = m_editor->textCursor();
    
    for (int i = 0; i < count; ++i) {
        switch (motion) {
        case VimMotion::Left:
            cursor.movePosition(QTextCursor::Left);
            break;
        case VimMotion::Right:
            cursor.movePosition(QTextCursor::Right);
            break;
        case VimMotion::Up:
            cursor.movePosition(QTextCursor::Up);
            break;
        case VimMotion::Down:
            cursor.movePosition(QTextCursor::Down);
            break;
        case VimMotion::WordForward:
            cursor.movePosition(QTextCursor::NextWord);
            break;
        case VimMotion::WordBack:
            cursor.movePosition(QTextCursor::PreviousWord);
            break;
        case VimMotion::WordEnd:
            cursor.movePosition(QTextCursor::EndOfWord);
            break;
        case VimMotion::LineStart:
            cursor.movePosition(QTextCursor::StartOfLine);
            break;
        case VimMotion::LineEnd:
            cursor.movePosition(QTextCursor::EndOfLine);
            break;
        case VimMotion::FirstNonSpace:
            cursor.movePosition(QTextCursor::StartOfLine);
            {
                QString line = cursor.block().text();
                int pos = 0;
                while (pos < line.length() && line[pos].isSpace()) pos++;
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos);
            }
            break;
        case VimMotion::FileStart:
            cursor.movePosition(QTextCursor::Start);
            break;
        case VimMotion::FileEnd:
            cursor.movePosition(QTextCursor::End);
            break;
        case VimMotion::PageUp:
            cursor.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, VIM_PAGE_SIZE);
            break;
        case VimMotion::PageDown:
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, VIM_PAGE_SIZE);
            break;
        default:
            break;
        }
    }
    
    m_editor->setTextCursor(cursor);
}

void VimMode::executeOperator(VimOperator op, VimMotion motion, int count)
{
    switch (op) {
    case VimOperator::Delete:
        deleteText(motion, count);
        break;
    case VimOperator::Change:
        changeText(motion, count);
        break;
    case VimOperator::Yank:
        yankText(motion, count);
        break;
    default:
        break;
    }
}

void VimMode::executeCommand(const QString& command)
{
    LOG_DEBUG(QString("Executing VIM command: %1").arg(command));
    
    if (command == "w") {
        emit commandExecuted("save");
        emit statusMessage("File saved");
    } else if (command == "q") {
        emit commandExecuted("quit");
    } else if (command == "wq" || command == "x") {
        emit commandExecuted("save");
        emit commandExecuted("quit");
    } else if (command == "q!") {
        emit commandExecuted("forceQuit");
    } else if (command.startsWith("set ")) {
        QString option = command.mid(4);
        emit statusMessage(QString("Set: %1").arg(option));
    } else {
        emit statusMessage(QString("Unknown command: %1").arg(command));
    }
}

void VimMode::moveCursor(QTextCursor::MoveOperation op, int count)
{
    QTextCursor cursor = m_editor->textCursor();
    for (int i = 0; i < count; ++i) {
        cursor.movePosition(op);
    }
    m_editor->setTextCursor(cursor);
}

void VimMode::deleteText(VimMotion motion, int count)
{
    QTextCursor cursor = m_editor->textCursor();
    int startPos = cursor.position();
    
    executeMotion(motion, count);
    
    cursor = m_editor->textCursor();
    int endPos = cursor.position();
    
    cursor.setPosition(qMin(startPos, endPos));
    cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);
    
    m_registerBuffer = cursor.selectedText();
    cursor.removeSelectedText();
    m_editor->setTextCursor(cursor);
}

void VimMode::yankText(VimMotion motion, int count)
{
    QTextCursor cursor = m_editor->textCursor();
    int startPos = cursor.position();
    
    executeMotion(motion, count);
    
    cursor = m_editor->textCursor();
    int endPos = cursor.position();
    
    cursor.setPosition(qMin(startPos, endPos));
    cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);
    
    m_registerBuffer = cursor.selectedText();
    cursor.setPosition(startPos);
    m_editor->setTextCursor(cursor);
    
    emit statusMessage("Yanked");
}

void VimMode::changeText(VimMotion motion, int count)
{
    deleteText(motion, count);
    setMode(VimEditMode::Insert);
}

void VimMode::insertNewLine(bool above)
{
    QTextCursor cursor = m_editor->textCursor();
    
    if (above) {
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("\n");
        cursor.movePosition(QTextCursor::Up);
    } else {
        cursor.movePosition(QTextCursor::EndOfLine);
        cursor.insertText("\n");
    }
    
    m_editor->setTextCursor(cursor);
}

void VimMode::joinLines()
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    
    // Remove leading whitespace from next line
    while (!cursor.atEnd()) {
        QChar ch = m_editor->document()->characterAt(cursor.position());
        if (ch.isSpace() && ch != '\n') {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        } else {
            break;
        }
    }
    
    cursor.insertText(" ");
    m_editor->setTextCursor(cursor);
}

void VimMode::replaceChar(QChar ch)
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    cursor.insertText(QString(ch));
    cursor.movePosition(QTextCursor::Left);
    m_editor->setTextCursor(cursor);
}
