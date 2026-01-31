#include "vimmode.h"
#include "../core/logging/logger.h"

#include <QTextBlock>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QScrollBar>

// Constants
static const int VIM_PAGE_SIZE = 20;
static const int VIM_HALF_PAGE_SIZE = 10;

VimMode::VimMode(QPlainTextEdit* editor, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
    , m_enabled(false)
    , m_mode(VimEditMode::Normal)
    , m_pendingOperator(VimOperator::None)
    , m_count(0)
    , m_searchForward(true)
    , m_lastChangeCount(1)
    , m_recordingChange(false)
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
    case VimEditMode::Replace:
        return "REPLACE";
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
    case VimEditMode::Replace:
        return handleReplaceMode(event);
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
        
        // Handle text objects (i/a prefix for inner/around)
        if (m_commandBuffer == "i" || m_commandBuffer == "a") {
            bool inner = (m_commandBuffer == "i");
            VimTextObject textObj = VimTextObject::None;
            
            switch (key) {
            case Qt::Key_W: textObj = inner ? VimTextObject::InnerWord : VimTextObject::AroundWord; break;
            case Qt::Key_ParenLeft:
            case Qt::Key_ParenRight: textObj = inner ? VimTextObject::InnerParen : VimTextObject::AroundParen; break;
            case Qt::Key_BracketLeft:
            case Qt::Key_BracketRight: textObj = inner ? VimTextObject::InnerBracket : VimTextObject::AroundBracket; break;
            case Qt::Key_BraceLeft:
            case Qt::Key_BraceRight: textObj = inner ? VimTextObject::InnerBrace : VimTextObject::AroundBrace; break;
            case Qt::Key_Less:
            case Qt::Key_Greater: textObj = inner ? VimTextObject::InnerAngle : VimTextObject::AroundAngle; break;
            case Qt::Key_QuoteDbl: textObj = inner ? VimTextObject::InnerQuote : VimTextObject::AroundQuote; break;
            case Qt::Key_Apostrophe: textObj = inner ? VimTextObject::InnerSingleQuote : VimTextObject::AroundSingleQuote; break;
            case Qt::Key_QuoteLeft: textObj = inner ? VimTextObject::InnerBacktick : VimTextObject::AroundBacktick; break;
            default: break;
            }
            
            if (textObj != VimTextObject::None) {
                executeOperatorOnTextObject(m_pendingOperator, textObj);
                m_pendingOperator = VimOperator::None;
                m_commandBuffer.clear();
                return true;
            }
            m_commandBuffer.clear();
        }
        
        // Start text object with i or a
        if (key == Qt::Key_I && !(mods & Qt::ShiftModifier)) {
            m_commandBuffer = "i";
            return true;
        }
        if (key == Qt::Key_A && !(mods & Qt::ShiftModifier)) {
            m_commandBuffer = "a";
            return true;
        }
        
        // Double operator (dd, yy, cc, >>, <<) operates on line
        if ((key == Qt::Key_D && m_pendingOperator == VimOperator::Delete) ||
            (key == Qt::Key_Y && m_pendingOperator == VimOperator::Yank) ||
            (key == Qt::Key_C && m_pendingOperator == VimOperator::Change) ||
            (key == Qt::Key_Greater && m_pendingOperator == VimOperator::Indent) ||
            (key == Qt::Key_Less && m_pendingOperator == VimOperator::Unindent)) {
            // Operate on current line
            QTextCursor cursor = m_editor->textCursor();
            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            
            if (m_pendingOperator == VimOperator::Indent || m_pendingOperator == VimOperator::Unindent) {
                // Indent/unindent the line
                QString line = cursor.selectedText();
                cursor.movePosition(QTextCursor::StartOfLine);
                cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
                
                if (m_pendingOperator == VimOperator::Indent) {
                    cursor.insertText("    " + line);
                } else {
                    // Remove leading spaces/tab
                    if (line.startsWith("    ")) {
                        cursor.insertText(line.mid(4));
                    } else if (line.startsWith("\t")) {
                        cursor.insertText(line.mid(1));
                    } else {
                        int spaces = 0;
                        while (spaces < line.length() && line[spaces] == ' ' && spaces < 4) spaces++;
                        cursor.insertText(line.mid(spaces));
                    }
                }
                m_editor->setTextCursor(cursor);
            } else {
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor); // Include newline
                m_registerBuffer = cursor.selectedText();
                
                if (m_pendingOperator == VimOperator::Delete || 
                    m_pendingOperator == VimOperator::Change) {
                    cursor.removeSelectedText();
                }
                m_editor->setTextCursor(cursor);
                
                if (m_pendingOperator == VimOperator::Change) {
                    setMode(VimEditMode::Insert);
                }
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
        case Qt::Key_Percent: motion = VimMotion::MatchingBrace; break;
        case Qt::Key_BraceLeft: motion = VimMotion::PrevParagraph; break;
        case Qt::Key_BraceRight: motion = VimMotion::NextParagraph; break;
        default:
            m_pendingOperator = VimOperator::None;
            return false;
        }
        
        executeOperator(m_pendingOperator, motion, count);
        m_pendingOperator = VimOperator::None;
        return true;
    }

    // Handle pending commands first (g, r, f, F, t, T, m, ', z)
    if (!m_commandBuffer.isEmpty()) {
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
        // Handle f/F/t/T commands
        if ((m_commandBuffer == "f" || m_commandBuffer == "F" ||
             m_commandBuffer == "t" || m_commandBuffer == "T") && !text.isEmpty()) {
            bool backward = (m_commandBuffer == "F" || m_commandBuffer == "T");
            bool before = (m_commandBuffer == "t" || m_commandBuffer == "T");
            m_findChar = text[0];
            m_findCharBefore = before;
            m_findCharBackward = backward;
            moveCursorToChar(m_findChar, before, backward);
            m_commandBuffer.clear();
            return true;
        }
        // Handle m{a-z} for setting marks
        if (m_commandBuffer == "m" && !text.isEmpty()) {
            QChar mark = text[0];
            if ((mark >= 'a' && mark <= 'z') || (mark >= 'A' && mark <= 'Z')) {
                setMark(mark);
            }
            m_commandBuffer.clear();
            return true;
        }
        // Handle '{a-z} for jumping to marks
        if (m_commandBuffer == "'" && !text.isEmpty()) {
            QChar mark = text[0];
            if ((mark >= 'a' && mark <= 'z') || (mark >= 'A' && mark <= 'Z')) {
                jumpToMark(mark);
            }
            m_commandBuffer.clear();
            return true;
        }
        // Handle z commands
        if (m_commandBuffer == "z") {
            QTextCursor cursor = m_editor->textCursor();
            if (key == Qt::Key_Z || text == "z") {
                // zz - Center cursor line in window
                m_editor->centerCursor();
            } else if (key == Qt::Key_T || text == "t") {
                // zt - Scroll cursor line to top
                m_editor->centerCursor();
                for (int i = 0; i < VIM_HALF_PAGE_SIZE; ++i) {
                    m_editor->verticalScrollBar()->setValue(
                        m_editor->verticalScrollBar()->value() - m_editor->fontMetrics().height());
                }
            } else if (key == Qt::Key_B || text == "b") {
                // zb - Scroll cursor line to bottom
                m_editor->centerCursor();
                for (int i = 0; i < VIM_HALF_PAGE_SIZE; ++i) {
                    m_editor->verticalScrollBar()->setValue(
                        m_editor->verticalScrollBar()->value() + m_editor->fontMetrics().height());
                }
            }
            m_commandBuffer.clear();
            return true;
        }
        // Unknown pending command, clear it
        m_commandBuffer.clear();
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

    case Qt::Key_Slash:
        // / - Start forward search
        setMode(VimEditMode::Command);
        m_commandBuffer = "/";
        m_searchForward = true;
        return true;

    case Qt::Key_Question:
        // ? - Start backward search
        setMode(VimEditMode::Command);
        m_commandBuffer = "?";
        m_searchForward = false;
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
        if (mods & Qt::ControlModifier) {
            // Ctrl+B - Full page up
            executeMotion(VimMotion::FullPageUp, count);
        } else {
            executeMotion(VimMotion::WordBack, count);
        }
        return true;
        
    case Qt::Key_E:
        if (mods & Qt::ControlModifier) {
            // Ctrl+E - Scroll down one line
            scrollLines(count);
        } else {
            executeMotion(VimMotion::WordEnd, count);
        }
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

    case Qt::Key_Percent:
        // % - Jump to matching brace
        executeMotion(VimMotion::MatchingBrace);
        return true;

    case Qt::Key_BraceLeft:
        // { - Previous paragraph
        executeMotion(VimMotion::PrevParagraph, count);
        return true;

    case Qt::Key_BraceRight:
        // } - Next paragraph
        executeMotion(VimMotion::NextParagraph, count);
        return true;

    case Qt::Key_ParenLeft:
        // ( - Previous sentence
        executeMotion(VimMotion::PrevSentence, count);
        return true;

    case Qt::Key_ParenRight:
        // ) - Next sentence
        executeMotion(VimMotion::NextSentence, count);
        return true;

    case Qt::Key_Asterisk:
        // * - Search word under cursor forward
        searchWord(true);
        return true;

    case Qt::Key_NumberSign:
        // # - Search word under cursor backward
        searchWord(false);
        return true;

    case Qt::Key_N:
        if (mods & Qt::ShiftModifier) {
            // N - Search previous
            searchNext(false);
        } else {
            // n - Search next
            searchNext(true);
        }
        return true;

    case Qt::Key_F:
        if (mods & Qt::ControlModifier) {
            // Ctrl+F - Full page down
            executeMotion(VimMotion::FullPageDown, count);
        } else if (mods & Qt::ShiftModifier) {
            // F - Find char backward
            m_commandBuffer = "F";
        } else {
            // f - Find char forward
            m_commandBuffer = "f";
        }
        return true;

    case Qt::Key_T:
        if (mods & Qt::ShiftModifier) {
            // T - To char backward
            m_commandBuffer = "T";
        } else {
            // t - To char forward
            m_commandBuffer = "t";
        }
        return true;

    case Qt::Key_Semicolon:
        // ; - Repeat last f/F/t/T
        if (!m_findChar.isNull()) {
            moveCursorToChar(m_findChar, m_findCharBefore, m_findCharBackward);
        }
        return true;

    case Qt::Key_Comma:
        // , - Repeat last f/F/t/T in opposite direction
        if (!m_findChar.isNull()) {
            moveCursorToChar(m_findChar, m_findCharBefore, !m_findCharBackward);
        }
        return true;

    // Operators
    case Qt::Key_D:
        if (mods & Qt::ControlModifier) {
            // Ctrl+D - Half page down
            executeMotion(VimMotion::PageDown, count);
        } else if (mods & Qt::ShiftModifier) {
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
        if (mods & Qt::ControlModifier) {
            // Ctrl+Y - Scroll up one line
            scrollLines(-count);
        } else if (mods & Qt::ShiftModifier) {
            // Y - Yank line
            yankText(VimMotion::LineEnd);
        } else {
            m_pendingOperator = VimOperator::Yank;
        }
        return true;

    case Qt::Key_Greater:
        // > - Indent operator
        m_pendingOperator = VimOperator::Indent;
        return true;

    case Qt::Key_Less:
        // < - Unindent operator
        m_pendingOperator = VimOperator::Unindent;
        return true;

    case Qt::Key_AsciiTilde:
        // ~ - Toggle case of character under cursor and move right
        {
            QTextCursor cursor = m_editor->textCursor();
            for (int i = 0; i < count; ++i) {
                if (!cursor.atEnd()) {
                    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                    QString ch = cursor.selectedText();
                    if (!ch.isEmpty()) {
                        QChar c = ch[0];
                        if (c.isLower()) {
                            cursor.insertText(c.toUpper());
                        } else if (c.isUpper()) {
                            cursor.insertText(c.toLower());
                        } else {
                            cursor.clearSelection();
                            cursor.movePosition(QTextCursor::Right);
                        }
                    }
                }
            }
            m_editor->setTextCursor(cursor);
        }
        return true;

    // Other commands
    case Qt::Key_X:
        deleteText(VimMotion::Right, count);
        return true;
        
    case Qt::Key_S:
        if (mods & Qt::ShiftModifier) {
            // S - Delete line and enter insert mode
            QTextCursor cursor = m_editor->textCursor();
            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            m_registerBuffer = cursor.selectedText();
            cursor.removeSelectedText();
            m_editor->setTextCursor(cursor);
            setMode(VimEditMode::Insert);
        } else {
            // s - Delete character and enter insert mode
            deleteText(VimMotion::Right);
            setMode(VimEditMode::Insert);
        }
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
            // Ctrl+U - Half page up
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
        } else if (mods & Qt::ShiftModifier) {
            // R - Enter Replace mode
            setMode(VimEditMode::Replace);
        } else {
            // r - Replace single char - wait for next char
            m_commandBuffer = "r";
        }
        return true;

    case Qt::Key_M:
        // m - Set mark, wait for mark character
        m_commandBuffer = "m";
        return true;

    case Qt::Key_Apostrophe:
    case Qt::Key_QuoteLeft:
        // ' or ` - Jump to mark, wait for mark character
        m_commandBuffer = "'";
        return true;

    case Qt::Key_Period:
        // . - Repeat last change
        repeatLastChange();
        return true;

    case Qt::Key_Z:
        // z commands for scrolling
        m_commandBuffer = "z";
        return true;

    default:
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

bool VimMode::handleReplaceMode(QKeyEvent* event)
{
    int key = event->key();
    QString text = event->text();
    
    if (key == Qt::Key_Escape || 
        (key == Qt::Key_BracketLeft && event->modifiers() & Qt::ControlModifier)) {
        setMode(VimEditMode::Normal);
        moveCursor(QTextCursor::Left);
        return true;
    }
    
    if (key == Qt::Key_Backspace) {
        // In replace mode, backspace moves left without deleting
        moveCursor(QTextCursor::Left);
        return true;
    }
    
    // Replace character under cursor with typed character
    if (!text.isEmpty() && text[0].isPrint()) {
        QTextCursor cursor = m_editor->textCursor();
        if (!cursor.atEnd()) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            cursor.insertText(text);
        } else {
            cursor.insertText(text);
        }
        m_editor->setTextCursor(cursor);
        return true;
    }
    
    // Let the editor handle other keys (arrows, etc.)
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
        if (mode == VimEditMode::Insert || mode == VimEditMode::Replace) {
            // Thin cursor for insert mode, block cursor for replace mode would be nice
            // but Qt doesn't support underline cursor, so use thin for both
            m_editor->setCursorWidth(mode == VimEditMode::Replace ? 
                m_editor->fontMetrics().horizontalAdvance('M') / 2 : 1);
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
        case VimMotion::HalfPageUp:
            cursor.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, VIM_HALF_PAGE_SIZE);
            break;
        case VimMotion::PageDown:
        case VimMotion::HalfPageDown:
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, VIM_HALF_PAGE_SIZE);
            break;
        case VimMotion::FullPageUp:
            cursor.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, VIM_PAGE_SIZE);
            break;
        case VimMotion::FullPageDown:
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, VIM_PAGE_SIZE);
            break;
        case VimMotion::MatchingBrace:
            m_editor->setTextCursor(cursor);
            moveCursorToMatchingBrace();
            cursor = m_editor->textCursor();
            break;
        case VimMotion::NextParagraph:
            m_editor->setTextCursor(cursor);
            moveCursorToParagraph(true);
            cursor = m_editor->textCursor();
            break;
        case VimMotion::PrevParagraph:
            m_editor->setTextCursor(cursor);
            moveCursorToParagraph(false);
            cursor = m_editor->textCursor();
            break;
        case VimMotion::NextSentence:
            m_editor->setTextCursor(cursor);
            moveCursorToSentence(true);
            cursor = m_editor->textCursor();
            break;
        case VimMotion::PrevSentence:
            m_editor->setTextCursor(cursor);
            moveCursorToSentence(false);
            cursor = m_editor->textCursor();
            break;
        case VimMotion::SearchNext:
            m_editor->setTextCursor(cursor);
            searchNext(true);
            cursor = m_editor->textCursor();
            break;
        case VimMotion::SearchPrev:
            m_editor->setTextCursor(cursor);
            searchNext(false);
            cursor = m_editor->textCursor();
            break;
        case VimMotion::WordUnderCursor:
            m_editor->setTextCursor(cursor);
            searchWord(true);
            cursor = m_editor->textCursor();
            break;
        case VimMotion::WordUnderCursorBack:
            m_editor->setTextCursor(cursor);
            searchWord(false);
            cursor = m_editor->textCursor();
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
    case VimOperator::Indent:
        indentText(motion, count, true);
        break;
    case VimOperator::Unindent:
        indentText(motion, count, false);
        break;
    case VimOperator::ToggleCase:
        toggleCase(motion, count);
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
    } else if (command.startsWith("/") || command.startsWith("?")) {
        // Search command
        bool forward = command.startsWith("/");
        QString pattern = command.mid(1);
        if (!pattern.isEmpty()) {
            m_searchPattern = QRegularExpression::escape(pattern);
            m_searchForward = forward;
            searchNext(true);
        }
    } else if (command.startsWith("s/") || command.startsWith("%s/")) {
        // Substitute command
        QString cmd = command;
        bool global = cmd.startsWith("%");
        if (global) cmd = cmd.mid(1);
        
        QStringList parts = cmd.mid(2).split('/');
        if (parts.size() >= 2) {
            QString pattern = parts[0];
            QString replacement = parts[1];
            bool replaceAll = (parts.size() > 2 && parts[2].contains('g'));
            
            QTextCursor cursor = m_editor->textCursor();
            QString text;
            int startPos = 0;
            
            if (global) {
                text = m_editor->toPlainText();
            } else {
                // Only current line
                text = cursor.block().text();
                startPos = cursor.block().position();
            }
            
            QRegularExpression regex(pattern);
            QString newText;
            if (replaceAll) {
                newText = text;
                newText.replace(regex, replacement);
            } else {
                // Replace first occurrence only
                QRegularExpressionMatch match = regex.match(text);
                if (match.hasMatch()) {
                    newText = text.left(match.capturedStart()) + replacement + text.mid(match.capturedEnd());
                } else {
                    newText = text;
                }
            }
            
            if (global) {
                cursor.select(QTextCursor::Document);
                cursor.insertText(newText);
            } else {
                cursor.movePosition(QTextCursor::StartOfLine);
                cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
                cursor.insertText(newText);
            }
            m_editor->setTextCursor(cursor);
            emit statusMessage("Substitution complete");
        }
    } else if (command.startsWith("e ")) {
        // Edit file command
        QString filename = command.mid(2).trimmed();
        emit commandExecuted(QString("edit:%1").arg(filename));
    } else {
        // Try to parse as line number
        bool ok;
        int lineNum = command.toInt(&ok);
        if (ok && lineNum > 0) {
            // Go to line
            QTextCursor cursor = m_editor->textCursor();
            QTextBlock block = m_editor->document()->findBlockByNumber(lineNum - 1);
            if (block.isValid()) {
                cursor.setPosition(block.position());
                m_editor->setTextCursor(cursor);
                emit statusMessage(QString("Line %1").arg(lineNum));
            } else {
                emit statusMessage("Invalid line number");
            }
        } else {
            emit statusMessage(QString("Unknown command: %1").arg(command));
        }
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

void VimMode::moveCursorToChar(QChar ch, bool before, bool backward)
{
    QTextCursor cursor = m_editor->textCursor();
    QString line = cursor.block().text();
    int col = cursor.positionInBlock();
    
    if (backward) {
        // Search backward from current position
        for (int i = col - 1; i >= 0; --i) {
            if (line[i] == ch) {
                int newPos = before ? i + 1 : i;
                cursor.movePosition(QTextCursor::StartOfLine);
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newPos);
                m_editor->setTextCursor(cursor);
                return;
            }
        }
    } else {
        // Search forward from current position
        for (int i = col + 1; i < line.length(); ++i) {
            if (line[i] == ch) {
                int newPos = before ? i - 1 : i;
                cursor.movePosition(QTextCursor::StartOfLine);
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newPos);
                m_editor->setTextCursor(cursor);
                return;
            }
        }
    }
}

