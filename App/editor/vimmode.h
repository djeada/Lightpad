#ifndef VIMMODE_H
#define VIMMODE_H

#include <QKeyEvent>
#include <QMap>
#include <QObject>
#include <QPlainTextEdit>
#include <QString>
#include <QStringList>
#include <QVector>

enum class VimEditMode {
  Normal,
  Insert,
  Visual,
  VisualLine,
  VisualBlock,
  Command,
  Replace
};

enum class VimMotion {
  None,
  Left,
  Right,
  Up,
  Down,
  WordForward,
  WordBack,
  WordEnd,
  WORDForward,
  WORDBack,
  WORDEnd,
  LineStart,
  LineEnd,
  FirstNonSpace,
  FileStart,
  FileEnd,
  PageUp,
  PageDown,
  HalfPageUp,
  HalfPageDown,
  FullPageUp,
  FullPageDown,
  MatchingBrace,
  FindChar,
  FindCharBack,
  ToChar,
  ToCharBack,
  NextParagraph,
  PrevParagraph,
  NextSentence,
  PrevSentence,
  SearchNext,
  SearchPrev,
  WordUnderCursor,
  WordUnderCursorBack,
  ScreenTop,
  ScreenMiddle,
  ScreenBottom,
  ColumnZero
};

enum class VimOperator {
  None,
  Delete,
  Change,
  Yank,
  Indent,
  Unindent,
  Format,
  ToggleCase,
  Lowercase,
  Uppercase
};

enum class VimTextObject {
  None,
  InnerWord,
  AroundWord,
  InnerWORD,
  AroundWORD,
  InnerParen,
  AroundParen,
  InnerBracket,
  AroundBracket,
  InnerBrace,
  AroundBrace,
  InnerAngle,
  AroundAngle,
  InnerQuote,
  AroundQuote,
  InnerSingleQuote,
  AroundSingleQuote,
  InnerBacktick,
  AroundBacktick,
  InnerParagraph,
  AroundParagraph,
  InnerSentence,
  AroundSentence,
  InnerTag,
  AroundTag
};

struct VimRegister {
  QString content;
  bool linewise = false;
};

struct VimChangeRecord {
  QList<QKeyEvent *> keys;
  QString insertedText;
  VimEditMode entryMode = VimEditMode::Normal;
  int count = 1;
};

class VimMode : public QObject {
  Q_OBJECT

public:
  explicit VimMode(QPlainTextEdit *editor, QObject *parent = nullptr);
  ~VimMode();

  void setEnabled(bool enabled);

  bool isEnabled() const;

  VimEditMode mode() const;

  QString modeName() const;

  QString commandBuffer() const;

  QString pendingKeys() const;

  bool isRecordingMacro() const;
  QChar macroRegister() const;

  QString registerContent(QChar reg) const;

  QString searchPattern() const;
  void setSearchPattern(const QString &pattern);

  bool processKeyEvent(QKeyEvent *event);

signals:

  void modeChanged(VimEditMode mode);

  void statusMessage(const QString &message);

  void commandExecuted(const QString &command);

  void commandBufferChanged(const QString &buffer);

  void pendingKeysChanged(const QString &keys);

  void macroRecordingChanged(bool recording, QChar reg);

  void searchHighlightRequested(const QString &pattern, bool enabled);

  void registerContentsChanged();

private:
  bool handleNormalMode(QKeyEvent *event);
  bool handleInsertMode(QKeyEvent *event);
  bool handleVisualMode(QKeyEvent *event);
  bool handleCommandMode(QKeyEvent *event);
  bool handleReplaceMode(QKeyEvent *event);

  void setMode(VimEditMode mode);
  void executeMotion(VimMotion motion, int count = 1,
                     QTextCursor::MoveMode moveMode = QTextCursor::MoveAnchor);
  void executeOperator(VimOperator op, VimMotion motion, int count = 1);
  void executeOperatorOnTextObject(VimOperator op, VimTextObject textObj);
  void executeCommand(const QString &command);

