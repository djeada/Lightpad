#ifndef GITUI_H
#define GITUI_H

class QAbstractItemView;
class QBoxLayout;
class QString;
class QWidget;
class Theme;

namespace GitUi {

void wrapActions(QBoxLayout *row);
void polish(QWidget *surface, const Theme &theme);
void emptyState(QAbstractItemView *view, const QString &message,
                const Theme &theme);
} // namespace GitUi

#endif