bool VimMode::moveCursorToMatchingBrace()
{
    QTextCursor cursor = m_editor->textCursor();
    QChar ch = m_editor->document()->characterAt(cursor.position());
    
    QChar match;
    bool forward = true;
    
    if (ch == '(') { match = ')'; forward = true; }
    else if (ch == ')') { match = '('; forward = false; }
    else if (ch == '[') { match = ']'; forward = true; }
    else if (ch == ']') { match = '['; forward = false; }
    else if (ch == '{') { match = '}'; forward = true; }
    else if (ch == '}') { match = '{'; forward = false; }
    else if (ch == '<') { match = '>'; forward = true; }
    else if (ch == '>') { match = '<'; forward = false; }
    else { return false; }
    
    int depth = 1;
    int pos = cursor.position();
    int len = m_editor->document()->characterCount();
    
    while (depth > 0) {
        if (forward) {
            pos++;
            if (pos >= len) return false;
        } else {
            pos--;
            if (pos < 0) return false;
        }
        
        QChar c = m_editor->document()->characterAt(pos);
        if (c == ch) depth++;
        else if (c == match) depth--;
    }
    
    cursor.setPosition(pos);
    m_editor->setTextCursor(cursor);
    return true;
}

void VimMode::moveCursorToParagraph(bool forward)
{
    QTextCursor cursor = m_editor->textCursor();
    
    if (forward) {
        // Move to next empty line (paragraph boundary)
        while (!cursor.atEnd()) {
            cursor.movePosition(QTextCursor::NextBlock);
            if (cursor.block().text().trimmed().isEmpty()) {
                break;
            }
        }
    } else {
        // Move to previous empty line (paragraph boundary)
        while (!cursor.atStart()) {
            cursor.movePosition(QTextCursor::PreviousBlock);
            if (cursor.block().text().trimmed().isEmpty()) {
                break;
            }
        }
    }
    
    m_editor->setTextCursor(cursor);
}

