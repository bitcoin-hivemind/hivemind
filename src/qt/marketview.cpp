/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "marketview.h"
#include "ui_marketview.h"

#include "markettradeview.h"
#include "walletmodel.h"

#include <QDialog>
#include <QHBoxLayout>

MarketView::MarketView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MarketView)
{
    ui->setupUi(this);

    /* Setup market table */
    marketTableView = new QTableView(this);
    marketTableView->horizontalHeader()->setStretchLastSection(true);

#if QT_VERSION < 0x050000
    marketTableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    marketTableView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    marketTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    marketTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif

    marketTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->frameRight->layout()->addWidget(marketTableView);

    // Setup signals
    connect(marketTableView, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(on_tableView_doubleClicked(QModelIndex)));

    // Default graph scale = 1 (1 day)
    graphScale = 1;
}

MarketView::~MarketView()
{
    delete ui;
}

void MarketView::on_tableView_doubleClicked(const QModelIndex &index)
{
    MarketTradeView *marketTradeView = new MarketTradeView(this);
    marketTradeView->setMarket(marketModel->index(index.row()));

    QHBoxLayout *hbox = new QHBoxLayout(this);
    hbox->addWidget(marketTradeView);

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Trade");
    dialog->setLayout(hbox);
    dialog->show();
}

void MarketView::setScale(int scale)
{
    switch (scale) {
    case 1: // 1 day
        ui->radioButtonScale1->setChecked(true);
        graphScale = scale;
        break;
    case 2: // 3 days
        ui->radioButtonScale2->setChecked(true);
        graphScale = scale;
        break;
    case 3: // 1 week
        ui->radioButtonScale3->setChecked(true);
        graphScale = scale;
        break;
    case 4: // 3 weeks
        ui->radioButtonScale4->setChecked(true);
        graphScale = scale;
        break;
    case 5: // 1 month
        ui->radioButtonScale5->setChecked(true);
        graphScale = scale;
        break;
    case 6: // 3 months
        ui->radioButtonScale6->setChecked(true);
        graphScale = scale;
        break;
    case 7: // 1 year
        ui->radioButtonScale7->setChecked(true);
        graphScale = scale;
        break;
    default:
        break;
    }
}

void MarketView::on_pushButtonScaleLess_clicked()
{
    setScale(graphScale-1);
}

void MarketView::on_pushButtonScaleMore_clicked()
{
    setScale(graphScale+1);
}

void MarketView::setModel(WalletModel *model)
{
    this->walletModel = model;
    marketModel = model->getMarketModel();
    if (!marketModel) return;

    // Grab the branch
    uint256 uBranch;
    uBranch.SetHex("0f894a25c5e0318ee148fe54600ebbf50782f0a1df1eb2aab06321a8ccec270d");
    // Set the branch for the market table model
    marketModel->setBranch(uBranch);

    marketTableView->setModel(marketModel);
}
