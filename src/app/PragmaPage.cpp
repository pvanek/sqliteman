// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PragmaPage.h"

#include "Icons.h"
#include "Session.h"
#include "Shortcuts.h"
#include "catalog/Pragmas.h"

#include <QAction>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

namespace sqliteman {

using catalog::PragmaInfo;

namespace {

// A choice is picked from its names, a boolean from ON and OFF.
class ValueDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        const PragmaInfo *p
            = catalog::pragma(index.siblingAtColumn(PragmaPage::Name).data().toString());
        if (p && (p->kind == PragmaInfo::Kind::Choice || p->kind == PragmaInfo::Kind::Boolean)) {
            auto *box = new QComboBox(parent);
            box->addItems(p->kind == PragmaInfo::Kind::Boolean
                              ? QStringList {QStringLiteral("ON"), QStringLiteral("OFF")}
                              : p->choices);
            return box;
        }
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *box = qobject_cast<QComboBox *>(editor)) {
            // "FULL (1)": the name before the number.
            box->setCurrentText(index.data().toString().section(QLatin1Char(' '), 0, 0));
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (auto *box = qobject_cast<QComboBox *>(editor)) {
            model->setData(index, box->currentText());
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }
};

} // namespace

PragmaPage::PragmaPage(Session *session, const QString &schema, QWidget *parent)
    : WorkspacePage(parent), m_session(session),
      m_schema(schema.isEmpty() ? QStringLiteral("main") : schema), m_table(new QTableWidget(this)),
      m_status(new QLabel(this))
{
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({tr("Pragma"), tr("Value"), tr("What it does")});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setItemDelegateForColumn(Value, new ValueDelegate(m_table));
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                             | QAbstractItemView::AnyKeyPressed);
    const auto &list = catalog::pragmas();
    m_table->setRowCount(int(list.size()));
    for (int row = 0; row < int(list.size()); ++row) {
        const PragmaInfo &p = list[std::size_t(row)];
        auto *name = new QTableWidgetItem(p.name);
        name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_table->setItem(row, Name, name);
        auto *value = new QTableWidgetItem;
        if (p.kind == PragmaInfo::Kind::ReadOnly)
            value->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_table->setItem(row, Value, value);
        auto *description = new QTableWidgetItem(p.description);
        description->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        description->setToolTip(p.description);
        m_table->setItem(row, Description, description);
    }
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_filling || item->column() != Value)
            return;
        setValue(m_table->item(item->row(), Name)->text(), item->text());
    });

    auto *toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    QAction *reload
        = toolbar->addAction(Icons::refresh(), tr("Refresh"), this, &PragmaPage::refresh);
    reload->setShortcuts(Shortcuts::refresh());
    reload->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(reload);
    m_status->setTextFormat(Qt::PlainText);
    toolbar->addWidget(m_status);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    layout->addWidget(m_table);
    if (m_session)
        setToolTip(m_session->path());
    this->refresh();
}

QString PragmaPage::title() const
{
    const QString name = m_session ? m_session->name() : QString();
    return tr("Pragmas of %1").arg(m_schema == QLatin1String("main") ? name : m_schema);
}

QIcon PragmaPage::icon() const
{
    return Icons::system();
}

QString PragmaPage::value(const QString &pragma) const
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, Name)->text() == pragma)
            return m_table->item(row, Value)->text();
    }
    return {};
}

void PragmaPage::refresh()
{
    if (!m_session || !m_session->runner())
        return;
    m_session->runner()->run(
        catalog::readPragmasQuery(m_schema), this, [this](const sqlite::QueryOutcome &outcome) {
            m_filling = true;
            const auto &list = catalog::pragmas();
            for (std::size_t i = 0; i < list.size() && i < outcome.results.size(); ++i) {
                const sqlite::Result &r = outcome.results[i];
                const QString raw = r.rowCount() > 0 ? r.valueAt(0, 0).toString() : QString();
                m_table->item(int(i), Value)->setText(catalog::displayValue(list[i], raw));
            }
            m_filling = false;
            m_table->resizeColumnToContents(Name);
            m_table->resizeColumnToContents(Value);
            if (!outcome.ok())
                m_status->setText(outcome.error);
            Q_EMIT loaded();
        });
}

void PragmaPage::setValue(const QString &pragma, const QString &value)
{
    const PragmaInfo *p = catalog::pragma(pragma);
    if (!p || !m_session || !m_session->runner())
        return;
    const QByteArray sql = catalog::setPragmaStatement(m_schema, *p, value);
    if (sql.isEmpty())
        return;
    m_session->runner()->run(sql, this, [this, sql](const sqlite::QueryOutcome &outcome) {
        m_status->setText(outcome.ok() ? QString::fromUtf8(sql) : outcome.error);
        refresh();
    });
}

} // namespace sqliteman