void VimMode::moveCursorToSentence(bool forward)
{
    QTextCursor cursor = m_editor->textCursor();
    QString text = m_editor->toPlainText();
    int pos = cursor.position();
    
    // Simple sentence detection: look for . ! ? followed by space or newline
    QRegularExpression sentenceEnd("[.!?][\\s\\n]");
    
    if (forward) {
        QRegularExpressionMatch match = sentenceEnd.match(text, pos);
        if (match.hasMatch()) {
            cursor.setPosition(match.capturedEnd());
            // Skip whitespace
            while (cursor.position() < text.length() && 
                   text[cursor.position()].isSpace()) {
                cursor.movePosition(QTextCursor::Right);
            }
        } else {
            cursor.movePosition(QTextCursor::End);
        }
    } else {
        // Search backward
        int searchPos = qMax(0, pos - 2);
        int lastMatch = 0;
        QRegularExpressionMatchIterator it = sentenceEnd.globalMatch(text.left(searchPos));
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            lastMatch = m.capturedEnd();
        }
        cursor.setPosition(lastMatch);
        // Skip whitespace
        while (cursor.position() < text.length() && 
               text[cursor.position()].isSpace()) {
            cursor.movePosition(QTextCursor::Right);
        }
    }
    
    m_editor->setTextCursor(cursor);
}