  void moveCursor(QTextCursor::MoveOperation op, int count = 1);
  void moveCursorWord(bool forward);
  void moveCursorWORD(bool forward);
  void moveCursorWORDEnd();
  void moveCursorToChar(QChar ch, bool before, bool backward = false);
  bool moveCursorToMatchingBrace();
  void moveCursorToParagraph(bool forward);
  void moveCursorToSentence(bool forward);
  void moveCursorToScreenLine(int which);

  void deleteText(VimMotion motion, int count = 1);
  void yankText(VimMotion motion, int count = 1);
  void changeText(VimMotion motion, int count = 1);
  void indentText(VimMotion motion, int count = 1, bool indent = true);
  void toggleCase(VimMotion motion, int count = 1);
  void lowercaseText(VimMotion motion, int count = 1);
  void uppercaseText(VimMotion motion, int count = 1);

  bool selectTextObject(VimTextObject textObj);

  void insertNewLine(bool above);
  void joinLines(int count = 1);
  void replaceChar(QChar ch);

  void setMark(QChar mark);
  bool jumpToMark(QChar mark);

  // Dot-repeat
  void repeatLastChange();
  void beginChangeRecording(int count);
  void endChangeRecording();

  // Registers
  void setRegister(QChar reg, const QString &text, bool linewise = false);
  VimRegister getRegister(QChar reg) const;
  void pushDeleteHistory(const QString &text, bool linewise);
  void yankToRegister(const QString &text, bool linewise = false);
  void deleteToRegister(const QString &text, bool linewise = false);
  void pasteFromRegister(QChar reg, bool after);

  // Macros
  void startMacroRecording(QChar reg);
  void stopMacroRecording();
  void playbackMacro(QChar reg, int count = 1);

  // Increment/Decrement
  void incrementNumber(int delta);

  // Search
  void searchWord(bool forward);
  void searchNext(bool forward);
  void scrollLines(int lines);
  void clearSearchHighlight();

  // Visual mode helpers
  void visualIndent(bool indent);
  void visualToggleCase();
  void visualLowercase();
  void visualUppercase();
  void visualJoinLines();

  // Last insert tracking
  void trackInsertPosition();

  // g commands
  bool handleGPrefix(QKeyEvent *event, int count);

  // Pending key display
  void updatePendingKeys();

  QPlainTextEdit *m_editor;
  bool m_enabled;
  VimEditMode m_mode;
  QString m_commandBuffer;
  VimOperator m_pendingOperator;
  int m_count;
  QChar m_findChar;
  bool m_findCharBefore;
  bool m_findCharBackward;

  QString m_searchPattern;
  bool m_searchForward;
  bool m_searchHighlightActive;

  QMap<QChar, int> m_marks;

  // Named registers: a-z, "+, "0, "1-"9, "_, "., ""
  QMap<QChar, VimRegister> m_registers;
  QChar m_pendingRegister;

  // Delete history ring ("1-"9)
  QVector<VimRegister> m_deleteHistory;

  // Dot-repeat
  struct ReplayableChange {
    QVector<int> keyCodes;
    QVector<Qt::KeyboardModifiers> keyMods;
    QStringList keyTexts;
    int count = 1;
  };
  ReplayableChange m_lastReplayable;
  bool m_recording;
  QVector<int> m_recordKeyCodes;
  QVector<Qt::KeyboardModifiers> m_recordKeyMods;
  QStringList m_recordKeyTexts;
  int m_recordCount;
  bool m_replaying;

  // Macros
  bool m_macroRecording;
  QChar m_macroRegister;
  QVector<int> m_macroKeyCodes;
  QVector<Qt::KeyboardModifiers> m_macroKeyMods;
  QStringList m_macroKeyTexts;
  QChar m_lastMacroRegister;

  // Last insert position for gi
  int m_lastInsertPosition;

  // Last visual selection for gv
  int m_lastVisualStart;
  int m_lastVisualEnd;
  VimEditMode m_lastVisualMode;

  // Insert/replace undo grouping — groups all edits into one undo step
  bool m_insertUndoOpen = false;

  QStringList m_commandHistory;
  int m_commandHistoryIndex;
  QString m_commandDraft;
  static const int kMaxCommandHistory = 50;
};

#endif
