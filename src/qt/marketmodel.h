/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef MARKETMODEL_H
#define MARKETMODEL_H

#include <QAbstractTableModel>
#include <string>
#include "uint256.h"

class marketMarket;
class MarketModelPriv;
class WalletModel;
class CWallet;

class MarketModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit MarketModel(CWallet *wallet, WalletModel *parent = 0);
    ~MarketModel();
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    const marketMarket *index(int index) const;

private:
    CWallet *wallet;
    WalletModel *walletModel;
    MarketModelPriv *priv;

public slots:
    void resetModel();
    void setBranch(uint256 branchID);

    friend class MarketModelPriv;
};

#endif // MARKETMODEL_H