void VimMode::setMark(QChar mark)
{
    m_marks[mark] = m_editor->textCursor().position();
    emit statusMessage(QString("Mark '%1' set").arg(mark));
}

bool VimMode::jumpToMark(QChar mark)
{
    if (!m_marks.contains(mark)) {
        emit statusMessage(QString("Mark '%1' not set").arg(mark));
        return false;
    }
    
    QTextCursor cursor = m_editor->textCursor();
    cursor.setPosition(m_marks[mark]);
    m_editor->setTextCursor(cursor);
    return true;
}

void VimMode::repeatLastChange()
{
    // Simple implementation: re-execute last recorded change
    // For now, emit a message - full implementation would require recording keystrokes
    emit statusMessage("Repeat not fully implemented");
}

void VimMode::recordChange(const QString& change)
{
    m_lastChange = change;
    m_lastChangeCount = qMax(1, m_count);
}

void VimMode::searchWord(bool forward)
{
    // Get word under cursor
    QTextCursor cursor = m_editor->textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    QString word = cursor.selectedText();
    
    if (word.isEmpty()) {
        emit statusMessage("No word under cursor");
        return;
    }
    
    m_searchPattern = "\\b" + QRegularExpression::escape(word) + "\\b";
    m_searchForward = forward;
    
    searchNext(forward);
}

