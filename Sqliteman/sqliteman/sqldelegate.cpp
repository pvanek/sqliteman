/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QToolButton>
#include <QModelIndex>
#include <QFocusEvent>

#include "sqldelegate.h"
#include "utils.h"
#include "multieditdialog.h"


SqlDelegate::SqlDelegate(QObject * parent)
	: QItemDelegate(parent)
{
}

QWidget *SqlDelegate::createEditor(QWidget *parent,
								   const QStyleOptionViewItem &/* option */,
								   const QModelIndex &/* index */) const
{
	SqlDelegateUi *editor = new SqlDelegateUi(parent);
	editor->setFocus();
	editor->setFocusPolicy(Qt::StrongFocus);
	connect(editor, SIGNAL(closeEditor()),
			this, SLOT(editor_closeEditor()));
	return editor;
}

void SqlDelegate::setEditorData(QWidget *editor,
								const QModelIndex &index) const
{
	static_cast<SqlDelegateUi*>(editor)->setSqlData(
								index.model()->data(index,
								Qt::EditRole));
}

void SqlDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
							   const QModelIndex &index) const
{
	SqlDelegateUi *ed = static_cast<SqlDelegateUi*>(editor);
	if (ed->sqlData() != index.model()->data(index, Qt::EditRole))
		model->setData(index, ed->sqlData(), Qt::EditRole);
// 	else
// 		qDebug("ed->sqlData() == index.model()->data(index, Qt::EditRole)");
}

void SqlDelegate::updateEditorGeometry(QWidget *editor,
									   const QStyleOptionViewItem &option,
									   const QModelIndex &/* index */) const
{
	editor->setGeometry(option.rect);
}

void SqlDelegate::editor_closeEditor()
{
	SqlDelegateUi *ed = qobject_cast<SqlDelegateUi*>(sender());
	emit commitData(ed);
	emit closeEditor(ed);
}


SqlDelegateUi::SqlDelegateUi(QWidget * parent)
	: QWidget(parent)
{
	setupUi(this);

	nullButton->setIcon(Utils::getIcon("setnull.png"));
	editButton->setIcon(Utils::getIcon("edit.png"));

	connect(nullButton, SIGNAL(clicked(bool)),
			this, SLOT(nullButton_clicked(bool)));
	connect(editButton, SIGNAL(clicked(bool)),
			this, SLOT(editButton_clicked(bool)));
	connect(lineEdit, SIGNAL(textEdited(const QString &)),
			this, SLOT(lineEdit_textEdited(const QString &)));
}

void SqlDelegateUi::focusInEvent(QFocusEvent *e)
{
	if (e->gotFocus())
		lineEdit->setFocus();
}

void SqlDelegateUi::setSqlData(const QVariant & data)
{
	m_sqlData = data;
	// blob or multiline
	if (data.type() == QVariant::ByteArray
		   || m_sqlData.toString().contains("\n"))
	{
		lineEdit->setDisabled(true);
		lineEdit->setToolTip(tr("Multiline texts can be edited with the enhanced editor only (Ctrl+Shift+E)"));
		editButton_clicked(true);
	}
	lineEdit->setText(data.toString());
}

QVariant SqlDelegateUi::sqlData()
{
	return m_sqlData;
}

void SqlDelegateUi::nullButton_clicked(bool)
{
	lineEdit->setText(QString());
	m_sqlData = QString();
	emit closeEditor();
}

void SqlDelegateUi::editButton_clicked(bool state)
{
	MultiEditDialog * dia = new MultiEditDialog(this);
	qApp->setOverrideCursor(Qt::WaitCursor);
	dia->setData(m_sqlData);
	qApp->restoreOverrideCursor();
	if (dia->exec())
		m_sqlData = dia->data();
	emit closeEditor();
}

void SqlDelegateUi::lineEdit_textEdited(const QString & text)
{
	m_sqlData = text;
}
