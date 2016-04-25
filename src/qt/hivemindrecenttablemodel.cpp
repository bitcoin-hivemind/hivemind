#include "hivemindrecenttablemodel.h"

#include "scicon.h"

#include <QIcon>
#include <QMetaType>
#include <QTimer>
#include <QVariant>

Q_DECLARE_METATYPE(marketDecision*)
Q_DECLARE_METATYPE(marketMarket*)
Q_DECLARE_METATYPE(marketTrade*)

HivemindRecentTableModel::HivemindRecentTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    /*
     * Table Columns:
     * 1. Type / icon (decision, market, trade, vote)
     * 2. Title
     * 3. Height
     */

    // This timer will update the recent hivemind model
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateModel()));
    pollTimer->start(600000); // 10 minutes
    // Also fire a single shot timer to update the model after initial sync.
    QTimer::singleShot(42000, this, SLOT(updateModel()));

    // Perform first update
    updateModel();
}

int HivemindRecentTableModel::rowCount(const QModelIndex & /*parent*/) const
{
    return recentObjects.size();
}

int HivemindRecentTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 3;
}

QVariant HivemindRecentTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    // Get the current object
    QVariant recentObject = recentObjects.at(row);

    switch (role) {
    case Qt::DecorationRole:
    {
        // Icon
        if (col == 0) {
            // Decision icon
            if (recentObject.canConvert<marketDecision *>()) {
                return QPixmap(":/icons/decision").scaled(80, 80);
            }
            // Market icon
            if (recentObject.canConvert<marketMarket *>()) {
                return QPixmap(":/icons/market").scaled(80, 80);
            }
            // Trade icon
            if (recentObject.canConvert<marketTrade *>()) {
                return QPixmap(":/icons/trade").scaled(80, 80);
            }
        }
    }
    case Qt::DisplayRole:
    {
        // Title
        if (col == 1) {
            // Decision
            if (recentObject.canConvert<marketDecision *>()) {
                marketDecision *decision;
                decision = recentObject.value<marketDecision *>();
                QString title = QString::fromStdString(decision->prompt);
                if (!title.isEmpty()) return title;
            }
            // Market
            if (recentObject.canConvert<marketMarket *>()) {
                marketMarket *market;
                market = recentObject.value<marketMarket *>();
                QString title = QString::fromStdString(market->title);
                if (!title.isEmpty()) return title;
            }
            // Trade
            if (recentObject.canConvert<marketTrade *>()) {
                marketTrade *trade;
                trade = recentObject.value<marketTrade *>();
                QString type = "Trade: ";
                type.append(trade->isBuy ? "Buy" : "Sell");
                type.append(" (");
                type.append(QString::fromStdString(trade->marketid.ToString()));
                type.append(") ");
                return type;
            }
        }
        // Height
        if (col == 2) {
            if (recentObject.canConvert<marketDecision *>()) {
                marketDecision *decision;
                decision = recentObject.value<marketDecision *>();
                QString height = QString::number(decision->nHeight);
                if (!height.isEmpty()) return height;
            }
            // Market
            if (recentObject.canConvert<marketMarket *>()) {
                marketMarket *market;
                market = recentObject.value<marketMarket *>();
                QString height = QString::number(market->nHeight);
                if (!height.isEmpty()) return height;
            }
            // Trade
            if (recentObject.canConvert<marketTrade *>()) {
                marketTrade *trade;
                trade = recentObject.value<marketTrade *>();
                QString height = QString::number(trade->nHeight);
                if (!height.isEmpty()) return height;
            }
        }
    }
    }
    return QVariant();
}

QVariant HivemindRecentTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
            case 0:
                return QString("Type/Icon");
            case 1:
                return QString("Title");
            case 2:
                return QString("Height");
            }
        }
    }
    return QVariant();
}

void HivemindRecentTableModel::updateModel()
{
    /*
     * 1. Get Branches
     * 2. Get Decisions
     * 3. Get Trades
     * 4. Get Votes
     * 5. Sort data by most recent
     * 6. Display up to limit of items
     */

    // Clear old data
    beginResetModel();
    recentObjects.clear();
    endResetModel();

    // TODO Performance / memory
    // TODO Replace some pmarkettree calls with calls to hivemind model classes
    // TODO get branches other than main:
    // vector<marketBranch *> branches = pmarkettree->GetBranches();

    // Amount of recent objects per type to display
    unsigned int limit = 7;

    // Get branches
    uint256 branchID;
    branchID.SetHex("0f894a25c5e0318ee148fe54600ebbf50782f0a1df1eb2aab06321a8ccec270d");

    // Get decisions
    vector<marketDecision *> decisions = pmarkettree->GetDecisions(branchID);

    // Resize decisions
    if (decisions.size() > limit) decisions.resize(limit);

    // Get markets
    vector<marketMarket *> markets;
    for (unsigned int x = 0; x < decisions.size(); ++x) {
        // Get the markets for this decision
        uint256 uDecision;
        uDecision.SetHex(decisions.at(x)->GetHash().GetHex());
        vector<marketMarket *> decisionMarkets = pmarkettree->GetMarkets(uDecision);

        // Add the markets to the markets vector
        for (unsigned int y = 0; y < decisionMarkets.size(); ++y) {
            markets.push_back(decisionMarkets.at(y));
        }
    }

    // Resize markets
    if (markets.size() > limit) markets.resize(limit);

    // Get trades
    vector<marketTrade *> trades;
    for (unsigned int x = 0; x < markets.size(); ++x) {
        // Get the trades for this market
        uint256 uMarket;
        uMarket.SetHex(markets.at(x)->GetHash().GetHex());
        vector<marketTrade *> marketTrades = pmarkettree->GetTrades(uMarket);

        // Add the trades to the trades vector
        for (unsigned int y = 0; y < marketTrades.size(); ++y) {
            trades.push_back(marketTrades.at(y));
        }
    }

    // Resize trades
    if (trades.size() > limit) trades.resize(limit);

    // Get votes TODO

    unsigned int totalSize = (decisions.size() + markets.size() + trades.size());
    beginInsertRows(QModelIndex(), recentObjects.size(), totalSize);
    // Add decisions to model
    for (unsigned int i = 0; i < decisions.size(); ++i) {
        recentObjects.append(QVariant::fromValue(decisions.at(i)));
    }
    // Add markets to model
    for (unsigned int i = 0; i < markets.size(); ++i) {
        recentObjects.append(QVariant::fromValue(markets.at(i)));
    }
    // Add trades to model
    for (unsigned int i = 0; i < trades.size(); ++i) {
        recentObjects.append(QVariant::fromValue(trades.at(i)));
    }
    endInsertRows();
}
