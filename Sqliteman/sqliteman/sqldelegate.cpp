/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QToolButton>
#include <QModelIndex>

#include "sqldelegate.h"
#include "utils.h"
#include "multieditdialog.h"

// #include <QtDebug>
SqlDelegate::SqlDelegate(QObject * parent)
	: QItemDelegate(parent)
{
// 	qDebug() << "SqlDelegate::SqlDelegate(QObject * parent)";
}

QWidget *SqlDelegate::createEditor(QWidget *parent,
								   const QStyleOptionViewItem &/* option */,
								   const QModelIndex &/* index */) const
{
	SqlDelegateUi *editor = new SqlDelegateUi(parent);
	editor->setFocus(/*Qt::OtherFocusReason*/);
	editor->setFocusPolicy(Qt::StrongFocus);
	connect(editor, SIGNAL(closeEditor(QWidget *)), this, SLOT(editor_closeEditor(QWidget *)));
// 	qDebug() << "createEditor";
	return editor;
}

void SqlDelegate::setEditorData(QWidget *editor,
								const QModelIndex &index) const
{
	static_cast<SqlDelegateUi*>(editor)->setSqlData(index.model()->data(index, Qt::EditRole));
// 	static_cast<SqlDelegateUi*>(editor)->lineEdit->setFocus(Qt::OtherFocusReason);
// 	static_cast<SqlDelegateUi*>(editor)->lineEdit->setFocusPolicy(Qt::StrongFocus);
// 	qDebug() << "setEditorData";
}

void SqlDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
							   const QModelIndex &index) const
{
	SqlDelegateUi *ed = static_cast<SqlDelegateUi*>(editor);
	model->setData(index, ed->sqlData(), Qt::EditRole);
// 	qDebug() << "setModelData";
}

void SqlDelegate::updateEditorGeometry(QWidget *editor,
									   const QStyleOptionViewItem &option,
									   const QModelIndex &/* index */) const
{
	editor->setGeometry(option.rect);
// 	qDebug() << "updateEditorGeometry";
}

void SqlDelegate::editor_closeEditor(QWidget * editor)
{
// 	qDebug() << "editor_closeEditor";
	emit commitData(editor);
	emit closeEditor(editor, QAbstractItemDelegate::NoHint);
}


SqlDelegateUi::SqlDelegateUi(QWidget * parent)
	: QWidget(parent),
	  m_openEditor(true)
{
	setupUi(this);

	nullButton->setIcon(Utils::getIcon("setnull.png"));
	editButton->setIcon(Utils::getIcon("edit.png"));

	connect(nullButton, SIGNAL(clicked(bool)), this, SLOT(nullButton_clicked(bool)));
	connect(editButton, SIGNAL(clicked(bool)), this, SLOT(editButton_clicked(bool)));
	connect(lineEdit, SIGNAL(textEdited(const QString &)),
			this, SLOT(lineEdit_textEdited(const QString &)));
// 	qDebug() << "SqlDelegateUi::SqlDelegateUi(QWidget * parent)";
}

void SqlDelegateUi::setSqlData(const QVariant & data)
{
	m_sqlData = data;
	// blob or multiline
	if (data.type() == QVariant::ByteArray || m_sqlData.toString().contains("\n"))
	{
		lineEdit->setDisabled(true);
		lineEdit->setToolTip(tr("Multiline texts can be edited by the enhanced editor only (Ctrl+Shift+E)"));
		if (m_openEditor)
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
	emit closeEditor(this);
}

void SqlDelegateUi::editButton_clicked(bool)
{
	MultiEditDialog * dia = new MultiEditDialog(this);
	dia->setData(m_sqlData);
	m_openEditor = false;
	
	if (dia->exec())
		setSqlData(dia->data());
	emit closeEditor(this);
}

void SqlDelegateUi::lineEdit_textEdited(const QString & text)
{
	m_sqlData = text;
}
// #include <QtDebug>
// void SqlDelegateUi::showEvent(QShowEvent * e)
// {
// 	qDebug() << "\n"<<QApplication::focusWidget ();
// 	QWidget::showEvent(e);
// 	qDebug() << "v: " << lineEdit->isVisible();
// // 	lineEdit->setFocus(/*Qt::MouseFocusReason*/);
// // 	lineEdit->setFocusPolicy(Qt::StrongFocus);
// 	lineEdit->setCursorPosition(666);
// // 	lineEdit->selectAll();
// 	qDebug() <<QApplication::focusWidget ();
// }
