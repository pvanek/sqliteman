/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef SQLEDITORWIDGET_H
#define SQLEDITORWIDGET_H

#include <QTextEdit>

class QCompleter;


/*! \brief A customized QTextEdit.
It handles the current line highlighting and max line width mark.
\note SqlEditorWidget is promoted into ui files as a custom widget.
\author Petr Vanek <petr@scribus.info>
*/
class SqlEditorWidget : public QTextEdit
{
	Q_OBJECT

	public:
		SqlEditorWidget(QWidget * parent = 0);

		void setCompletion(bool useCompletion, int minLength);

	private:
		QCompleter *m_completer;
		bool m_useCompleter;
		int m_completerLength;

		void paintEvent(QPaintEvent * e);
		void keyPressEvent(QKeyEvent * e);
		void mousePressEvent(QMouseEvent * e);

	private slots:
		void insertCompletion(const QString&);
};

#endif
