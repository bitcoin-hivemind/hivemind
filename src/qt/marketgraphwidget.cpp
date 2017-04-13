/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "marketgraphwidget.h"
#include "ui_marketgraphwidget.h"

#include "qcustomplot.h"
#include "txdb.h"

extern CMarketTreeDB *pmarkettree;

MarketGraphWidget::MarketGraphWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MarketGraphWidget)
{
    ui->setupUi(this);
}

MarketGraphWidget::~MarketGraphWidget()
{
    delete ui;
}

QPixmap MarketGraphWidget::getTableGraphPixmap(QString title, const marketMarket *market)
{
    // If market doesn't exist, display error icon
    if (!market) return QPixmap(":/icons/quit");

    /* Get the trades for this market */
    std::vector<marketTrade *> trades;
    trades = pmarkettree->GetTrades(market->GetHash());

    unsigned int numTrades = trades.size();
    unsigned int maxPrice = 0;

    /* Load trading data for graph */
    QVector<double> x(numTrades), y(numTrades);
    for (unsigned int i = 0; i < numTrades; i++) {
        x[i] = i;
        y[i] = 1e-8*trades.at(i)->price;

        // Do we need to increase the range of the Y axis?
        if (y[i] > maxPrice) maxPrice = y[i];
    }

    /* Initialize Graph */
    ui->customPlot->addGraph();
    ui->customPlot->graph(0)->setData(x, y);

    /*
     * Graph range:
     * X axis min = 0 max = number of trades
     * Y axis min = 0 max = highest trade price plus 50 for padding
     */
    ui->customPlot->xAxis->setRange(0, numTrades);
    ui->customPlot->yAxis->setRange(0, maxPrice + 50);

    /* Style graph */
    QPen linePen;
    linePen.setWidth(4);
    ui->customPlot->graph()->setPen(linePen);
    ui->customPlot->plotLayout()->insertRow(0);
    QCPPlotTitle *plotTitle = new QCPPlotTitle(ui->customPlot);
    plotTitle->setText(title);
    plotTitle->setFont(QFont("Helvetica [Cronyx]", 16));
    ui->customPlot->plotLayout()->addElement(0, 0, plotTitle);
    ui->customPlot->xAxis->setTicks(false);
    ui->customPlot->xAxis->setTickLabels(false);
    ui->customPlot->yAxis->setTicks(false);
    ui->customPlot->yAxis->setTickLabels(false);

    ui->customPlot->replot();

    return ui->customPlot->toPixmap(480, 360);
}

