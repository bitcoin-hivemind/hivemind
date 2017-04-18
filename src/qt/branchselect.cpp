/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "branchselect.h"
#include "rpcwrapper.h"

using namespace Ui;

BranchSelect::BranchSelect(QWidget *parent):
    QComboBox(parent)
{
    vector<RpcWrapper::BranchData> branches = RpcWrapper::listBranches();
    for (size_t i = 0; i < branches.size(); i++) {
        QComboBox::addItem(branches[i].name);
        branchIds.push_back(branches[i].branchid);
        branchIndexes[branches[i].branchid] = i;
    }
}

BranchSelect::~BranchSelect() {
}

uint256 BranchSelect::currentId() {
    int index = currentIndex();
    uint256 ret;
    if (index != -1) {
        ret = branchIds[index];
    }
    return ret;
}

void BranchSelect::setCurrentId(const uint256 &id) {
    map<uint256, size_t>::iterator it = branchIndexes.find(id);
    if (it != branchIndexes.end()) {
        setCurrentIndex(it->second);
    }
}