void VimMode::searchNext(bool forward)
{
    if (m_searchPattern.isEmpty()) {
        emit statusMessage("No previous search");
        return;
    }
    
    QTextCursor cursor = m_editor->textCursor();
    QString text = m_editor->toPlainText();
    QRegularExpression regex(m_searchPattern);
    
    bool actualForward = (forward == m_searchForward); // Account for N vs n
    
    if (actualForward) {
        QRegularExpressionMatch match = regex.match(text, cursor.position() + 1);
        if (match.hasMatch()) {
            cursor.setPosition(match.capturedStart());
            m_editor->setTextCursor(cursor);
        } else {
            // Wrap around
            match = regex.match(text, 0);
            if (match.hasMatch()) {
                cursor.setPosition(match.capturedStart());
                m_editor->setTextCursor(cursor);
                emit statusMessage("Search wrapped");
            } else {
                emit statusMessage("Pattern not found");
            }
        }
    } else {
        // Search backward
        int lastMatch = -1;
        QRegularExpressionMatchIterator it = regex.globalMatch(text.left(cursor.position()));
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            lastMatch = m.capturedStart();
        }
        
        if (lastMatch >= 0) {
            cursor.setPosition(lastMatch);
            m_editor->setTextCursor(cursor);
        } else {
            // Wrap around
            it = regex.globalMatch(text);
            while (it.hasNext()) {
                QRegularExpressionMatch m = it.next();
                lastMatch = m.capturedStart();
            }
            if (lastMatch >= 0) {
                cursor.setPosition(lastMatch);
                m_editor->setTextCursor(cursor);
                emit statusMessage("Search wrapped");
            } else {
                emit statusMessage("Pattern not found");
            }
        }
    }
}