void MarketGraphWidget::setupMarketTradeViewGraph(const marketMarket *market)
{
    if (!market) return;

    /* Get the trades for this market */
    std::vector<marketTrade *> trades;
    trades = pmarkettree->GetTrades(market->GetHash());

    unsigned int numTrades = trades.size();
    unsigned int maxPrice = 0;

    /* Load trading data for graph */
    QVector<double> x(numTrades), y(numTrades); // Trade data
    QVector<double> x2(numTrades), y2(numTrades); // Volume data
    for (unsigned int i = 0; i < numTrades; i++) {
        x [i] = i;
        x2[i] = i;
        y [i] = 1e-8*trades.at(i)->price;
        y2[i] = 1e-8*trades.at(i)->nShares;

        // Do we need to increase the range of the Y axis?
        if (y[i] > maxPrice) maxPrice = y[i];
    }

    /* Setup graphs */

    // Price graph
    QCPGraph *graphPrice = ui->customPlot->addGraph();
    graphPrice->setData(x, y);
    graphPrice->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::black, 1.5), QBrush(Qt::white), 9));
    graphPrice->setPen(QPen(QColor(120, 120, 120), 4));

    // Volume
    QCPBars *barsVolume = new QCPBars(ui->customPlot->xAxis, ui->customPlot->yAxis);
    ui->customPlot->addPlottable(barsVolume);
    barsVolume->setWidth(3/(double)x2.size());
    barsVolume->setData(x2, y2);
    barsVolume->setPen(Qt::NoPen);
    barsVolume->setBrush(QColor(10, 140, 70, 80));

    // move bars above graphs and grid below bars:
    ui->customPlot->addLayer("abovemain", ui->customPlot->layer("main"), QCustomPlot::limAbove);
    ui->customPlot->addLayer("belowmain", ui->customPlot->layer("main"), QCustomPlot::limBelow);
    graphPrice->setLayer("abovemain");
    ui->customPlot->xAxis->grid()->setLayer("belowmain");
    ui->customPlot->yAxis->grid()->setLayer("belowmain");

    // Set style (based on qcustomplot style demo)
    ui->customPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    ui->customPlot->yAxis->setBasePen(QPen(Qt::white, 1));
    ui->customPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    ui->customPlot->yAxis->setTickPen(QPen(Qt::white, 1));
    ui->customPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    ui->customPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));
    ui->customPlot->xAxis->setTickLabelColor(Qt::white);
    ui->customPlot->yAxis->setTickLabelColor(Qt::white);
    ui->customPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    ui->customPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    ui->customPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    ui->customPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    ui->customPlot->xAxis->grid()->setSubGridVisible(true);
    ui->customPlot->yAxis->grid()->setSubGridVisible(true);
    ui->customPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    ui->customPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    ui->customPlot->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    ui->customPlot->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    QLinearGradient plotGradient;
    plotGradient.setStart(0, 0);
    plotGradient.setFinalStop(0, 350);
    plotGradient.setColorAt(0, QColor(80, 80, 80));
    plotGradient.setColorAt(1, QColor(50, 50, 50));
    ui->customPlot->setBackground(plotGradient);
    QLinearGradient axisRectGradient;
    axisRectGradient.setStart(0, 0);
    axisRectGradient.setFinalStop(0, 350);
    axisRectGradient.setColorAt(0, QColor(80, 80, 80));
    axisRectGradient.setColorAt(1, QColor(30, 30, 30));
    ui->customPlot->axisRect()->setBackground(axisRectGradient);

    ui->customPlot->rescaleAxes();
    ui->customPlot->yAxis->setRange(-5, maxPrice + 5);
}

