/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef MARKETVIEW_H
#define MARKETVIEW_H

#include "marketmodel.h"

#include <QTableView>
#include <QWidget>

class WalletModel;

namespace Ui {
class MarketView;
}

class MarketView : public QWidget
{
    Q_OBJECT

public:
    explicit MarketView(QWidget *parent = 0);
    ~MarketView();

    void setModel(WalletModel *model);

public slots:
        void on_tableView_doubleClicked(const QModelIndex &index);

        void setScale(int scale);

private slots:
        void on_pushButtonScaleLess_clicked();

        void on_pushButtonScaleMore_clicked();

private:
    Ui::MarketView *ui;

    WalletModel *walletModel;
    MarketModel *marketModel;
    QTableView *marketTableView;

    /*
     * 1 = 1 day
     * 2 = 3 days
     * 3 = 1 week
     * 4 = 3 weeks
     * 5 = 1 month
     * 6 = 3 months
     * 7 = 1 year
     */
    int graphScale;
};

#endif // MARKETVIEW_H
