/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef SQLEDITORWIDGET_H
#define SQLEDITORWIDGET_H

#include <qsciscintilla.h>

class Preferences;


/*! \brief A customized QTextEdit.
It handles the current line highlighting and max line width mark.
\note SqlEditorWidget is promoted into ui files as a custom widget.
\author Petr Vanek <petr@scribus.info>
*/
class SqlEditorWidget : public QsciScintilla/*QTextEdit*/
{
	Q_OBJECT

	public:
		SqlEditorWidget(QWidget * parent = 0);

	public slots:
		//! \brief Apply new preferences for editor.
		void prefsChanged();

	private:
		Preferences * m_prefs;
		//! \brief A handler for current line highlighting
		int m_currentLineHandle;
		/*! \brief Store current line number to change
		m_currentLineHandle in cursorPositionChanged() slot. */
		int m_prevCurrentLine;

		void keyPressEvent(QKeyEvent * e);

	private slots:
		//! \brief Change the line numbering scope.
		void linesChanged();
		/*! \brief Handle m_currentLineHandle handler to 
		highlight current line. */
		void cursorPositionChanged(int, int);
};

#endif