void MarketGraphWidget::setupFullMarketGraph()
{
    // generate two sets of random walk data (one for candlestick and one for ohlc chart):
    int n = 500;
    QVector<double> time(n), value1(n), value2(n);
    QDateTime start = QDateTime(QDate(2014, 6, 11));
    start.setTimeSpec(Qt::UTC);
    double startTime = start.toTime_t();
    double binSize = 3600*24; // bin data in 1 day intervals
    time[0] = startTime;
    value1[0] = 60;
    value2[0] = 20;
    qsrand(9);
    for (int i=1; i<n; ++i)
    {
      time[i] = startTime + 3600*i;
      value1[i] = value1[i-1] + (qrand()/(double)RAND_MAX-0.5)*10;
      value2[i] = value2[i-1] + (qrand()/(double)RAND_MAX-0.5)*3;
    }

    // create candlestick chart:
    QCPFinancial *candlesticks = new QCPFinancial(ui->customPlot->xAxis, ui->customPlot->yAxis);
    ui->customPlot->addPlottable(candlesticks);
    QCPFinancialDataMap data1 = QCPFinancial::timeSeriesToOhlc(time, value1, binSize, startTime);
    candlesticks->setName("Candlestick");
    candlesticks->setChartStyle(QCPFinancial::csCandlestick);
    candlesticks->setData(&data1, true);
    candlesticks->setWidth(binSize*0.9);
    candlesticks->setTwoColored(true);
    candlesticks->setBrushPositive(QColor(245, 245, 245));
    candlesticks->setBrushNegative(QColor(0, 0, 0));
    candlesticks->setPenPositive(QPen(QColor(0, 0, 0)));
    candlesticks->setPenNegative(QPen(QColor(0, 0, 0)));

    // create ohlc chart:
    QCPFinancial *ohlc = new QCPFinancial(ui->customPlot->xAxis, ui->customPlot->yAxis);
    ui->customPlot->addPlottable(ohlc);
    QCPFinancialDataMap data2 = QCPFinancial::timeSeriesToOhlc(time, value2, binSize/3.0, startTime); // divide binSize by 3 just to make the ohlc bars a bit denser
    ohlc->setName("OHLC");
    ohlc->setChartStyle(QCPFinancial::csOhlc);
    ohlc->setData(&data2, true);
    ohlc->setWidth(binSize*0.2);
    ohlc->setTwoColored(true);

    // create bottom axis rect for volume bar chart:
    QCPAxisRect *volumeAxisRect = new QCPAxisRect(ui->customPlot);
    ui->customPlot->plotLayout()->addElement(1, 0, volumeAxisRect);
    volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
    volumeAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
    // bring bottom and main axis rect closer together:
    ui->customPlot->plotLayout()->setRowSpacing(0);
    volumeAxisRect->setAutoMargins(QCP::msLeft|QCP::msRight|QCP::msBottom);
    volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));
    // create two bar plottables, for positive (green) and negative (red) volume bars:
    QCPBars *volumePos = new QCPBars(volumeAxisRect->axis(QCPAxis::atBottom), volumeAxisRect->axis(QCPAxis::atLeft));
    QCPBars *volumeNeg = new QCPBars(volumeAxisRect->axis(QCPAxis::atBottom), volumeAxisRect->axis(QCPAxis::atLeft));
    for (int i=0; i<n/5; ++i)
    {
      int v = qrand()%20000+qrand()%20000+qrand()%20000-10000*3;
      (v < 0 ? volumeNeg : volumePos)->addData(startTime+3600*5.0*i, qAbs(v)); // add data to either volumeNeg or volumePos, depending on sign of v
    }
    ui->customPlot->setAutoAddPlottableToLegend(false);
    ui->customPlot->addPlottable(volumePos);
    ui->customPlot->addPlottable(volumeNeg);
    volumePos->setWidth(3600*4);
    volumePos->setPen(Qt::NoPen);
    volumePos->setBrush(QColor(100, 180, 110));
    volumeNeg->setWidth(3600*4);
    volumeNeg->setPen(Qt::NoPen);
    volumeNeg->setBrush(QColor(180, 90, 90));

    // interconnect x axis ranges of main and bottom axis rects:
    connect(ui->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), volumeAxisRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    connect(volumeAxisRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), ui->customPlot->xAxis, SLOT(setRange(QCPRange)));
    // configure axes of both main and bottom axis rect:
    volumeAxisRect->axis(QCPAxis::atBottom)->setAutoTickStep(false);
    volumeAxisRect->axis(QCPAxis::atBottom)->setTickStep(3600*24*4); // 4 day tickstep
    volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelType(QCPAxis::ltDateTime);
    volumeAxisRect->axis(QCPAxis::atBottom)->setDateTimeSpec(Qt::UTC);
    volumeAxisRect->axis(QCPAxis::atBottom)->setDateTimeFormat("dd. MMM");
    volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    volumeAxisRect->axis(QCPAxis::atLeft)->setAutoTickCount(3);
    ui->customPlot->xAxis->setBasePen(Qt::NoPen);
    ui->customPlot->xAxis->setTickLabels(false);
    ui->customPlot->xAxis->setTicks(false); // only want vertical grid in main axis rect, so hide xAxis backbone, ticks, and labels
    ui->customPlot->xAxis->setAutoTickStep(false);
    ui->customPlot->xAxis->setTickStep(3600*24*4); // 4 day tickstep
    ui->customPlot->rescaleAxes();
    ui->customPlot->xAxis->scaleRange(1.025, ui->customPlot->xAxis->range().center());
    ui->customPlot->yAxis->scaleRange(1.1, ui->customPlot->yAxis->range().center());

    // make axis rects' left side line up:
    QCPMarginGroup *group = new QCPMarginGroup(ui->customPlot);
    ui->customPlot->axisRect()->setMarginGroup(QCP::msLeft|QCP::msRight, group);
    volumeAxisRect->setMarginGroup(QCP::msLeft|QCP::msRight, group);
}

