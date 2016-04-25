#ifndef MARKETGRAPHWIDGET_H
#define MARKETGRAPHWIDGET_H

#include <QWidget>

namespace Ui {
class MarketGraphWidget;
}

class MarketGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MarketGraphWidget(QWidget *parent = 0);
    ~MarketGraphWidget();

private:
    Ui::MarketGraphWidget *ui;

    void setupGraph();

    /*
     * Market graph to be used inside of table cells.
     *
     * Very small, 80x80 or smaller.
     * Should display a simple line X Y chart that is legible at small sizes
     */
    void setupTableGraph();

    /*
     * Market graph to be used inside the TradeView widget.
     * Should display a Candlestick and OHLC graph with enough detail for
     * traders making quick decisions but still fitting into a reasonable space.
     */
    void setupMarketTradeViewGraph();

    /*
     * Market graph to be viewed on its own, as large as the user wants it and
     * with as much detail as possible. Should be customizable. Users will be
     * able to open up this view from the MarketTradeView.
     */
    void setupFullMarketGraph();
};

#endif // MARKETGRAPHWIDGET_H
