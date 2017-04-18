/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef BRANCHSELECT_H
#define BRANCHSELECT_H

#include <vector>
#include <QComboBox>
#include "uint256.h"

using namespace std;

namespace Ui {
class BranchSelect : public QComboBox
{
    Q_OBJECT

public:
    explicit BranchSelect(QWidget *parent = 0);
    virtual ~BranchSelect();

    uint256 currentId();
    void setCurrentId(const uint256 &id);

private:
    vector<uint256> branchIds;
    map<uint256, size_t> branchIndexes;
};
}

#endif // BRANCHSELECT_H
