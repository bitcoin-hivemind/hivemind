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
    // Set the market ID of the market to create trades on
    void setMarketToTrade(uint256 uMarket);

private slots:
    void on_pushButtonFinalize_clicked();

    // Update info on the trade widget by reloading market info
    void updateMarketInfo();

    void on_finalizeError(const QString &errorMessage);

    void on_spinBoxShares_valueChanged(int arg1);

    void on_pushButtonCopyMarketID_clicked();

signals:
    void finalizeError(const QString &errorMessage);

private:
    Ui::MarketTradeView *ui;

    // Market ID
    uint256 uMarketID;

    // Market graph
    MarketGraphWidget *marketGraph;
};

#endif // MARKETTRADEVIEW_H
