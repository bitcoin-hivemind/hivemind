/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "authorview.h"
#include "ui_authorview.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QMessageBox>

AuthorView::AuthorView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AuthorView)
{
    ui->setupUi(this);

    // Setup model & author pending table
    pendingTableView = new QTableView(this);
    pendingTableView->horizontalHeader()->setStretchLastSection(true);

#if QT_VERSION < 0x050000
    pendingTableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    pendingTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    pendingTableModel = new AuthorPendingTableModel(this);
    pendingTableView->setModel(pendingTableModel);
    pendingTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->frameLeft->layout()->addWidget(pendingTableView);

    // Setup signals
    connect(this, SIGNAL(newPendingCombo(json_spirit::Array)),
            pendingTableModel, SLOT(receivePendingCombo(json_spirit::Array)));

    connect(this, SIGNAL(newPendingDecision(json_spirit::Array)),
            pendingTableModel, SLOT(receivePendingDecision(json_spirit::Array)));

    connect(this, SIGNAL(newPendingDecisionMarket(json_spirit::Array)),
            pendingTableModel, SLOT(receivePendingDecisionMarket(json_spirit::Array)));

    connect(pendingTableView, SIGNAL(doubleClicked(QModelIndex)),
            pendingTableModel, SLOT(on_tableView_doubleClicked(QModelIndex)));

    connect(pendingTableModel, SIGNAL(finalizeError(QString)),
            this, SLOT(on_finalizeError(QString)));

    connect(pendingTableModel, SIGNAL(finalizeComplete()),
            this, SLOT(on_finalizeComplete()));

    connect(pendingTableModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(calculateFees()));
}

AuthorView::~AuthorView()
{
    delete ui;
}

void AuthorView::on_pushButtonCreateCombo_clicked()
{
    comboCreationWidget = new ComboCreationWidget(this);

    QHBoxLayout *hbox = new QHBoxLayout(this);
    hbox->addWidget(comboCreationWidget);

    // Pass arrays to model
    connect(comboCreationWidget, SIGNAL(newComboArray(json_spirit::Array)),
            this, SIGNAL(newPendingCombo(json_spirit::Array)));

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Create Combo (Decision & Market)");
    dialog->setLayout(hbox);
    dialog->show();
}

void AuthorView::on_pushButtonCreateDecision_clicked()
{
    decisionCreationWidget = new DecisionCreationWidget(this);
    QHBoxLayout *hbox = new QHBoxLayout(this);
    hbox->addWidget(decisionCreationWidget);

    // Pass array to model
    connect(decisionCreationWidget, SIGNAL(newDecisionArray(json_spirit::Array)),
            this, SIGNAL(newPendingDecision(json_spirit::Array)));

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Create Decision");
    dialog->setLayout(hbox);
    dialog->show();
}

void AuthorView::on_pushButtonCreateMarket_clicked()
{
    decisionMarketCreationWidget = new DecisionMarketCreationWidget(this);
    QHBoxLayout *hbox = new QHBoxLayout(this);
    hbox->addWidget(decisionMarketCreationWidget);

    // Pass array to model
    connect(decisionMarketCreationWidget, SIGNAL(newDecisionMarketArray(json_spirit::Array)),
            this, SIGNAL(newPendingDecisionMarket(json_spirit::Array)));

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Create Market");
    dialog->setLayout(hbox);
    dialog->show();
}

void AuthorView::on_pushButtonFinalize_clicked()
{
    pendingTableModel->finalize();
    calculateFees();
}

void AuthorView::on_finalizeError(const QString &errorMessage)
{
    QMessageBox errorMessageBox;
    errorMessageBox.setWindowTitle("Error finalizing creations!");
    errorMessageBox.setText(errorMessage);
    errorMessageBox.setStandardButtons(QMessageBox::Ok);
    errorMessageBox.setDefaultButton(QMessageBox::Ok);
    errorMessageBox.exec();
}

void AuthorView::calculateFees()
{
    double fees = 0.0;
    for (int i = 0; i < pendingTableModel->rowCount(); i++) {
        fees += 0.02;
    }

    ui->labelTotalValue->setText(QString::number(fees));
}
