/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QPainter>
#include <QScrollBar>
#include <QCompleter>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QStringListModel>

#include <qscilexersql.h>
#include <qsciapis.h>

#include "sqleditorwidget.h"
#include "preferences.h"
#include "sqlkeywords.h"


SqlEditorWidget::SqlEditorWidget(QWidget * parent)
	: QsciScintilla(parent),
	  m_prevCurrentLine(0)
{
	m_prefs = Preferences::instance();

	setMarginLineNumbers(0, true);
	setBraceMatching(SloppyBraceMatch);
	setAutoIndent(true);

	QsciLexerSQL * lexer = new QsciLexerSQL();
	lexer->setFont(m_prefs->sqlFont());
	QsciAPIs * api = new QsciAPIs(lexer);
	foreach(QString i, sqlKeywords())
		api->add(i);
	setAutoCompletionSource(AcsAll);
	setLexer(lexer);
	setUtf8(true);
	setAutoCompletionReplaceWord(true);
	m_currentLineHandle = markerDefine(QsciScintilla::Background);
	cursorPositionChanged(0, 0);

	connect(this, SIGNAL(linesChanged()),
			this, SLOT(linesChanged()));
	connect(this, SIGNAL(cursorPositionChanged(int, int)),
			this, SLOT(cursorPositionChanged(int, int)));
}

void SqlEditorWidget::keyPressEvent(QKeyEvent * e)
{
	// handle editor shortcuts with TAB
	// It uses qscintilla lowlevel API to handle "word unde cursor"
	if (m_prefs->useShortcuts() && e->key() == Qt::Key_Tab)
	{
		int pos = SendScintilla(SCI_GETCURRENTPOS);
		int start = SendScintilla(SCI_WORDSTARTPOSITION, pos,true);
		int end = SendScintilla(SCI_WORDENDPOSITION, pos, true);
		SendScintilla(SCI_SETSELECTIONSTART, start, true);
		SendScintilla(SCI_SETSELECTIONEND, end, true);
		QString key(selectedText());
		bool done = false;
		if (m_prefs->shortcuts().contains(key))
		{
			removeSelectedText();
			insert(m_prefs->shortcuts().value(key).toString());
			SendScintilla(SCI_SETCURRENTPOS,
						  SendScintilla(SCI_GETCURRENTPOS) +
								  m_prefs->shortcuts().value(key).toString().length());
			done = true;
		}
		pos = SendScintilla(SCI_GETCURRENTPOS);
		SendScintilla(SCI_SETSELECTIONSTART, pos,true);
		SendScintilla(SCI_SETSELECTIONEND, pos, true);

		if (done)
			return;
	}

	// Manual popup with code completion
	if (m_prefs->codeCompletion()
		   && (e->modifiers() & Qt::ControlModifier)
		   && e->key() == Qt::Key_Space)
	{
		autoCompleteFromAll();
		return;
	}

	QsciScintilla::keyPressEvent(e);
}

void SqlEditorWidget::linesChanged()
{
	int x = QString::number(lines()).length();
	if (x==0) ++x;
	setMarginWidth(0, QString().fill('0', x));
}

void SqlEditorWidget::cursorPositionChanged(int line, int)
{
	markerDelete(m_prevCurrentLine, m_currentLineHandle);
	markerAdd(line, m_currentLineHandle);
	m_prevCurrentLine = line;
}

void SqlEditorWidget::prefsChanged()
{
	lexer()->setFont(m_prefs->sqlFont());
	setFont(m_prefs->sqlFont());

	setAutoCompletionThreshold(m_prefs->codeCompletion() ?
								m_prefs->codeCompletionLength() : -1
							  );

	if (m_prefs->textWidthMark())
	{
		setEdgeColumn(m_prefs->textWidthMarkSize());
		setEdgeColor(Qt::gray);
		setEdgeMode(QsciScintilla::EdgeLine);
	}
	else
		setEdgeMode(QsciScintilla::EdgeNone);

	setMarkerBackgroundColor(m_prefs->activeHighlighting() ?
								m_prefs->activeHighlightColor() : paper());
}
