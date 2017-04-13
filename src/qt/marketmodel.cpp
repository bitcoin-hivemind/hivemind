/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "marketmodel.h"

#include <QHBoxLayout>
#include <QList>
#include <QWidget>
#include <sstream>

#include "marketgraphwidget.h"
#include "txdb.h"
#include "wallet.h"
#include "walletmodel.h"

extern CMarketTreeDB *pmarkettree;

// Private implementation
class MarketModelPriv
{
public:
    MarketModelPriv(CWallet *wallet, MarketModel *parent)
        : wallet(wallet),
          parent(parent)
    {
    }

    CWallet *wallet;
    MarketModel *parent;

    QList<const marketMarket *> cached;

    int size()
    {
        return cached.size();
    }

    const marketMarket *index(int idx)
    {
        if(idx >= 0 && idx < cached.size())
            return cached[idx];
        return 0;
    }
};

MarketModel::MarketModel(CWallet *wallet, WalletModel *parent) :
   QAbstractTableModel(parent),
   wallet(wallet),
   walletModel(parent),
   priv(new MarketModelPriv(wallet, this))
{
}

MarketModel::~MarketModel()
{
    delete priv;
}

int MarketModel::rowCount(const QModelIndex & /*parent*/) const
{
   return priv->size();
}

int MarketModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 2;
}

QVariant MarketModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    const marketMarket *market = priv->index(row);

    switch(role) {
    case Qt::DisplayRole:
    {
        // Market Details
        if (col == 1) {
            std::stringstream sstream;
            sstream << "Title: " << market->title << std::endl;
            sstream << "Description: " << market->description << std::endl;
            sstream << "Tags: " << market->tags << std::endl;
            sstream << "Market ID: " << market->GetHash().GetHex() << std::endl;

            return QString::fromStdString(sstream.str());
        }
    }

    case Qt::DecorationRole:
    {
        // Graph
        if (col == 0) {
            MarketGraphWidget graphWidget;
            return graphWidget.getTableGraphPixmap(QString::fromStdString(market->title), market);
        }
    }

    case Qt::SizeHintRole:
    {
        // Graph
        if (col == 0) {
            return QSize(480, 360);
        }
    }
    }

    return QVariant();
}

const marketMarket *MarketModel::index(int row) const
{
    return priv->index(row);
}

QVariant MarketModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch(section) {
            case 0:
                return QString("Graph");
            case 1:
                return QString("Market Info");
            }
        }
    }

    return QVariant();
}

void MarketModel::setBranch(uint256 branchID)
{
    if (!priv) return;

    // Make sure the branch exists
    const marketBranch *branch = pmarkettree->GetBranch(branchID);
    if (!branch) return;

    // Erase / reset the cache
    resetModel();

    // Get the list of decisions on the branch
    vector<marketDecision *> decisions = pmarkettree->GetDecisions(branchID);

    // Make a list of markets from every decision on the branch
    vector<marketMarket *> markets;

    for (unsigned int x = 0; x < decisions.size(); x++) {
        // Get the markets for this decision
        uint256 uDecision;
        uDecision.SetHex(decisions.at(x)->GetHash().GetHex());
        vector<marketMarket* > decisionMarkets = pmarkettree->GetMarkets(uDecision);

        // Add the markets to the markets vector
        for (unsigned int y = 0; y < decisionMarkets.size(); y++) {
            markets.push_back(decisionMarkets.at(y));
        }
    }

    // Add markets to the model
    if (markets.size()) {
        beginInsertRows(QModelIndex(), 0, markets.size()-1);

        for (unsigned int i = 0; i < markets.size(); i++)
            priv->cached.append(markets.at(i));

        endInsertRows();
    }
}

void MarketModel::resetModel()
{
    beginResetModel();

    if (priv->cached.size()) {
        beginRemoveRows(QModelIndex(), 0, priv->cached.size()-1);
        for (ssize_t i = 0; i < priv->cached.size(); i++)
            delete priv->cached.at(i);
        priv->cached.clear();
        endRemoveRows();
    }

    endResetModel();
}
