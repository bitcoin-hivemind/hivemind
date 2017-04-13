/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "decisionselectionview.h"
#include "ui_decisionselectionview.h"

#include <QItemSelectionModel>
#include <QModelIndexList>

DecisionSelectionView::DecisionSelectionView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DecisionSelectionView)
{
    ui->setupUi(this);

    // Setup model and decision selection table
    decisionSelectionTable = new QTableView(this);
    decisionSelectionTable->horizontalHeader()->setStretchLastSection(true);

#if QT_VERSION < 0x050000
    decisionSelectionTable->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
#else
    decisionSelectionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
#endif

    decisionSelectionModel = new DecisionSelectionModel(this);
    decisionSelectionTable->setModel(decisionSelectionModel);
    decisionSelectionTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->frame->layout()->addWidget(decisionSelectionTable);

    // Setup signals
    connect(decisionSelectionTable, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(on_table_doubleClicked(QModelIndex)));
}

DecisionSelectionView::~DecisionSelectionView()
{
    delete ui;
}

void DecisionSelectionView::loadDecisions(QList<marketDecision *> decisions)
{
    decisionSelectionModel->loadDecisions(decisions);
}

void DecisionSelectionView::on_table_doubleClicked(QModelIndex index)
{
    QString hex = decisionSelectionTable->model()->data(decisionSelectionTable->model()->index(index.row(), 1)).toString();
    emit decisionSelected(hex);
    emit done();
}

void DecisionSelectionView::on_pushButtonDone_clicked()
{
    QItemSelectionModel *selection = decisionSelectionTable->selectionModel();
    QStringList hexList;

    if (selection->hasSelection()) {
        QModelIndexList list = selection->selectedRows();

        for (int i = 0; i < list.size(); i++) {
            QString hex = decisionSelectionTable->model()->data(decisionSelectionTable->model()->index(i, 1)).toString();
            hexList.push_back(hex);
        }
    }
    emit multipleDecisionsSelected(hexList);
    emit done();
}
