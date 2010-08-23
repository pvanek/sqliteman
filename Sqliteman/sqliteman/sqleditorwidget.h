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

        /*! \brief Highlight all occurrences of string s.
        Nothing is processed when s == m_searchText (cache).
        */
        void highlightAllOccurrences(const QString & s,
                                      bool caseSensitive,
                                      bool wholeWords);

	public slots:
		//! \brief Apply new preferences for editor.
		void prefsChanged();

	private:
		Preferences * m_prefs;

        //! Previously searched string
        QString m_searchText;
        //! Highligh all occurrences of m_searchText QScintilla indicator
        int m_searchIndicator;

		void keyPressEvent(QKeyEvent * e);

	private slots:
		//! \brief Change the line numbering scope.
		void linesChanged();
#if 0
		/*! \brief Handle m_currentLineHandle handler to 
		highlight current line. */
		void cursorPositionChanged(int, int);
#endif
};

#endif
