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
#include "ui_multieditdialog.h"


SqlDelegate::SqlDelegate(QObject * parent)
	: QItemDelegate(parent)
{
}

QWidget *SqlDelegate::createEditor(QWidget *parent,
								   const QStyleOptionViewItem &/* option */,
								   const QModelIndex &/* index */) const
{
	SqlDelegateUi *editor = new SqlDelegateUi(parent);
	editor->setFocus(Qt::OtherFocusReason);
	editor->lineEdit->setFocus(Qt::OtherFocusReason);
	return editor;
}

void SqlDelegate::setEditorData(QWidget *editor,
								const QModelIndex &index) const
{
	QString value = index.model()->data(index, Qt::EditRole).toString();
	static_cast<SqlDelegateUi*>(editor)->setSqlData(value);
}

void SqlDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
							   const QModelIndex &index) const
{
	SqlDelegateUi *ed = static_cast<SqlDelegateUi*>(editor);
	model->setData(index, ed->sqlData(), Qt::EditRole);
}

void SqlDelegate::updateEditorGeometry(QWidget *editor,
									   const QStyleOptionViewItem &option,
									   const QModelIndex &/* index */) const
{
	editor->setGeometry(option.rect);
}


SqlDelegateUi::SqlDelegateUi(QWidget * parent)
	: QWidget(parent)
{
	setupUi(this);

	nullButton->setIcon(getIcon("setnull.png"));
	editButton->setIcon(getIcon("edit.png"));

	connect(nullButton, SIGNAL(clicked(bool)), this, SLOT(nullButton_clicked(bool)));
	connect(editButton, SIGNAL(clicked(bool)), this, SLOT(editButton_clicked(bool)));
	connect(lineEdit, SIGNAL(textEdited(const QString &)),
			this, SLOT(lineEdit_textEdited(const QString &)));
}

void SqlDelegateUi::setSqlData(const QString & data)
{
	m_sqlData = data;
	if (m_sqlData.contains("\n"))
	{
		lineEdit->setDisabled(true);
		editButton->setFocus(Qt::OtherFocusReason);
	}
	lineEdit->setText(data);
}

QString SqlDelegateUi::sqlData()
{
	return m_sqlData;
}

void SqlDelegateUi::nullButton_clicked(bool)
{
	lineEdit->setText(QString());
	m_sqlData = QString();
}

void SqlDelegateUi::editButton_clicked(bool)
{
	QDialog * dia = new QDialog(this);
	Ui::MultiEditDialog ui;
	ui.setupUi(dia);
	ui.textEdit->setPlainText(m_sqlData);
	if (dia->exec())
		setSqlData(ui.textEdit->toPlainText());
}

void SqlDelegateUi::lineEdit_textEdited(const QString & text)
{
	m_sqlData = text;
}
