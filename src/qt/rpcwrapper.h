/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

// Wrap RPC functions to implement packing/unpacking of JSON data.

#ifndef RPCWRAPPER_H
#define RPCWRAPPER_H

#include <vector>
#include <QString>

#include "uint256.h"

using namespace std;

namespace RpcWrapper {
    struct BranchData {
        uint256 branchid;
        QString name;
        // TODO fill in other fields
    };

    vector<BranchData> listBranches();
}

#endif // RPCWRAPPER_H
