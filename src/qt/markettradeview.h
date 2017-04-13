/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef MARKETTRADEVIEW_H
#define MARKETTRADEVIEW_H

#include "json/json_spirit_writer_template.h"
#include "uint256.h"

#include <QWidget>
#include "marketgraphwidget.h"

namespace Ui {
class MarketTradeView;
}

class MarketTradeView : public QWidget
{
    Q_OBJECT

public:
    explicit MarketTradeView(QWidget *parent = 0);
    ~MarketTradeView();

public slots:
    // Set (pointer to MarketModel) the market to create trades on
    void setMarket(const marketMarket *marketToTrade);

private slots:
    void on_pushButtonFinalize_clicked();

    // Update info on the trade widget by reloading market info
    void updateMarketInfo();

    void on_finalizeError(const QString &errorMessage);

    void on_spinBoxShares_valueChanged(int arg1);

    void on_pushButtonCopyMarketID_clicked();

    void on_pushButtonPopupGraph_clicked();

    void on_pushButtonMinus10_clicked();

    void on_pushButtonPlus10_clicked();

    void on_horizontalSliderNumberShares_valueChanged(int value);

signals:
    void finalizeError(const QString &errorMessage);

private:
    Ui::MarketTradeView *ui;

    // Market
    const marketMarket *market;

    // Market graph
    MarketGraphWidget *marketGraph;
};

#endif // MARKETTRADEVIEW_H
