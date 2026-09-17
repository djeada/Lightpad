#include "editor/vimmode.h"
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <iostream>
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QFile f(argv[1]);
  f.open(QIODevice::ReadOnly);
  QJsonArray cases = QJsonDocument::fromJson(f.readAll()).array();
  QJsonArray out;
  for (const auto &cv : cases) {
    QJsonObject c = cv.toObject();
    QPlainTextEdit ed;
    ed.resize(600, 400);
    VimMode vim(&ed);
    ed.setPlainText(c["text"].toString());
    QTextCursor cur(ed.document());
    QTextBlock b = ed.document()->findBlockByNumber(c["line"].toInt());
    cur.setPosition(b.position() + c["col"].toInt());
    ed.setTextCursor(cur);
    vim.setEnabled(true);
    vim.feedKeys(c["keys"].toString());
    if (vim.mode() != VimEditMode::Normal || !vim.pendingKeys().isEmpty())
      vim.feedKeys("<Esc>");
    if (vim.mode() != VimEditMode::Normal)
      vim.feedKeys("<Esc>");
    QTextCursor tc = ed.textCursor();
    QJsonObject r;
    r["text"] = ed.toPlainText();
    r["line"] = tc.blockNumber();
    r["col"] = tc.positionInBlock();
    VimRegister reg = vim.registerValue('"');
    r["reg"] = reg.content;
    r["regtype"] = reg.blockwise ? "b" : reg.linewise ? "V" : "v";
    out.append(r);
  }
  QFile o(argv[2]);
  o.open(QIODevice::WriteOnly);
  o.write(QJsonDocument(out).toJson());
  return 0;
}