void VimMode::scrollLines(int lines)
{
    QScrollBar* vbar = m_editor->verticalScrollBar();
    int lineHeight = m_editor->cursorRect().height();
    if (lineHeight <= 0)
        lineHeight = m_editor->fontMetrics().height();
    vbar->setValue(vbar->value() + lines * lineHeight);
}

void VimMode::indentText(VimMotion motion, int count, bool indent)
{
    QTextCursor cursor = m_editor->textCursor();
    int startPos = cursor.position();
    
    executeMotion(motion, count);
    
    cursor = m_editor->textCursor();
    int endPos = cursor.position();
    
    cursor.setPosition(qMin(startPos, endPos));
    cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);
    
    // Get selected lines and indent/unindent
    int startBlock = m_editor->document()->findBlock(qMin(startPos, endPos)).blockNumber();
    int endBlock = m_editor->document()->findBlock(qMax(startPos, endPos)).blockNumber();
    
    cursor.beginEditBlock();
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock block = m_editor->document()->findBlockByNumber(i);
        QTextCursor lineCursor(block);
        lineCursor.movePosition(QTextCursor::StartOfLine);
        
        if (indent) {
            lineCursor.insertText("    ");
        } else {
            QString line = block.text();
            if (line.startsWith("    ")) {
                lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
                lineCursor.removeSelectedText();
            } else if (line.startsWith("\t")) {
                lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
                lineCursor.removeSelectedText();
            }
        }
    }
    cursor.endEditBlock();
    
    cursor.setPosition(startPos);
    m_editor->setTextCursor(cursor);
}

