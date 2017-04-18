/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

// Wrap RPC functions to implement packing/unpacking of JSON data.

#include "rpcwrapper.h"

#include <vector>
#include <QString>
#include <QMessageBox>

#include "json/json_spirit.h"


using namespace RpcWrapper;

using namespace std;
using namespace json_spirit;

extern Value listbranches(const Array &params, bool fHelp);

vector<BranchData> RpcWrapper::listBranches() {
    QString error;
    vector<BranchData> branches;


    // TODO: return real data once rpc is up and working.

    BranchData main;
    main.name = "Main";
    main.branchid = uint256S("0f894a25c5e0318ee148fe54600ebbf50782f0a1df1eb2aab06321a8ccec270d");
    branches.push_back(main);
    return branches;


    try {
        Value resp = listbranches(Array(), false);
        if (resp.type() == array_type) {
            Array branches_array = resp.get_array();
            for (size_t i = 0; i < branches_array.size() && error.isEmpty(); i++) {
                Value branch_value = branches_array[i];
                if (branch_value.type() == obj_type) {
                    Object branch_object = branch_value.get_obj();
                    QString name;
                    uint256 branchid;
                    for (size_t j = 0; j < branch_object.size() && error.isEmpty(); j++) {
                        if (branch_object[j].name_ == "name") {
                            if (!name.isEmpty()) {
                                error = "duplicate 'name' key in JSON data";
                                break;
                            }
                            Value name_value = branch_object[j].value_;
                            if (name_value.type() == str_type) {
                                name = QString(name_value.get_str().c_str());
                            } else {
                                error = "Expected a string for 'name' key in JSON data";
                                break;
                            }
                        }
                        if (branch_object[j].name_ == "branchid") {
                            if (!branchid.IsNull()) {
                                error = "duplicate 'branchid' key in JSON data";
                                break;
                            }
                            Value branchid_value = branch_object[j].value_;
                            if (branchid_value.type() == str_type) {
                                branchid.SetHex(branchid_value.get_str().c_str());
                            } else {
                                error = "Expected a string for 'name' key in JSON data";
                                break;
                            }
                        }
                    }
                    if (name.isEmpty()) {
                        error = "missing 'name' key in JSON data";
                        break;
                    }
                    if (branchid.IsNull()) {
                        error = "missing 'branchid' key in JSON data";
                        break;
                    }
                    BranchData branch;
                    branch.branchid = branchid;
                    branch.name = name;
                    branches.push_back(branch);
                } else {
                    error = "expected an object";
                    break;
                }
            }
        } else {
            error = "expected an array";
        }
    } catch (const std::exception &e) {
        error = e.what();
    } catch (...) {
        error = "unknown exception";
    };

    if (!error.isEmpty()) {
        QMessageBox msgBox;
        msgBox.setText("Failed to get list of branches");
        msgBox.setInformativeText(error);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        branches.clear();
    }

    return branches;
}

