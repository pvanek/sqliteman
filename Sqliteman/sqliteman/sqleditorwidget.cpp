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

#include "sqleditorwidget.h"
#include "preferences.h"
#include "sqlkeywords.h"


SqlEditorWidget::SqlEditorWidget(QWidget * parent)
	: QTextEdit(parent)
{
	m_completer = new QCompleter(this);
	m_completer->setModel(new QStringListModel(sqlKeywords(), m_completer));
	m_completer->setWidget(this);
	m_completer->setCaseSensitivity(Qt::CaseInsensitive);

	ensureCursorVisible();
	m_prefs = Preferences::instance();
	setCompletion(m_prefs->codeCompletion(),
				  m_prefs->codeCompletionLength());
	setShortcuts(m_prefs->useShortcuts(),
				 m_prefs->shortcuts());

	connect(m_completer, SIGNAL(activated(const QString&)),
			this, SLOT(insertCompletion(const QString&)));
}

void SqlEditorWidget::setCompletion(bool useCompletion, int minLength)
{
	m_useCompleter = useCompletion;
	m_completerLength = minLength;
}

void SqlEditorWidget::setShortcuts(bool useShortcuts, QMap<QString,QVariant> shortcuts)
{
	m_useShortcuts = useShortcuts;
	m_shortcuts = shortcuts;
}

void SqlEditorWidget::paintEvent(QPaintEvent* e)
{
	QPainter p(viewport());

	// highlight current line
	if (m_prefs->activeHighlighting())
	{
		QRect currLine = cursorRect();
		currLine.setX(0);
		currLine.setWidth(viewport()->width());
		p.fillRect(currLine, QBrush(m_prefs->activeHighlightColor()));
	}

	// draw "max line lenght" mark
	if (m_prefs->textWidthMark())
	{
		QFont fTmp(m_prefs->sqlFont());
		fTmp.setPointSize(m_prefs->sqlFontSize());
		QFontMetrics fm(fTmp);
		int xpos = fm.width(QString(m_prefs->textWidthMarkSize()-1, 'X'));
		const QPen prevPen = p.pen();
		p.setPen(QPen(Qt::darkGreen, 1.0, Qt::DotLine));
		p.drawLine(xpos, 0, xpos, viewport()->height() - 1);
		p.setPen(prevPen);
	}
	p.end();

	QTextEdit::paintEvent(e);
}

void SqlEditorWidget::keyPressEvent(QKeyEvent * e)
{
	viewport()->update();
	ensureCursorVisible();

	if (m_useCompleter && m_completer->popup()->isVisible())
	{
		// The following keys are forwarded by the completer to the widget
		switch (e->key())
		{
			case Qt::Key_Enter:
			case Qt::Key_Return:
			case Qt::Key_Escape:
			case Qt::Key_Tab:
			case Qt::Key_Backtab:
					e->ignore();
					return; // let the completer do default behavior
			default:
				break;
		}
	}

	// handle editor shortcuts with TAB
	if (m_useShortcuts && e->key() == Qt::Key_Tab)
	{
		QTextCursor tc = textCursor();
		tc.select(QTextCursor::WordUnderCursor);
		QString currWord(tc.selectedText());
		if (m_shortcuts.contains(currWord))
		{
			QString val(m_shortcuts.value(currWord).toString());
			tc.removeSelectedText();
			tc.insertText(val);
			setTextCursor(tc);
			return;
		}
	}

	QTextEdit::keyPressEvent(e);

	if (!m_useCompleter)
		return;

	// completion popup handling
	const bool ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
	if (!m_completer || (ctrlOrShift && e->text().isEmpty()))
		return;

	static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="); // end of word
	bool hasModifier = (e->modifiers() != Qt::NoModifier) && !ctrlOrShift;
	QTextCursor tc = textCursor();
	tc.select(QTextCursor::WordUnderCursor);
	QString completionPrefix(tc.selectedText());

	if (hasModifier || e->text().isEmpty()
		|| completionPrefix.length() < m_completerLength
		|| eow.contains(e->text().right(1))
	   )
	{
		m_completer->popup()->hide();
		return;
	}

	if (completionPrefix != m_completer->completionPrefix())
	{
		m_completer->setCompletionPrefix(completionPrefix);
		m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
	}
	QRect cr = cursorRect();
	cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
				+ m_completer->popup()->verticalScrollBar()->sizeHint().width());
	m_completer->complete(cr);
}

void SqlEditorWidget::mousePressEvent(QMouseEvent * e)
{
	viewport()->update();
// 	ensureCursorVisible(); // mouse wheel-click jump fix
	QTextEdit::mousePressEvent(e);
}

void SqlEditorWidget::insertCompletion(const QString& str)
{
	QTextCursor cur(textCursor());
	int offset = str.length() - m_completer->completionPrefix().length();
	cur.movePosition(QTextCursor::Left);
	cur.movePosition(QTextCursor::EndOfWord);
	cur.insertText(str.right(offset));
	setTextCursor(cur);
}