void VimMode::toggleCase(VimMotion motion, int count)
{
    QTextCursor cursor = m_editor->textCursor();
    int startPos = cursor.position();
    
    executeMotion(motion, count);
    
    cursor = m_editor->textCursor();
    int endPos = cursor.position();
    
    cursor.setPosition(qMin(startPos, endPos));
    cursor.setPosition(qMax(startPos, endPos), QTextCursor::KeepAnchor);
    
    QString text = cursor.selectedText();
    QString result;
    for (const QChar& c : text) {
        if (c.isLower()) {
            result += c.toUpper();
        } else if (c.isUpper()) {
            result += c.toLower();
        } else {
            result += c;
        }
    }
    
    cursor.insertText(result);
    cursor.setPosition(qMin(startPos, endPos));
    m_editor->setTextCursor(cursor);
}

bool VimMode::selectTextObject(VimTextObject textObj)
{
    QTextCursor cursor = m_editor->textCursor();
    int pos = cursor.position();
    QString text = m_editor->toPlainText();
    
    QChar openChar, closeChar;
    bool isQuote = false;
    bool inner = false;
    
    switch (textObj) {
    case VimTextObject::InnerWord:
    case VimTextObject::AroundWord:
        cursor.select(QTextCursor::WordUnderCursor);
        if (textObj == VimTextObject::AroundWord) {
            // Include trailing space if any
            int end = cursor.selectionEnd();
            if (end < text.length() && text[end].isSpace()) {
                cursor.setPosition(cursor.selectionStart());
                cursor.setPosition(end + 1, QTextCursor::KeepAnchor);
            }
        }
        m_editor->setTextCursor(cursor);
        return true;
        
    case VimTextObject::InnerParen:
    case VimTextObject::AroundParen:
        openChar = '('; closeChar = ')';
        inner = (textObj == VimTextObject::InnerParen);
        break;
    case VimTextObject::InnerBracket:
    case VimTextObject::AroundBracket:
        openChar = '['; closeChar = ']';
        inner = (textObj == VimTextObject::InnerBracket);
        break;
    case VimTextObject::InnerBrace:
    case VimTextObject::AroundBrace:
        openChar = '{'; closeChar = '}';
        inner = (textObj == VimTextObject::InnerBrace);
        break;
    case VimTextObject::InnerAngle:
    case VimTextObject::AroundAngle:
        openChar = '<'; closeChar = '>';
        inner = (textObj == VimTextObject::InnerAngle);
        break;
    case VimTextObject::InnerQuote:
    case VimTextObject::AroundQuote:
        openChar = closeChar = '"';
        isQuote = true;
        inner = (textObj == VimTextObject::InnerQuote);
        break;
    case VimTextObject::InnerSingleQuote:
    case VimTextObject::AroundSingleQuote:
        openChar = closeChar = '\'';
        isQuote = true;
        inner = (textObj == VimTextObject::InnerSingleQuote);
        break;
    case VimTextObject::InnerBacktick:
    case VimTextObject::AroundBacktick:
        openChar = closeChar = '`';
        isQuote = true;
        inner = (textObj == VimTextObject::InnerBacktick);
        break;
    default:
        return false;
    }
    
    if (isQuote) {
        // Find quotes on current line
        QString line = cursor.block().text();
        int col = cursor.positionInBlock();
        int lineStart = cursor.position() - col;
        
        int openPos = -1, closePos = -1;
        bool foundOpen = false;
        
        for (int i = 0; i < line.length(); ++i) {
            if (line[i] == openChar) {
                if (!foundOpen) {
                    if (i <= col) {
                        openPos = i;
                        foundOpen = true;
                    }
                } else {
                    closePos = i;
                    if (i >= col) break;
                    // Reset and look for next pair
                    openPos = -1;
                    closePos = -1;
                    foundOpen = false;
                }
            }
        }
        
        if (openPos >= 0 && closePos > openPos) {
            int start = inner ? openPos + 1 : openPos;
            int end = inner ? closePos : closePos + 1;
            cursor.setPosition(lineStart + start);
            cursor.setPosition(lineStart + end, QTextCursor::KeepAnchor);
            m_editor->setTextCursor(cursor);
            return true;
        }
    } else {
        // Find matching brackets
        int openPos = -1;
        int depth = 0;
        
        // Search backward for opening bracket
        for (int i = pos; i >= 0; --i) {
            if (text[i] == closeChar) depth++;
            else if (text[i] == openChar) {
                if (depth == 0) {
                    openPos = i;
                    break;
                }
                depth--;
            }
        }
        
        if (openPos >= 0) {
            // Search forward for closing bracket
            depth = 1;
            for (int i = openPos + 1; i < text.length(); ++i) {
                if (text[i] == openChar) depth++;
                else if (text[i] == closeChar) {
                    depth--;
                    if (depth == 0) {
                        int start = inner ? openPos + 1 : openPos;
                        int end = inner ? i : i + 1;
                        cursor.setPosition(start);
                        cursor.setPosition(end, QTextCursor::KeepAnchor);
                        m_editor->setTextCursor(cursor);
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

void VimMode::executeOperatorOnTextObject(VimOperator op, VimTextObject textObj)
{
    if (!selectTextObject(textObj)) {
        return;
    }
    
    QTextCursor cursor = m_editor->textCursor();
    m_registerBuffer = cursor.selectedText();
    
    switch (op) {
    case VimOperator::Delete:
        cursor.removeSelectedText();
        m_editor->setTextCursor(cursor);
        break;
    case VimOperator::Change:
        cursor.removeSelectedText();
        m_editor->setTextCursor(cursor);
        setMode(VimEditMode::Insert);
        break;
    case VimOperator::Yank:
        cursor.clearSelection();
        m_editor->setTextCursor(cursor);
        emit statusMessage("Yanked");
        break;
    default:
        break;
    }
}
