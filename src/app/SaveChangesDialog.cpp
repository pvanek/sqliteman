// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SaveChangesDialog.h"

#include "ui_SaveChangesDialog.h"

#include <QFontDatabase>

namespace sqliteman {

SaveChangesDialog::SaveChangesDialog(const QString &table, const QStringList &statements,
                                     bool inTransaction, QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::SaveChangesDialog>())
{
    m_ui->setupUi(this);
    m_ui->summary->setText(
        (inTransaction
             ? tr("These %n statement(s) will change %1 in the open transaction; commit it "
                  "to keep them.",
                  nullptr, int(statements.size()))
             : tr("These %n statement(s) will change %1 and commit.", nullptr,
                  int(statements.size())))
            .arg(table));
    m_ui->statements->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    QStringList lines;
    for (const QString &s : statements)
        lines << s + QLatin1Char(';');
    m_ui->statements->setPlainText(lines.join(QLatin1Char('\n')));
    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SaveChangesDialog::~SaveChangesDialog() = default;

} // namespace sqliteman
