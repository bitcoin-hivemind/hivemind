/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "markettradeview.h"
#include "ui_markettradeview.h"

#include "txdb.h"
#include "utilmoneystr.h"

#include <QClipboard>
#include <QDialog>
#include <QMessageBox>

extern CMarketTreeDB *pmarkettree;

MarketTradeView::MarketTradeView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MarketTradeView)
{
    ui->setupUi(this);

    connect(this, SIGNAL(finalizeError(QString)),
            this, SLOT(on_finalizeError(QString)));

    marketGraph = new MarketGraphWidget(this);
    marketGraph->setFixedSize(540, 380);

    ui->frameMarketGraph->layout()->addWidget(marketGraph);
}

MarketTradeView::~MarketTradeView()
{
    delete ui;
}

void MarketTradeView::on_pushButtonFinalize_clicked()
{
    updateMarketInfo();

    extern json_spirit::Value createtrade(const json_spirit::Array &params, bool fHelp);

    // Grab params from the UI
    std::string address = ui->lineEditAddress->text().toStdString();
    std::string marketHex = ui->labelMarketIDValue->text().toStdString();
    std::string type = (ui->radioButtonLong->isChecked())? "buy":"sell";
    double shares = ui->labelSharesToBuyValue->text().toDouble();
    double price = ui->doubleSpinBoxOrderPrice->value();
    int state = ui->spinBoxState->value();
    int nonce = 0;

    // Create params spirit array
    json_spirit::Array params;
    params.push_back(address);
    params.push_back(marketHex);
    params.push_back(type);
    params.push_back(shares);
    params.push_back(price);
    params.push_back(state);
    params.push_back(nonce);

    // Try to create trade
    json_spirit::Value result;
    try {
        result = createtrade(params, false);
    } catch (const std::runtime_error &error) {
        QString errorText = QString::fromStdString(error.what());
        emit finalizeError(errorText);
    } catch (const std::exception &exception) {
        QString exceptionText = QString::fromStdString(exception.what());
        emit finalizeError(exceptionText);
    } catch (const json_spirit::Object &object) {
        result = object;
    }

    // Check the result
    try {
        // Get result pairs
        json_spirit::Object resultObject = result.get_obj();
        json_spirit::Pair codePair = resultObject[0];
        json_spirit::Pair messagePair = resultObject[1];

        // If error, get error code and message
        if (codePair.name_ == "code") {
            int code = codePair.value_.get_int();
            if (code < 0) {
                QString messageText = "Error creating trade: \n";
                messageText.append(QString::fromStdString(messagePair.value_.get_str()));
                emit finalizeError(messageText);
            }
        }

        // If success, get txid and tradeid
        if (codePair.name_ == "txid" ) {
            if (messagePair.name_ == "tradeid") {
                ui->listWidgetYourTrades->addItem(QString::fromStdString(messagePair.value_.get_str()));
            }
        }

    } catch (const std::runtime_error &error) {
        QString errorText = QString::fromStdString(error.what());
        emit finalizeError(errorText);
    } catch (const std::exception &exception) {
        QString exceptionText = QString::fromStdString(exception.what());
        emit finalizeError(exceptionText);
    }
}

void MarketTradeView::updateMarketInfo()
{
    // Check that market has been set
    if (!market) return;

    // Set the market ID
    ui->labelMarketIDValue->setText(QString::fromStdString(market->GetHash().GetHex()));

    // Current trades on the market
    vector<marketTrade *> trades = pmarkettree->GetTrades(market->GetHash());

    // States on the market
    uint32_t nStates = marketNStates(market);

    // Shares on the market
    double *nShares = new double [nStates];
    marketNShares(trades, nStates, nShares);

    // Price of the market
    double accountValue = marketAccountValue(market->maxCommission, 1e-8*market->B, nStates, nShares);
    std::string marketPrice = FormatMoney(accountValue);
    ui->labelCurrentPriceValue->setText(QString::fromStdString(marketPrice));

    // Market graph
    marketGraph->setupMarketTradeViewGraph(market);
}

void MarketTradeView::setMarket(const marketMarket *marketToTrade)
{
    if (marketToTrade) market = marketToTrade;
    updateMarketInfo();
}

void MarketTradeView::on_finalizeError(const QString &errorMessage)
{
    QMessageBox errorMessageBox;
    errorMessageBox.setWindowTitle("Error creating trade!");
    errorMessageBox.setText(errorMessage);
    errorMessageBox.setStandardButtons(QMessageBox::Ok);
    errorMessageBox.setDefaultButton(QMessageBox::Ok);
    errorMessageBox.exec();
}

void MarketTradeView::on_spinBoxShares_valueChanged(int arg1)
{
    ui->labelSharesToBuyValue->setText(QString::number(arg1));
}

void MarketTradeView::on_pushButtonCopyMarketID_clicked()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(ui->labelMarketIDValue->text());
}

void MarketTradeView::on_pushButtonPopupGraph_clicked()
{
    QDialog *graphDialog = new QDialog(this);
    MarketGraphWidget *graphWidget = new MarketGraphWidget(this);
    QHBoxLayout *hbox = new QHBoxLayout(this);

    graphWidget->setupFullMarketGraph();
    graphDialog->setLayout(hbox);
    graphDialog->layout()->addWidget(graphWidget);

    graphDialog->show();
}

void MarketTradeView::on_pushButtonMinus10_clicked()
{
    ui->spinBoxShares->setValue(ui->spinBoxShares->value() - 10);
}

void MarketTradeView::on_pushButtonPlus10_clicked()
{
    ui->spinBoxShares->setValue(ui->spinBoxShares->value() + 10);
}

void MarketTradeView::on_horizontalSliderNumberShares_valueChanged(int value)
{
    ui->spinBoxShares->setValue(value);
}
