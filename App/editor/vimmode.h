#ifndef VIMMODE_H
#define VIMMODE_H

#include <QKeyEvent>
#include <QMap>
#include <QObject>
#include <QPlainTextEdit>
#include <QString>
#include <QStringList>

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
  WordUnderCursorBack
};

enum class VimOperator {
  None,
  Delete,
  Change,
  Yank,
  Indent,
  Unindent,
  Format,
  ToggleCase
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
  AroundBacktick
};

class VimMode : public QObject {
  Q_OBJECT

public:
  explicit VimMode(QPlainTextEdit *editor, QObject *parent = nullptr);
  ~VimMode() = default;

  void setEnabled(bool enabled);

  bool isEnabled() const;

  VimEditMode mode() const;

  QString modeName() const;

  QString commandBuffer() const;

  bool processKeyEvent(QKeyEvent *event);

signals:

  void modeChanged(VimEditMode mode);

  void statusMessage(const QString &message);

  void commandExecuted(const QString &command);

  void commandBufferChanged(const QString &buffer);

private:
  bool handleNormalMode(QKeyEvent *event);
  bool handleInsertMode(QKeyEvent *event);
  bool handleVisualMode(QKeyEvent *event);
  bool handleCommandMode(QKeyEvent *event);
  bool handleReplaceMode(QKeyEvent *event);

  void setMode(VimEditMode mode);
  void executeMotion(VimMotion motion, int count = 1);
  void executeOperator(VimOperator op, VimMotion motion, int count = 1);
  void executeOperatorOnTextObject(VimOperator op, VimTextObject textObj);
  void executeCommand(const QString &command);

  void moveCursor(QTextCursor::MoveOperation op, int count = 1);
  void moveCursorWord(bool forward);
  void moveCursorToChar(QChar ch, bool before, bool backward = false);
  bool moveCursorToMatchingBrace();
  void moveCursorToParagraph(bool forward);
  void moveCursorToSentence(bool forward);

  void deleteText(VimMotion motion, int count = 1);
  void yankText(VimMotion motion, int count = 1);
  void changeText(VimMotion motion, int count = 1);
  void indentText(VimMotion motion, int count = 1, bool indent = true);
  void toggleCase(VimMotion motion, int count = 1);

  bool selectTextObject(VimTextObject textObj);

  void insertNewLine(bool above);
  void joinLines();
  void replaceChar(QChar ch);

  void setMark(QChar mark);
  bool jumpToMark(QChar mark);

  void repeatLastChange();
  void recordChange(const QString &change);

  void searchWord(bool forward);
  void searchNext(bool forward);
  void scrollLines(int lines);

  QPlainTextEdit *m_editor;
  bool m_enabled;
  VimEditMode m_mode;
  QString m_commandBuffer;
  QString m_registerBuffer;
  VimOperator m_pendingOperator;
  int m_count;
  QChar m_findChar;
  bool m_findCharBefore;
  bool m_findCharBackward;

  QString m_searchPattern;
  bool m_searchForward;

  QMap<QChar, int> m_marks;

  QString m_lastChange;
  int m_lastChangeCount;
  bool m_recordingChange;

  QStringList m_commandHistory;
  int m_commandHistoryIndex;
  QString m_commandDraft;
  static const int kMaxCommandHistory = 50;
};

#endif
