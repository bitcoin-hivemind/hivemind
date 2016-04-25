#include "marketgraphwidget.h"
#include "ui_marketgraphwidget.h"

#include "qcustomplot.h"

MarketGraphWidget::MarketGraphWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MarketGraphWidget)
{
    ui->setupUi(this);

    setupMarketTradeViewGraph();
}

MarketGraphWidget::~MarketGraphWidget()
{
    delete ui;
}

void MarketGraphWidget::setupGraph()
{
}

void MarketGraphWidget::setupTableGraph()
{

}

void MarketGraphWidget::setupMarketTradeViewGraph()
{
    // Create random trade data for testing based on qcustomplot examples
    int trades = 10;
    QVector<double> time(trades), candlestickValues(trades), ohlcValues(trades);
    QDateTime start = QDateTime(QDate(2016, 4, 12));
    start.setTimeSpec(Qt::UTC);
    double startTime = start.toTime_t();
    double binSize = 3600*24*1; // bin data in 1 day intervals

    // Add times to trades
    qsrand(9);
    for (int i = 1; i < trades; ++i)
    {
      time[i] = startTime + 3600*24*i;
    }

    time[0] = startTime;
    candlestickValues[0] = 5;
    ohlcValues[0] = 10;
    candlestickValues[1] = 1;
    ohlcValues[1] = 22;
    candlestickValues[2] = 1;
    ohlcValues[2] = 30;
    candlestickValues[3] = 1;
    ohlcValues[3] = 55;
    candlestickValues[4] = 3;
    ohlcValues[4] = 20;
    candlestickValues[5] = 4;
    ohlcValues[5] = 30;
    candlestickValues[6] = 8;
    ohlcValues[6] = 40;
    candlestickValues[7] = 4;
    ohlcValues[7] = 20;
    candlestickValues[8] = 3;
    ohlcValues[8] = 15;
    candlestickValues[9] = 4;
    ohlcValues[9] = 10;

    // create candlestick chart:
    QCPFinancial *candlesticks = new QCPFinancial(ui->customPlot->xAxis, ui->customPlot->yAxis);
    ui->customPlot->addPlottable(candlesticks);
    QCPFinancialDataMap data1 = QCPFinancial::timeSeriesToOhlc(time, candlestickValues, binSize/5, startTime);
    candlesticks->setName("Candlestick");
    candlesticks->setChartStyle(QCPFinancial::csCandlestick);
    candlesticks->setData(&data1, true);
    candlesticks->setWidth(binSize*0.6);
    candlesticks->setTwoColored(true);
    candlesticks->setBrushPositive(QColor(245, 245, 245));
    candlesticks->setBrushNegative(QColor(0, 0, 0));
    candlesticks->setPenPositive(QPen(QColor(0, 0, 0)));
    candlesticks->setPenNegative(QPen(QColor(0, 0, 0)));

    // create ohlc chart:
    QCPFinancial *ohlc = new QCPFinancial(ui->customPlot->xAxis, ui->customPlot->yAxis);
    ui->customPlot->addPlottable(ohlc);
    QCPFinancialDataMap data2 = QCPFinancial::timeSeriesToOhlc(time, ohlcValues, binSize/5, startTime);
    ohlc->setName("OHLC");
    ohlc->setChartStyle(QCPFinancial::csOhlc);
    ohlc->setData(&data2, true);
    ohlc->setWidth(binSize*0.6);
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
    for (int i = 0; i < trades; ++i)
    {
      int v = i;
      (v < 0 ? volumeNeg : volumePos)->addData(startTime+3600*24*i, qAbs(v));
    }
    ui->customPlot->setAutoAddPlottableToLegend(false);
    ui->customPlot->addPlottable(volumePos);
    ui->customPlot->addPlottable(volumeNeg);
    volumePos->setWidth(3600);
    volumePos->setPen(Qt::NoPen);
    volumePos->setBrush(QColor(100, 180, 110));
    volumeNeg->setWidth(3600);
    volumeNeg->setPen(Qt::NoPen);
    volumeNeg->setBrush(QColor(180, 90, 90));

    // interconnect x axis ranges of main and bottom axis rects:
    connect(ui->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), volumeAxisRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    connect(volumeAxisRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), ui->customPlot->xAxis, SLOT(setRange(QCPRange)));
    // configure axes of both main and bottom axis rect:
    volumeAxisRect->axis(QCPAxis::atBottom)->setAutoTickStep(false);
    volumeAxisRect->axis(QCPAxis::atBottom)->setTickStep(3600*24*1); // 1 day tickstep
    volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelType(QCPAxis::ltDateTime);
    volumeAxisRect->axis(QCPAxis::atBottom)->setDateTimeSpec(Qt::UTC);
    volumeAxisRect->axis(QCPAxis::atBottom)->setDateTimeFormat("dd");
    volumeAxisRect->axis(QCPAxis::atLeft)->setAutoTickCount(3);
    ui->customPlot->xAxis->setBasePen(Qt::NoPen);
    ui->customPlot->xAxis->setTickLabels(false);
    ui->customPlot->xAxis->setTicks(false); // only want vertical grid in main axis rect, so hide xAxis backbone, ticks, and labels
    ui->customPlot->xAxis->setAutoTickStep(false);
    ui->customPlot->xAxis->setTickStep(3600*24*1); // 1 day tickstep
    ui->customPlot->rescaleAxes();
    ui->customPlot->xAxis->scaleRange(1, ui->customPlot->xAxis->range().center());
    ui->customPlot->yAxis->scaleRange(1, ui->customPlot->yAxis->range().center());

    // Align left side of axis
    QCPMarginGroup *group = new QCPMarginGroup(ui->customPlot);
    ui->customPlot->axisRect()->setMarginGroup(QCP::msLeft|QCP::msRight, group);
    volumeAxisRect->setMarginGroup(QCP::msLeft|QCP::msRight, group);
}

void MarketGraphWidget::setupFullMarketGraph()
{

}

