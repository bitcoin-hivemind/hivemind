// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2015 The Hivemind Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "base58.h"
#include "coincontrol.h"
#include "core_io.h"
#include "rpcserver.h"
#include "init.h"
#include "net.h"
#include "netbase.h"
#include "primitives/market.h"
#include "timedata.h"
#include "txdb.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"

#include <stdint.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace std;
using namespace json_spirit;

int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;
extern CMarketTreeDB *pmarkettree;


std::string HelpRequiringPassphrase()
{
    return pwalletMain && pwalletMain->IsCrypted()
        ? "\nRequires wallet passphrase to be set with walletpassphrase call."
        : "";
}

void EnsureWalletIsUnlocked()
{
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
}

void WalletTxToJSON(const CWalletTx& wtx, Object& entry)
{
    int confirms = wtx.GetDepthInMainChain();
    entry.push_back(Pair("confirmations", confirms));
    if (wtx.IsCoinBase())
        entry.push_back(Pair("generated", true));
    if (confirms > 0)
    {
        entry.push_back(Pair("blockhash", wtx.hashBlock.GetHex()));
        entry.push_back(Pair("blockindex", wtx.nIndex));
        entry.push_back(Pair("blocktime", mapBlockIndex[wtx.hashBlock]->GetBlockTime()));
    }
    uint256 hash = wtx.GetHash();
    entry.push_back(Pair("txid", hash.GetHex()));
    Array conflicts;
    BOOST_FOREACH(const uint256& conflict, wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.push_back(Pair("walletconflicts", conflicts));
    entry.push_back(Pair("time", wtx.GetTxTime()));
    entry.push_back(Pair("timereceived", (int64_t)wtx.nTimeReceived));
    BOOST_FOREACH(const PAIRTYPE(string,string)& item, wtx.mapValue)
        entry.push_back(Pair(item.first, item.second));
}

string AccountFromValue(const Value& value)
{
    string strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}

Value getnewaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewaddress ( \"account\" )\n"
            "\nReturns a new Hivemind address for receiving payments.\n"
            "If 'account' is specified (recommended), it is added to the address book \n"
            "so payments received with the address will be credited to 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"        (string, optional) The account name for the address to be linked to. if not provided, the default account \"\" is used. It can also be set to the empty string \"\" to represent the default account. The account does not need to exist, it will be created if there is no account by the given name.\n"
            "\nResult:\n"
            "\"hivemindaddress\"    (string) The new hivemind address\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleCli("getnewaddress", "\"\"")
            + HelpExampleCli("getnewaddress", "\"myaccount\"")
            + HelpExampleRpc("getnewaddress", "\"myaccount\"")
        );

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBook(keyID, strAccount, "receive");

    return CHivemindAddress(keyID).ToString();
}


CHivemindAddress GetAccountAddress(string strAccount, bool bForceNew=false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    bool bKeyUsed = false;

    // Check if the current key has been used
    if (account.vchPubKey.IsValid())
    {
        CScript scriptPubKey = GetScriptForDestination(account.vchPubKey.GetID());
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && account.vchPubKey.IsValid();
             ++it)
        {
            const CWalletTx& wtx = (*it).second;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    bKeyUsed = true;
        }
    }

    // Generate a new key
    if (!account.vchPubKey.IsValid() || bForceNew || bKeyUsed)
    {
        if (!pwalletMain->GetKeyFromPool(account.vchPubKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

        pwalletMain->SetAddressBook(account.vchPubKey.GetID(), strAccount, "receive");
        walletdb.WriteAccount(strAccount, account);
    }

    return CHivemindAddress(account.vchPubKey.GetID());
}

CHivemindAddress GetBranchAccountAddress(string branchID, bool bForceNew=false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CHivemindAddress branchAddress;
    branchAddress.is_votecoin = 1;

    // Check that branch is valid
    uint256 uBranch;
    uBranch.SetHex(branchID);

    marketBranch *branch = pmarkettree->GetBranch(uBranch);
    if (!branch) return branchAddress;

    delete branch;

    // Read account
    CAccount account;
    walletdb.ReadAccount(branchID, account);

    bool bKeyUsed = false;

    // Check if the current key has been used
    if (account.vchPubKey.IsValid())
    {
        CScript scriptPubKey = GetScriptForDestination(account.vchPubKey.GetID());
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && account.vchPubKey.IsValid();
             ++it)
        {
            const CWalletTx& wtx = (*it).second;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    bKeyUsed = true;
        }
    }

    // Generate a new key
    if (!account.vchPubKey.IsValid() || bForceNew || bKeyUsed)
    {
        if (!pwalletMain->GetKeyFromPool(account.vchPubKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

        pwalletMain->SetAddressBook(account.vchPubKey.GetID(), branchID, "receive");
        walletdb.WriteAccount(branchID, account);
    }

    branchAddress.Set(account.vchPubKey.GetID());
    return branchAddress;
}

Value getaccountaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccountaddress \"account\"\n"
            "\nReturns the current Hivemind address for receiving payments to this account.\n"
            "\nArguments:\n"
            "1. \"account\"       (string, required) The account name for the address. It can also be set to the empty string \"\" to represent the default account. The account does not need to exist, it will be created and a new address created  if there is no account by the given name.\n"
            "\nResult:\n"
            "\"hivemindaddress\"   (string) The account hivemind address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccountaddress", "")
            + HelpExampleCli("getaccountaddress", "\"\"")
            + HelpExampleCli("getaccountaddress", "\"myaccount\"")
            + HelpExampleRpc("getaccountaddress", "\"myaccount\"")
        );

    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    Value ret;

    ret = GetAccountAddress(strAccount).ToString();

    return ret;
}

Value getbranchaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getbranchaddress \"branchid\"\n"
            "\nReturns the users current votecoin address for this branch.\n"
            "\nArguments:\n"
            "1. \"branchid\"       (string, required) The branch id (account) to get an address from.\n"
            "\nResult:\n"
            "\"votecoinaddress\"   (string) The branch (account) votecoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("getbranchaddress", "")
            + HelpExampleCli("getbranchaddress", "\"\"")
            + HelpExampleCli("getbranchaddress", "\"branchid\"")
            + HelpExampleRpc("getbranchaddress", "\"branchid\"")
        );

    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    Value ret;

    ret = GetBranchAccountAddress(strAccount).ToString();

    return ret;
}

Value getrawchangeaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getrawchangeaddress\n"
            "\nReturns a new Hivemind address, for receiving change.\n"
            "This is for use with raw transactions, NOT normal use.\n"
            "\nResult:\n"
            "\"address\"    (string) The address\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawchangeaddress", "")
            + HelpExampleRpc("getrawchangeaddress", "")
       );

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    CReserveKey reservekey(pwalletMain);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CKeyID keyID = vchPubKey.GetID();

    return CHivemindAddress(keyID).ToString();
}


Value setaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setaccount \"hivemindaddress\" or \"votecoinaddress\" \"account\"\n"
            "\nSets the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"hivemindaddress\" or \"votecoinaddress\"  (string, required) The hivemind address to be associated with an account.\n"
            "2. \"account\"         (string, required) The account to assign the address to.\n"
            "\nExamples:\n"
            + HelpExampleCli("setaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"tabby\"")
            + HelpExampleRpc("setaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"tabby\"")
        );

    CHivemindAddress address;
    string strAddress = params[0].get_str();

    // Try setting Hivemind address
    if (!address.SetString(strAddress)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind / Votecoin address");
    }

    // Is this a Votecoin address?
    if (!address.IsValid()) {
        address.is_votecoin = 1;
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind / Votecoin address");
    }

    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Only add the account if the address is yours.
    if (IsMine(*pwalletMain, address.Get()))
    {
        // Detect when changing the account of an address that is the 'unused current key' of another account:
        if (pwalletMain->mapAddressBook.count(address.Get()))
        {
            string strOldAccount = pwalletMain->mapAddressBook[address.Get()].name;
            if (address == GetAccountAddress(strOldAccount))
                GetAccountAddress(strOldAccount, true);
        }
        pwalletMain->SetAddressBook(address.Get(), strAccount, "receive");
    }
    else
        throw JSONRPCError(RPC_MISC_ERROR, "setaccount can only be used with own address");

    return Value::null;
}


Value getaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccount \"hivemindaddress\" or \"votecoinaddress\"\n"
            "\nReturns the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"hivemindaddress\" or \"votecoinaddress\" (string, required) The address for account lookup.\n"
            "\nResult:\n"
            "\"accountname\"        (string) the account address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"")
            + HelpExampleRpc("getaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"")
        );

    CHivemindAddress address;
    string strAddress = params[0].get_str();

    // Try setting Hivemind address
    if (!address.SetString(strAddress)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind / Votecoin address");
    }

    // Is this a Votecoin address?
    if (!address.IsValid()) {
        address.is_votecoin = 1;
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind / Votecoin address");
    }

    string strAccount;
    map<CTxDestination, CAddressBookData>::iterator mi = pwalletMain->mapAddressBook.find(address.Get());
    if (mi != pwalletMain->mapAddressBook.end() && !(*mi).second.name.empty())
        strAccount = (*mi).second.name;
    return strAccount;
}


Value getaddressesbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressesbyaccount \"account\"\n"
            "\nReturns the list of addresses for the given account.\n"
            "\nArguments:\n"
            "1. \"account\"  (string, required) The account name.\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"hivemindaddress\"  (string) a hivemind address associated with the given account\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressesbyaccount", "\"tabby\"")
            + HelpExampleRpc("getaddressesbyaccount", "\"tabby\"")
        );

    string strAccount = AccountFromValue(params[0]);

    // Find all addresses that have the given account
    Array ret;
    BOOST_FOREACH(const PAIRTYPE(CHivemindAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
    {
        const CHivemindAddress& address = item.first;
        const string& strName = item.second.name;
        if (strName == strAccount)
            ret.push_back(address.ToString());
    }
    return ret;
}

void SendMoney(
    const std::string &strAccount,
    const CTxDestination &address,
    CAmount nValue,
    CWalletTx& wtxNew)
{
    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > pwalletMain->GetBalance())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    string strError;
    if (pwalletMain->IsLocked())
    {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Parse Hivemind address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    CTxDestination txDestChange;
    bool rc = pwalletMain->CreateTransaction(scriptPubKey, strAccount, nValue, wtxNew,
        reservekey, nFeeRequired, strError, txDestChange);
    if (!rc) {
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(
                "Error: This transaction requires a transaction fee of at least"
                " %s because of its amount, complexity, or use of recently "
                " received funds!", FormatMoney(nFeeRequired));
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    rc = pwalletMain->CommitTransaction(wtxNew, reservekey);
    if (!rc) {
        throw JSONRPCError(RPC_WALLET_ERROR,
            "Error: The transaction was rejected! This might happen if some of the"
            " coins in your wallet were already spent, such as if you used a copy"
            " of wallet.dat and coins were spent in the copy but not marked as spent here.");
    }

    // If the account was specified, set the change to be in the same account
    if (strAccount.size())
        pwalletMain->SetAddressBook(txDestChange, strAccount, "receive");
}

Value sendtoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendtoaddress \"hivemindaddress\" or  \"votecoinaddress\" amount ( \"comment\" \"comment-to\" )\n"
            "\nSend an amount to a given address. The amount is a real and is rounded to the nearest 0.00000001\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"hivemindaddress\"  (string, required) The hivemind / votecoin address to send to.\n"
            "2. \"amount\"      (numeric, required) The amount in btc to send. eg 0.1\n"
            "3. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleRpc("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
        );

    string strAddress = params[0].get_str();
    CHivemindAddress address;

    // Try setting Hivemind address
    address.SetString(strAddress);
    if (!address.IsValid()) {
        // Try setting Votecoin address
        address.is_votecoin = 1;
        address.SetString(strAddress);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind / Votecoin address");
    }

    // Amount
    CAmount nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();

    EnsureWalletIsUnlocked();

    string strAccount;
    SendMoney(strAccount, address.Get(), nAmount, wtx);

    return wtx.GetHash().GetHex();
}

Value listaddressgroupings(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "listaddressgroupings\n"
            "\nLists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"hivemindaddress\",     (string) The hivemind address\n"
            "      amount,                 (numeric) The amount in btc\n"
            "      \"account\"             (string, optional) The account\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        );

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Array jsonGroupings;
    map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances();
    BOOST_FOREACH(set<CTxDestination> grouping, pwalletMain->GetAddressGroupings())
    {
        Array jsonGrouping;
        BOOST_FOREACH(CTxDestination txdest, grouping)
        {
            bool is_votecoin = 0;
            string strAccount;
            CHivemindAddress addr0(txdest);
            LOCK(pwalletMain->cs_wallet);
            std::map<CTxDestination, CAddressBookData>::const_iterator addrit
                = pwalletMain->mapAddressBook.find(addr0.Get());
            if (addrit != pwalletMain->mapAddressBook.end()) {
                strAccount = addrit->second.name;
                if (strAccount.size() == 64) {
                    uint256 branchid(ParseHex(strAccount));
                    std::reverse(branchid.begin(), branchid.end());
                    marketBranch *branch = pmarkettree->GetBranch(branchid);
                    if (branch) {
                        is_votecoin = 1;
                        delete branch;
                    }
                }
            }
            CHivemindAddress addr;
            addr.is_votecoin = is_votecoin;
            if (!addr.Set(txdest))
                continue;

            Array addressInfo;
            addressInfo.push_back(addr.ToString());
            addressInfo.push_back(ValueFromAmount(balances[txdest]));
            if (strAccount.size())
                addressInfo.push_back(strAccount);
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

Value signmessage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "signmessage \"hivemindaddress\" \"message\"\n"
            "\nSign a message with the private key of an address"
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"hivemindaddress\"  (string, required) The hivemind address to use for the private key.\n"
            "2. \"message\"         (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"my message\"")
        );

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strMessage = params[1].get_str();

    CHivemindAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

Value getreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaddress \"hivemindaddress\" ( minconf )\n"
            "\nReturns the total amount received by the given hivemindaddress in transactions with at least minconf confirmations.\n"
            "\nArguments:\n"
            "1. \"hivemindaddress\"  (string, required) The hivemind address for transactions.\n"
            "2. minconf             (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount   (numeric) The total amount in btc received at this address.\n"
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", 6")
       );

    // Hivemind address
    CHivemindAddress address = CHivemindAddress(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind address");
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    if (!IsMine(*pwalletMain,scriptPubKey))
        return (double)0.0;

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    CAmount nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !IsFinalTx(wtx))
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
    }

    return  ValueFromAmount(nAmount);
}

Value getreceivedbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaccount \"account\" ( minconf )\n"
            "\nReturns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, required) The selected account, may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in btc received for this account.\n"
            "\nExamples:\n"
            "\nAmount received by the default account with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaccount", "\"\"") +
            "\nAmount received at the tabby account including unconfirmed amounts with zero confirmations\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaccount", "\"tabby\", 6")
        );

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Get the set of pub keys assigned to account
    string strAccount = AccountFromValue(params[0]);
    set<CTxDestination> setAddress = pwalletMain->GetAccountAddresses(strAccount);

    // Tally
    CAmount nAmount = 0;
    map<uint256, CWalletTx>::iterator it;
    for (it=pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (!IsFinalTx(wtx))
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address)
                    && IsMine(*pwalletMain, address)
                    && setAddress.count(address))
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
        }
    }

    return (double)nAmount / (double)COIN;
}


CAmount GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth, const isminefilter& filter)
{
    CAmount nBalance = 0;

    // Tally wallet transactions
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (!IsFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
            continue;

        CAmount nReceived, nSent, nFee;
        wtx.GetAccountAmounts(strAccount, nReceived, nSent, nFee, filter);

        if (nReceived != 0 && wtx.GetDepthInMainChain() >= nMinDepth)
            nBalance += nReceived;
        nBalance -= nSent + nFee;
    }

    // Tally internal accounting entries
    nBalance += walletdb.GetAccountCreditDebit(strAccount);

    return nBalance;
}

CAmount GetAccountBalance(const string& strAccount, int nMinDepth, const isminefilter& filter)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter);
}


Value getbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "getbalance ( \"account\" minconf includeWatchonly )\n"
            "\nIf account is not specified, returns the server's total available balance.\n"
            "If account is specified, returns the balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, optional) The selected account, or \"*\" for entire wallet. It may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in btc received for this account.\n"
            "\nExamples:\n"
            "\nThe total amount in the server across all accounts\n"
            + HelpExampleCli("getbalance", "") +
            "\nThe total amount in the server across all accounts, with at least 5 confirmations\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nThe total amount in the default account with at least 1 confirmation\n"
            + HelpExampleCli("getbalance", "\"\"") +
            "\nThe total amount in the account named tabby with at least 6 confirmations\n"
            + HelpExampleCli("getbalance", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getbalance", "\"tabby\", 6")
        );

    if (params.size() == 0)
        return ValueFromAmount(pwalletMain->GetBalance());

    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and getbalance '*' 0 should return the same number
        CAmount nBalance = 0;
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (!wtx.IsTrusted() || wtx.GetBlocksToMaturity() > 0)
                continue;

            CAmount allFee;
            string strSentAccount;
            list<COutputEntry> listReceived;
            list<COutputEntry> listSent;
            wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);
            if (wtx.GetDepthInMainChain() >= nMinDepth)
            {
                BOOST_FOREACH(const COutputEntry& r, listReceived)
                    nBalance += r.amount;
            }
            BOOST_FOREACH(const COutputEntry& s, listSent)
                nBalance -= s.amount;
            nBalance -= allFee;
        }
        return ValueFromAmount(nBalance);
    }

    string strAccount = AccountFromValue(params[0]);

    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, filter);

    return ValueFromAmount(nBalance);
}

Value getunconfirmedbalance(const Array &params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "getunconfirmedbalance\n"
                "Returns the server's total unconfirmed balance\n");
    return ValueFromAmount(pwalletMain->GetUnconfirmedBalance());
}


Value movecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
            "move \"fromaccount\" \"toaccount\" amount ( minconf \"comment\" )\n"
            "\nMove a specified amount from one account in your wallet to another.\n"
            "\nArguments:\n"
            "1. \"fromaccount\"   (string, required) The name of the account to move funds from. May be the default account using \"\".\n"
            "2. \"toaccount\"     (string, required) The name of the account to move funds to. May be the default account using \"\".\n"
            "3. minconf           (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "4. \"comment\"       (string, optional) An optional comment, stored in the wallet only.\n"
            "\nResult:\n"
            "true|false           (boolean) true if successfull.\n"
            "\nExamples:\n"
            "\nMove 0.01 btc from the default account to the account named tabby\n"
            + HelpExampleCli("move", "\"\" \"tabby\" 0.01") +
            "\nMove 0.01 btc timotei to akiko with a comment and funds have 6 confirmations\n"
            + HelpExampleCli("move", "\"timotei\" \"akiko\" 0.01 6 \"happy birthday!\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("move", "\"timotei\", \"akiko\", 0.01, 6, \"happy birthday!\"")
        );

    string strFrom = AccountFromValue(params[0]);
    string strTo = AccountFromValue(params[1]);
    CAmount nAmount = AmountFromValue(params[2]);
    if (params.size() > 3)
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)params[3].get_int();
    string strComment;
    if (params.size() > 4)
        strComment = params[4].get_str();

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.TxnBegin())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    walletdb.WriteAccountingEntry(debit);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    walletdb.WriteAccountingEntry(credit);

    if (!walletdb.TxnCommit())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    return true;
}


Value sendfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error(
            "sendfrom \"fromaccount\" \"tohivemindaddress\" amount ( minconf \"comment\" \"comment-to\" )\n"
            "\nSent an amount from an account to a hivemind address.\n"
            "The amount is a real and is rounded to the nearest 0.00000001."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaccount\"       (string, required) The name of the account to send funds from. May be the default account using \"\".\n"
            "2. \"tohivemindaddress\"  (string, required) The hivemind address to send funds to.\n"
            "3. amount                (numeric, required) The amount in btc. (transaction fee is added on top).\n"
            "4. minconf               (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "5. \"comment\"           (string, optional) A comment used to store what the transaction is for. \n"
            "                                     This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"        (string, optional) An optional comment to store the name of the person or organization \n"
            "                                     to which you're sending the transaction. This is not part of the transaction, \n"
            "                                     it is just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"        (string) The transaction id.\n"
            "\nExamples:\n"
            "\nSend 0.01 btc from the default account to the address, must have at least 1 confirmation\n"
            + HelpExampleCli("sendfrom", "\"\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.01") +
            "\nSend 0.01 from the tabby account to the given address, funds must have at least 6 confirmations\n"
            + HelpExampleCli("sendfrom", "\"tabby\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.01 6 \"donation\" \"seans outpost\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendfrom", "\"tabby\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.01, 6, \"donation\", \"seans outpost\"")
        );

    string strAccount = AccountFromValue(params[0]);
    string strAddress = params[1].get_str();
    CHivemindAddress address;

    // Try setting Hivemind address
    address.SetString(strAddress);
    if (!address.IsValid()) {
        // Try setting Votecoin address
        address.is_votecoin = 1;
        address.SetString(strAddress);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind / Votecoin address");
    }

    CAmount nAmount = AmountFromValue(params[2]);
    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, ISMINE_SPENDABLE);
    if (nAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    SendMoney(strAccount, address.Get(), nAmount, wtx);

    return wtx.GetHash().GetHex();
}


Value sendmany(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendmany \"fromaccount\" {\"address\":amount,...} ( minconf \"comment\" )\n"
            "\nSend multiple times. Amounts are double-precision floating point numbers."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaccount\"         (string, required) The account to send the funds from, can be \"\" for the default account\n"
            "2. \"amounts\"             (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric) The hivemind address is the key, the numeric amount in btc is the value\n"
            "      ,...\n"
            "    }\n"
            "3. minconf                 (numeric, optional, default=1) Only use the balance confirmed at least this many times.\n"
            "4. \"comment\"             (string, optional) A comment\n"
            "\nResult:\n"
            "\"transactionid\"          (string) The transaction id for the send. Only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"
            "\nExamples:\n"
            "\nSend two amounts to two different addresses:\n"
            + HelpExampleCli("sendmany", "\"tabby\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\"") +
            "\nSend two amounts to two different addresses setting the confirmation and comment:\n"
            + HelpExampleCli("sendmany", "\"tabby\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 6 \"testing\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendmany", "\"tabby\", \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\", 6, \"testing\"")
        );

    string strAccount = AccountFromValue(params[0]);
    Object sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();

    set<CHivemindAddress> setAddress;
    vector<pair<CScript, CAmount> > vecSend;

    CAmount totalAmount = 0;
    BOOST_FOREACH(const Pair& s, sendTo)
    {
        CHivemindAddress address(s.name_);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Hivemind address: ")+s.name_);

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+s.name_);
        setAddress.insert(address);

        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CAmount nAmount = AmountFromValue(s.value_);
        totalAmount += nAmount;

        vecSend.push_back(make_pair(scriptPubKey, nAmount));
    }

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, ISMINE_SPENDABLE);
    if (totalAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    // Send
    CReserveKey keyChange(pwalletMain);
    CAmount nFeeRequired = 0;
    string strError;
    CTxDestination txDestChange;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, strAccount, wtx,
        keyChange, nFeeRequired, strError, txDestChange);
    if (!fCreated)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strError);
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");

    return wtx.GetHash().GetHex();
}

// Defined in rpcmisc.cpp
extern CScript _createmultisig_redeemScript(const Array& params);

Value addmultisigaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
    {
        string msg = "addmultisigaddress nrequired [\"key\",...] ( \"account\" )\n"
            "\nAdd a nrequired-to-sign multisignature address to the wallet.\n"
            "Each key is a Hivemind address or hex-encoded public key.\n"
            "If 'account' is specified, assign address to that account.\n"

            "\nArguments:\n"
            "1. nrequired        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keysobject\"   (string, required) A json array of hivemind addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"  (string) hivemind address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. \"account\"      (string, optional) An account to assign the addresses to.\n"

            "\nResult:\n"
            "\"hivemindaddress\"  (string) A hivemind address associated with the keys.\n"

            "\nExamples:\n"
            "\nAdd a multisig address from 2 addresses\n"
            + HelpExampleCli("addmultisigaddress", "2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("addmultisigaddress", "2, \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    string strAccount;
    if (params.size() > 2)
        strAccount = AccountFromValue(params[2]);

    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);
    pwalletMain->AddCScript(inner);

    pwalletMain->SetAddressBook(innerID, strAccount, "send");
    return CHivemindAddress(innerID).ToString();
}


struct tallyitem
{
    CAmount nAmount;
    int nConf;
    vector<uint256> txids;
    bool fIsWatchonly;
    tallyitem()
    {
        nAmount = 0;
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
    }
};

Value ListReceived(const Array& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    // Tally
    map<CHivemindAddress, tallyitem> mapTally;
    map<uint256, CWalletTx>::iterator it;
    for(it=pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = it->second;

        if (wtx.IsCoinBase() || !IsFinalTx(wtx))
            continue;

        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < nMinDepth)
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            isminefilter mine = IsMine(*pwalletMain, address);
            if(!(mine & filter))
                continue;

            tallyitem& item = mapTally[address];
            item.nAmount += txout.nValue;
            item.nConf = min(item.nConf, nDepth);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
                item.fIsWatchonly = true;
        }
    }

    // Reply
    Array ret;
    map<string, tallyitem> mapAccountTally;
    BOOST_FOREACH(const PAIRTYPE(CHivemindAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
    {
        const CHivemindAddress& address = item.first;
        const string& strAccount = item.second.name;
        map<CHivemindAddress, tallyitem>::iterator it = mapTally.find(address);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        CAmount nAmount = 0;
        int nConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        if (it != mapTally.end())
        {
            nAmount = (*it).second.nAmount;
            nConf = (*it).second.nConf;
            fIsWatchonly = (*it).second.fIsWatchonly;
        }

        if (fByAccounts)
        {
            tallyitem& item = mapAccountTally[strAccount];
            item.nAmount += nAmount;
            item.nConf = min(item.nConf, nConf);
            item.fIsWatchonly = fIsWatchonly;
        }
        else
        {
            Object obj;
            if(fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("address",       address.ToString()));
            obj.push_back(Pair("account",       strAccount));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            Array transactions;
            if (it != mapTally.end())
            {
                BOOST_FOREACH(const uint256& item, (*it).second.txids)
                {
                    transactions.push_back(item.GetHex());
                }
            }
            obj.push_back(Pair("txids", transactions));
            ret.push_back(obj);
        }
    }

    if (fByAccounts)
    {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it)
        {
            CAmount nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            Object obj;
            if((*it).second.fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("account",       (*it).first));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

Value listreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listreceivedbyaddress ( minconf includeempty includeWatchonly)\n"
            "\nList balances by receiving address.\n"
            "\nArguments:\n"
            "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty  (numeric, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,        (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"account\" : \"accountname\",       (string) The account of the receiving address. The default account is \"\".\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in btc received by the address\n"
            "    \"confirmations\" : n                (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "6 true")
            + HelpExampleRpc("listreceivedbyaddress", "6, true, true")
        );

    return ListReceived(params, false);
}

Value listreceivedbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listreceivedbyaccount ( minconf includeempty includeWatchonly)\n"
            "\nList balances by account.\n"
            "\nArguments:\n"
            "1. minconf      (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty (boolean, optional, default=false) Whether to include accounts that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,   (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"account\" : \"accountname\",  (string) The account name of the receiving account\n"
            "    \"amount\" : x.xxx,             (numeric) The total amount received by addresses with this account\n"
            "    \"confirmations\" : n           (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaccount", "")
            + HelpExampleCli("listreceivedbyaccount", "6 true")
            + HelpExampleRpc("listreceivedbyaccount", "6, true, true")
        );

    return ListReceived(params, true);
}

static void MaybePushAddress(Object & entry, const CTxDestination &dest)
{
    CHivemindAddress addr;
    if (addr.Set(dest))
        entry.push_back(Pair("address", addr.ToString()));
}

void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, Array& ret, const isminefilter& filter)
{
    CAmount nFee;
    string strSentAccount;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);

    bool fAllAccounts = (strAccount == string("*"));
    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount))
    {
        BOOST_FOREACH(const COutputEntry& s, listSent)
        {
            Object entry;
            if(involvesWatchonly || (::IsMine(*pwalletMain, s.destination) & ISMINE_WATCH_ONLY))
                entry.push_back(Pair("involvesWatchonly", true));
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.destination);
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.amount)));
            entry.push_back(Pair("vout", s.vout));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            ret.push_back(entry);
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth)
    {
        BOOST_FOREACH(const COutputEntry& r, listReceived)
        {
            string account;
            if (pwalletMain->mapAddressBook.count(r.destination))
                account = pwalletMain->mapAddressBook[r.destination].name;
            if (fAllAccounts || (account == strAccount))
            {
                Object entry;
                if(involvesWatchonly || (::IsMine(*pwalletMain, r.destination) & ISMINE_WATCH_ONLY))
                    entry.push_back(Pair("involvesWatchonly", true));
                entry.push_back(Pair("account", account));
                MaybePushAddress(entry, r.destination);
                if (wtx.IsCoinBase())
                {
                    if (wtx.GetDepthInMainChain() < 1)
                        entry.push_back(Pair("category", "orphan"));
                    else if (wtx.GetBlocksToMaturity() > 0)
                        entry.push_back(Pair("category", "immature"));
                    else
                        entry.push_back(Pair("category", "generate"));
                }
                else
                {
                    entry.push_back(Pair("category", "receive"));
                }
                entry.push_back(Pair("amount", ValueFromAmount(r.amount)));
                entry.push_back(Pair("vout", r.vout));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
        }
    }
}

void AcentryToJSON(const CAccountingEntry& acentry, const string& strAccount, Array& ret)
{
    bool fAllAccounts = (strAccount == string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount)
    {
        Object entry;
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}

Value listtransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw runtime_error(
            "listtransactions ( \"account\" count from includeWatchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"    (string, optional) The account name. If not included, it will list all transactions for all accounts.\n"
            "                                     If \"\" is set, it will list transactions for the default account.\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"account\":\"accountname\",       (string) The account name associated with the transaction. \n"
            "                                                It will be \"\" for the default account.\n"
            "    \"address\":\"hivemindaddress\",    (string) The hivemind address of the transaction. Not present for \n"
            "                                                move transactions (category = move).\n"
            "    \"category\":\"send|receive|move\", (string) The transaction category. 'move' is a local (off blockchain)\n"
            "                                                transaction between accounts, and not associated with an address,\n"
            "                                                transaction id or block. 'send' and 'receive' transactions are \n"
            "                                                associated with an address, transaction id and block details\n"
            "    \"amount\": x.xxx,          (numeric) The amount in btc. This is negative for the 'send' category, and for the\n"
            "                                         'move' category for moves outbound. It is positive for the 'receive' category,\n"
            "                                         and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in btc. This is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"txid\": \"transactionid\", (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"otheraccount\": \"accountname\",  (string) For the 'move' category of transactions, the account the funds came \n"
            "                                          from (for receiving funds, positive amounts), or went to (for sending funds,\n"
            "                                          negative amounts).\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listtransactions", "") +
            "\nList the most recent 10 transactions for the tabby account\n"
            + HelpExampleCli("listtransactions", "\"tabby\"") +
            "\nList transactions 100 to 120 from the tabby account\n"
            + HelpExampleCli("listtransactions", "\"tabby\" 20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listtransactions", "\"tabby\", 20, 100")
        );

    string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 3)
        if(params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    Array ret;

    std::list<CAccountingEntry> acentries;
    CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        if (pwtx != 0)
            ListTransactions(*pwtx, strAccount, 0, true, ret, filter);
        CAccountingEntry *const pacentry = (*it).second.second;
        if (pacentry != 0)
            AcentryToJSON(*pacentry, strAccount, ret);

        if ((int)ret.size() >= (nCount+nFrom)) break;
    }
    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;
    Array::iterator first = ret.begin();
    std::advance(first, nFrom);
    Array::iterator last = ret.begin();
    std::advance(last, nFrom+nCount);

    if (last != ret.end()) ret.erase(last, ret.end());
    if (first != ret.begin()) ret.erase(ret.begin(), first);

    std::reverse(ret.begin(), ret.end()); // Return oldest to newest

    return ret;
}

Value listaccounts(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listaccounts ( minconf includeWatchonly)\n"
            "\nReturns Object that has account names as keys, account balances as values.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) Only include transactions with at least this many confirmations\n"
            "2. includeWatchonly (bool, optional, default=false) Include balances in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "{                      (json object where keys are account names, and values are numeric balances\n"
            "  \"account\": x.xxx,  (numeric) The property name is the account name, and the value is the total balance for the account.\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            "\nList account balances where there at least 1 confirmation\n"
            + HelpExampleCli("listaccounts", "") +
            "\nList account balances including zero confirmation transactions\n"
            + HelpExampleCli("listaccounts", "0") +
            "\nList account balances for 6 or more confirmations\n"
            + HelpExampleCli("listaccounts", "6") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("listaccounts", "6")
        );

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();
    isminefilter includeWatchonly = ISMINE_SPENDABLE;
    if(params.size() > 1)
        if(params[1].get_bool())
            includeWatchonly = includeWatchonly | ISMINE_WATCH_ONLY;

    map<string, CAmount> mapAccountBalances;
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, CAddressBookData)& entry, pwalletMain->mapAddressBook) {
        if (IsMine(*pwalletMain, entry.first) & includeWatchonly) // This address belongs to me
            mapAccountBalances[entry.second.name] = 0;
    }

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        CAmount nFee;
        string strSentAccount;
        list<COutputEntry> listReceived;
        list<COutputEntry> listSent;
        int nDepth = wtx.GetDepthInMainChain();
        if (wtx.GetBlocksToMaturity() > 0 || nDepth < 0)
            continue;
        wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, includeWatchonly);
        mapAccountBalances[strSentAccount] -= nFee;
        BOOST_FOREACH(const COutputEntry& s, listSent)
            mapAccountBalances[strSentAccount] -= s.amount;
        if (nDepth >= nMinDepth)
        {
            BOOST_FOREACH(const COutputEntry& r, listReceived)
                if (pwalletMain->mapAddressBook.count(r.destination))
                    mapAccountBalances[pwalletMain->mapAddressBook[r.destination].name] += r.amount;
                else
                    mapAccountBalances[""] += r.amount;
        }
    }

    list<CAccountingEntry> acentries;
    CWalletDB(pwalletMain->strWalletFile).ListAccountCreditDebit("*", acentries);
    BOOST_FOREACH(const CAccountingEntry& entry, acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    Object ret;
    BOOST_FOREACH(const PAIRTYPE(string, CAmount)& accountBalance, mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

Value listsinceblock(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "listsinceblock ( \"blockhash\" target-confirmations includeWatchonly)\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, optional) The block hash to list transactions since\n"
            "2. target-confirmations:    (numeric, optional) The confirmations required, must be 1 or more\n"
            "3. includeWatchonly:        (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')"
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"account\":\"accountname\",       (string) The account name associated with the transaction. Will be \"\" for the default account.\n"
            "    \"address\":\"hivemindaddress\",    (string) The hivemind address of the transaction. Not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",     (string) The transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in btc. This is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. It is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in btc. This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",     (string) The block hash containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",  (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (Jan 1 1970 GMT). Available for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"to\": \"...\",            (string) If a comment to is associated with the transaction.\n"
             "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the last block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listsinceblock", "")
            + HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        );

    CBlockIndex *pindex = NULL;
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (params.size() > 0)
    {
        uint256 blockId;

        blockId.SetHex(params[0].get_str());
        BlockMap::iterator it = mapBlockIndex.find(blockId);
        if (it != mapBlockIndex.end())
            pindex = it->second;
    }

    if (params.size() > 1)
    {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
    }

    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    int depth = pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1;

    Array transactions;

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); it++)
    {
        CWalletTx tx = (*it).second;

        if (depth == -1 || tx.GetDepthInMainChain() < depth)
            ListTransactions(tx, "*", 0, true, transactions, filter);
    }

    CBlockIndex *pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : uint256();

    Object ret;
    ret.push_back(Pair("transactions", transactions));
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;
}

Value gettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "gettransaction \"txid\" ( includeWatchonly )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "2. \"includeWatchonly\"    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"
            "\nResult:\n"
            "{\n"
            "  \"amount\" : x.xxx,        (numeric) The transaction amount in btc\n"
            "  \"confirmations\" : n,     (numeric) The number of confirmations\n"
            "  \"blockhash\" : \"hash\",  (string) The block hash\n"
            "  \"blockindex\" : xx,       (numeric) The block index\n"
            "  \"blocktime\" : ttt,       (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id.\n"
            "  \"time\" : ttt,            (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,    (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"account\" : \"accountname\",  (string) The account name involved in the transaction, can be \"\" for the default account.\n"
            "      \"address\" : \"hivemindaddress\",   (string) The hivemind address involved in the transaction\n"
            "      \"category\" : \"send|receive\",    (string) The category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx                  (numeric) The amount in btc\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"         (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    uint256 hash;
    hash.SetHex(params[0].get_str());

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 1)
        if(params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    Object entry;
    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    CAmount nCredit = wtx.GetCredit(filter);
    CAmount nDebit = wtx.GetDebit(filter);
    CAmount nNet = nCredit - nDebit;
    CAmount nFee = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

    entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
    if (wtx.IsFromMe(filter))
        entry.push_back(Pair("fee", ValueFromAmount(nFee)));

    WalletTxToJSON(wtx, entry);

    Array details;
    ListTransactions(wtx, "*", 0, false, details, filter);
    entry.push_back(Pair("details", details));

    string strHex = EncodeHexTx(static_cast<CTransaction>(wtx));
    entry.push_back(Pair("hex", strHex));

    return entry;
}


Value backupwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "backupwallet \"destination\"\n"
            "\nSafely copies wallet.dat to destination, which can be a directory or a path with filename.\n"
            "\nArguments:\n"
            "1. \"destination\"   (string) The destination directory or file\n"
            "\nExamples:\n"
            + HelpExampleCli("backupwallet", "\"backup.dat\"")
            + HelpExampleRpc("backupwallet", "\"backup.dat\"")
        );

    string strDest = params[0].get_str();
    if (!BackupWallet(*pwalletMain, strDest))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet backup failed!");

    return Value::null;
}


Value keypoolrefill(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "keypoolrefill ( newsize )\n"
            "\nFills the keypool."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        );

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize = 0;
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)params[0].get_int();
    }

    EnsureWalletIsUnlocked();
    pwalletMain->TopUpKeyPool(kpSize);

    if (pwalletMain->GetKeyPoolSize() < kpSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return Value::null;
}


static void LockWallet(CWallet* pWallet)
{
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = 0;
    pWallet->Lock();
}

Value walletpassphrase(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrase \"passphrase\" timeout\n"
            "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
            "This is needed prior to performing transactions related to private keys such as sending hiveminds\n"
            "\nArguments:\n"
            "1. \"passphrase\"     (string, required) The wallet passphrase\n"
            "2. timeout            (numeric, required) The time to keep the decryption key in seconds.\n"
            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nExamples:\n"
            "\nunlock the wallet for 60 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nLock the wallet again (before 60 seconds)\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60")
        );

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() > 0)
    {
        if (!pwalletMain->Unlock(strWalletPass))
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }
    else
        throw runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");

    pwalletMain->TopUpKeyPool();

    int64_t nSleepTime = params[1].get_int64();
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = GetTime() + nSleepTime;
    RPCRunLater("lockwallet", boost::bind(LockWallet, pwalletMain), nSleepTime);

    return Value::null;
}


Value walletpassphrasechange(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrasechange \"oldpassphrase\" \"newpassphrase\"\n"
            "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"
            "\nArguments:\n"
            "1. \"oldpassphrase\"      (string) The current passphrase\n"
            "2. \"newpassphrase\"      (string) The new passphrase\n"
            "\nExamples:\n"
            + HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"")
            + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\"")
        );

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    return Value::null;
}


Value walletlock(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error(
            "walletlock\n"
            "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"
            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletlock", "")
        );

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");

    {
        LOCK(cs_nWalletUnlockTime);
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return Value::null;
}


Value encryptwallet(const Array& params, bool fHelp)
{
    if (!pwalletMain->IsCrypted() && (fHelp || params.size() != 1))
        throw runtime_error(
            "encryptwallet \"passphrase\"\n"
            "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
            "After this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "Use the walletpassphrase call for this, and then walletlock call.\n"
            "If the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "Note that this will shutdown the server.\n"
            "\nArguments:\n"
            "1. \"passphrase\"    (string) The pass phrase to encrypt the wallet with. It must be at least 1 character, but should be long.\n"
            "\nExamples:\n"
            "\nEncrypt you wallet\n"
            + HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending hivemind\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can so something like sign\n"
            + HelpExampleCli("signmessage", "\"hivemindaddress\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("encryptwallet", "\"my pass phrase\"")
        );

    if (fHelp)
        return true;
    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; Hivemind server stopping, restart to run with encrypted wallet. The keypool has been flushed, you need to make a new backup.";
}

Value lockunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "lockunspent unlock [{\"txid\":\"txid\",\"vout\":n},...]\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending hiveminds.\n"
            "Locks are stored in memory only. Nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "Also see the listunspent call\n"
            "\nArguments:\n"
            "1. unlock            (boolean, required) Whether to unlock (true) or lock (false) the specified transactions\n"
            "2. \"transactions\"  (string, required) A json array of objects. Each object the txid (string) vout (numeric)\n"
            "     [           (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",    (string) The transaction id\n"
            "         \"vout\": n         (numeric) The output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "true|false    (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        );

    if (params.size() == 1)
        RPCTypeCheck(params, boost::assign::list_of(bool_type));
    else
        RPCTypeCheck(params, boost::assign::list_of(bool_type)(array_type));

    bool fUnlock = params[0].get_bool();

    if (params.size() == 1) {
        if (fUnlock)
            pwalletMain->UnlockAllCoins();
        return true;
    }

    Array outputs = params[1].get_array();
    BOOST_FOREACH(Value& output, outputs)
    {
        if (output.type() != obj_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");
        const Object& o = output.get_obj();

        RPCTypeCheck(o, boost::assign::map_list_of("txid", str_type)("vout", int_type));

        string txid = find_value(o, "txid").get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        COutPoint outpt(uint256S(txid), nOutput);

        if (fUnlock)
            pwalletMain->UnlockCoin(outpt);
        else
            pwalletMain->LockCoin(outpt);
    }

    return true;
}

Value listlockunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "listlockunspent\n"
            "\nReturns list of temporarily unspendable outputs.\n"
            "See the lockunspent call to lock and unlock transactions for spending.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listlockunspent", "")
        );

    vector<COutPoint> vOutpts;
    pwalletMain->ListLockedCoins(vOutpts);

    Array ret;

    BOOST_FOREACH(COutPoint &outpt, vOutpts) {
        Object o;

        o.push_back(Pair("txid", outpt.hash.GetHex()));
        o.push_back(Pair("vout", (int)outpt.n));
        ret.push_back(o);
    }

    return ret;
}

Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "settxfee amount\n"
            "\nSet the transaction fee per kB.\n"
            "\nArguments:\n"
            "1. amount         (numeric, required) The transaction fee in BTC/kB rounded to the nearest 0.00000001\n"
            "\nResult\n"
            "true|false        (boolean) Returns true if successful\n"
            "\nExamples:\n"
            + HelpExampleCli("settxfee", "0.00001")
            + HelpExampleRpc("settxfee", "0.00001")
        );

    // Amount
    CAmount nAmount = 0;
    if (params[0].get_real() != 0.0)
        nAmount = AmountFromValue(params[0]);        // rejects 0.0 amounts

    payTxFee = CFeeRate(nAmount, 1000);
    return true;
}

Value getwalletinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getwalletinfo\n"
            "Returns an object containing various wallet state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total confirmed hivemind balance of the wallet\n"
            "  \"unconfirmed_balance\": xxx, (numeric) the total unconfirmed hivemind balance of the wallet\n"
            "  \"immature_balance\": xxxxxx, (numeric) the total immature balance of the wallet\n"
            "  \"txcount\": xxxxxxx,         (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletinfo", "")
            + HelpExampleRpc("getwalletinfo", "")
        );

    Object obj;
    obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("unconfirmed_balance", ValueFromAmount(pwalletMain->GetUnconfirmedBalance())));
    obj.push_back(Pair("immature_balance",    ValueFromAmount(pwalletMain->GetImmatureBalance())));
    obj.push_back(Pair("txcount",       (int)pwalletMain->mapWallet.size()));
    obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    return obj;
}

static inline uint64_t rounduint64(double d)
{
    return (uint64_t)(d + 0.5);
}

static inline int64_t roundint64(double d)
{
    return (int64_t) ((d > 0.0)? (d + 0.5): (d - 0.5));
}

int64_t int64FromValue(const Value& value)
{
    double dAmount = value.get_real();
    return roundint64(dAmount * COIN);
}

uint64_t uint64FromValue(const Value &value, bool allow_zero)
{
    double dAmount = value.get_real();
    if (dAmount < 0.0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Negative amount");
    if ((!allow_zero) && (dAmount == 0.0))
        throw JSONRPCError(RPC_TYPE_ERROR, "Zero amount");
    return rounduint64(dAmount * COIN);
}

Value listbranches(const Array &params, bool fHelp)
{
    string strHelp =
        "listbranches"
        "\nReturns an array of all branches.";

    if (fHelp || (params.size() != 0))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;

    Array array;
    vector<marketBranch *> vec = pmarkettree->GetBranches();
    for(size_t i=0; i < vec.size(); i++) {
        const marketBranch *obj = vec[i];
        Object item;
        item.push_back(Pair("branchid", obj->GetHash().ToString()));
        item.push_back(Pair("txid", obj->txid.ToString()));
        item.push_back(Pair("name", obj->name));
        item.push_back(Pair("description", obj->description));
        item.push_back(Pair("baselistingfee", ValueFromAmount(obj->baseListingFee)));
        item.push_back(Pair("freedecisions", (int)obj->freeDecisions));
        item.push_back(Pair("targetdecisions", (int)obj->targetDecisions));
        item.push_back(Pair("maxdecisions", (int)obj->maxDecisions));
        item.push_back(Pair("mintradingfee", ValueFromAmount(obj->minTradingFee)));
        item.push_back(Pair("tau", (int)obj->tau));
        item.push_back(Pair("ballottime", (int)obj->ballotTime));
        item.push_back(Pair("unsealtime", (int)obj->unsealTime));
        item.push_back(Pair("consensusthreshold", ValueFromAmount(obj->consensusThreshold)));
        item.push_back(Pair("alpha", ValueFromAmount(obj->alpha)));
        item.push_back(Pair("tol", ValueFromAmount(obj->tol)));
        array.push_back(item);
    }
    entry.push_back(Pair("decisions", array));

    /* clean up */
    for(size_t i=0; i < vec.size(); i++)
        delete vec[i];

    return entry;
}

Value listdecisions(const Array &params, bool fHelp)
{
    string strHelp =
        "listdecisions branchid"
        "\nReturns an array of all decisions within the branch."
        "\nArguments:"
        "\n1. branchid     (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;

    uint256 branchid;
    branchid.SetHex(params[0].get_str());
    entry.push_back(Pair("branchid", branchid.ToString()));

    Array array;
    vector<marketDecision *> vec = pmarkettree->GetDecisions(branchid);
    for(size_t i=0; i < vec.size(); i++) {
        const marketDecision *obj = vec[i];
        Object item;
        item.push_back(Pair("decisionid", obj->GetHash().ToString()));
        item.push_back(Pair("txid", obj->txid.ToString()));
        CHivemindAddress addr;
        if (addr.Set(obj->keyID))
            item.push_back(Pair("keyID", addr.ToString()));
        item.push_back(Pair("branchid", obj->branchid.ToString()));
        item.push_back(Pair("prompt", obj->prompt));
        item.push_back(Pair("eventoverby", (int)obj->eventOverBy));
        item.push_back(Pair("isScaled", (int)obj->isScaled));
        item.push_back(Pair("min", ValueFromAmount(obj->min)));
        item.push_back(Pair("max", ValueFromAmount(obj->max)));
        item.push_back(Pair("answerOptionality", (int)obj->answerOptionality));
        array.push_back(item);
    }
    entry.push_back(Pair("decisions", array));

    /* clean up */
    for(size_t i=0; i < vec.size(); i++)
        delete vec[i];

    return entry;
}

Value listmarkets(const Array &params, bool fHelp)
{
    string strHelp =
        "listmarkets decisionid"
        "\nReturns an array of all markets depending on the decision."
        "\nArguments:"
        "\n1. decisionid      (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;

    uint256 decisionid;
    decisionid.SetHex(params[0].get_str());
    entry.push_back(Pair("decisionid", decisionid.ToString()));

    Array array;
    vector<marketMarket *> vec = pmarkettree->GetMarkets(decisionid);
    for(size_t i=0; i < vec.size(); i++) {
        const marketMarket *obj = vec[i];
        Object item;
        item.push_back(Pair("marketid", obj->GetHash().ToString()));
        item.push_back(Pair("txid", obj->txid.ToString()));
        CHivemindAddress addr;
        if (addr.Set(obj->keyID))
            item.push_back(Pair("keyID", addr.ToString()));
        item.push_back(Pair("B", ValueFromAmount(obj->B)));
        item.push_back(Pair("tradingFee", ValueFromAmount(obj->tradingFee)));
        item.push_back(Pair("maxCommission", ValueFromAmount(obj->maxCommission)));
        item.push_back(Pair("title", obj->title));
        item.push_back(Pair("description", obj->description));
        item.push_back(Pair("tags", obj->tags));
        item.push_back(Pair("maturation", (int)obj->maturation));
        Array darray; /* decision array */
        for(uint32_t i=0; i < obj->decisionIDs.size(); i++)
            darray.push_back(obj->decisionIDs[i].ToString());
        item.push_back(Pair("decisionIDs", darray));
        array.push_back(item);
    }
    entry.push_back(Pair("markets", array));

    /* clean up */
    for(size_t i=0; i < vec.size(); i++)
        delete vec[i];

    return entry;
}

Value listoutcomes(const Array &params, bool fHelp)
{
    string strHelp =
        "listoutcomes branchid"
        "\nReturns an array of all outcomes in the branch."
        "\nArguments:"
        "\n1. branchid        (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;

    uint256 branchid;
    branchid.SetHex(params[0].get_str());
    entry.push_back(Pair("branchid", branchid.ToString()));

    Array array;
    vector<marketOutcome *> vec = pmarkettree->GetOutcomes(branchid);
    for(size_t i=0; i < vec.size(); i++) {
        const marketOutcome *obj = vec[i];
        Object item;
        item.push_back(Pair("outcomeid", obj->GetHash().ToString()));
        item.push_back(Pair("height", (int)obj->nHeight));
        array.push_back(item);
    }
    entry.push_back(Pair("outcomes", array));

    /* clean up */
    for(size_t i=0; i < vec.size(); i++)
        delete vec[i];

    return entry;
}

Value listtrades(const Array& params, bool fHelp)
{
    string strHelp =
        "listtrades marketid"
        "\nReturns an array of all trades for the market."
        "\nArguments:"
        "\n1. tradeid      (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;

    uint256 marketid;
    marketid.SetHex(params[0].get_str());
    entry.push_back(Pair("marketid", marketid.ToString()));

    Array array;
    vector<marketTrade *> vec = pmarkettree->GetTrades(marketid);
    for(size_t i=0; i < vec.size(); i++) {
        const marketTrade *obj = vec[i];
        Object item;
        item.push_back(Pair("tradeid", obj->GetHash().ToString()));
        item.push_back(Pair("txid", obj->txid.ToString()));
        CHivemindAddress addr;
        if (addr.Set(obj->keyID))
            item.push_back(Pair("keyID", addr.ToString()));
        item.push_back(Pair("marketid", obj->marketid.ToString()));
        item.push_back(Pair("isbuy", (obj->isBuy)? "buy":"sell"));
        item.push_back(Pair("nshares", ValueFromAmount(obj->nShares)));
        item.push_back(Pair("price", ValueFromAmount(obj->price)));
        item.push_back(Pair("decision_state", (int)obj->decisionState));
        item.push_back(Pair("nonce", (int)obj->nonce));
        array.push_back(item);
    }
    entry.push_back(Pair("trades", array));

    /* clean up */
    for(size_t i=0; i < vec.size(); i++)
        delete vec[i];

    return entry;
}

Value listvotes(const Array &params, bool fHelp)
{
    string strHelp =
        "listvotes branchid height "
        "\nReturns an array of all votes for the ballot."
        "\nArguments:"
        "\n1. branchid     (uint256 string)"
        "\n2. height       (numeric)";

    if (fHelp || (params.size() < 2))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;

    uint256 branchid;
    branchid.SetHex(params[0].get_str());
    entry.push_back(Pair("branchid", branchid.ToString()));
    uint32_t height = (unsigned int)params[1].get_int();
    entry.push_back(Pair("height", (int)height));

    Array array;
    vector<marketRevealVote *> vec = pmarkettree->GetRevealVotes(branchid, height);
    for(size_t i=0; i < vec.size(); i++) {
        const marketRevealVote *obj = vec[i];
        Object item;
        item.push_back(Pair("voteid", obj->ToString()));
        item.push_back(Pair("txid", obj->txid.ToString()));
        item.push_back(Pair("NA", ValueFromAmount(obj->NA)));
        Array decision_array;
        for(uint32_t i=0; i < obj->decisionIDs.size(); i++) {
            Object decision;
            decision.push_back(Pair("decisionid", obj->decisionIDs[i].ToString()));
            if (i < obj->decisionVotes.size())
                decision.push_back(Pair("vote", ValueFromAmount(obj->decisionVotes[i])));
            decision_array.push_back(decision);
        }
        array.push_back(item);
    }
    entry.push_back(Pair("votes", array));

    /* clean up */
    for(size_t i=0; i < vec.size(); i++)
        delete vec[i];

    return entry;
}

Value createbranch(const Array& params, bool fHelp)
{
    string strHelp =
        "createbranch name description baselistingfee"
        " freedecisions targetdecisions maxdecisions"
        " mintradingfee"
        " tau ballottime unsealtime"
        " consensusthreshold"
        "\nCreates a new branch."
        "\n1. name                (string) the name of the branch"
        "\n2. description         (string) a short description of the branch"
        "\n3. baselistingfee      (numeric)"
        "\n4. freedecisions       (numeric < 65536)"
        "\n5. targetdecisions     (numeric < 65536)"
        "\n6. maxdecisions        (numeric < 65536)"
        "\n7. mintradingfee       (numeric)"
        "\n8. tau                 (block number < 65536)"
        "\n9. ballottime          (block number < 65536)"
        "\n10. unsealtime         (block number < 65536)"
        "\n11. consensusthreshold (numeric)"
        "\n12. alpha (numeric)"
        "\n13. tol (numeric)";

    if (fHelp || (params.size() != 13))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    EnsureWalletIsUnlocked();

    struct marketBranch obj;
    obj.name = params[0].get_str();
    obj.description = params[1].get_str();
    obj.baseListingFee = uint64FromValue(params[2], false);
    obj.freeDecisions = (uint16_t)params[3].get_int();
    obj.targetDecisions = (uint16_t)params[4].get_int();
    obj.maxDecisions = (uint16_t)params[5].get_int();
    obj.minTradingFee = uint64FromValue(params[6], false);
    obj.tau = (uint16_t)params[7].get_int();
    obj.ballotTime = (uint16_t)params[8].get_int();
    obj.unsealTime = (uint16_t)params[9].get_int();
    obj.consensusThreshold = uint64FromValue(params[10], false);
    obj.alpha = uint64FromValue(params[11], false);
    obj.tol = uint64FromValue(params[12], false);

    // double-check object is not a duplicate
    uint256 objid = obj.GetHash();
    marketBranch *tmpbranch = pmarkettree->GetBranch(objid);
    if (tmpbranch) {
        delete tmpbranch;
        string strError = std::string("Error: branchid ")
            + objid.ToString() + " already exists!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    // ensure unlocked wallet
    string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("createbranch() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // create output script
    CScript scriptPubKey = obj.GetScript();

    // key to send the change to
    CReserveKey reservekey(pwalletMain);

    // fee to create branch
    CAmount nFeeRequired;
    CAmount nAmount = (int64_t) rounduint64(0.01*COIN);

    string strAccount;
    CWalletTx wtx;
    CTxDestination txDestChange;
    if (!pwalletMain->CreateTransaction(scriptPubKey, strAccount, nAmount, wtx,
        reservekey, nFeeRequired, strError, txDestChange))
    {
        if (nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(
                "Error: This transaction requires a transaction fee of at least %s!",
                FormatMoney(nFeeRequired));
        LogPrintf("createbranch() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtx, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
            "Error: The createbranch transaction was rejected!");

    Object entry;
    entry.push_back(Pair("txid", wtx.GetHash().ToString()));
    entry.push_back(Pair("branchid", objid.ToString()));
    return entry;
}

Value createdecision(const Array& params, bool fHelp)
{
    string strHelp =
        "createdecision branchid prompt address eventoverby [scaled min max]"
        "\nCreates a new decision within the branch."
        "\n1. address             (base58 address)"
        "\n2. branchid            (uint256 string)"
        "\n3. prompt              (string)"
        "\n4. eventoverby         (block number)"
        "\n5. answer optionality  (false=mandatory to answer, true=optional to answer)"
        "\n6. is_scaled           (boolean)"
        "\n7. scaled min          (if scaled, numeric)"
        "\n8. scaled max          (if scaled, numeric)";

    if (fHelp || (params.size() != 6 && params.size() != 8))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    EnsureWalletIsUnlocked();

    struct marketDecision obj;
    CHivemindAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
            "Invalid Hivemind address");
    address.GetKeyID(obj.keyID);
    obj.branchid.SetHex(params[1].get_str());
    obj.prompt = params[2].get_str();
    obj.eventOverBy = (uint32_t) params[3].get_int();
    obj.answerOptionality = (params[4].get_bool())? 1: 0;
    obj.isScaled = (params[5].get_bool())? 1: 0;
    if ((obj.isScaled) && (params.size() != 8))
        throw runtime_error(strHelp);
    obj.min = (params.size() == 6)? 0: int64FromValue(params[6]);
    obj.max = (params.size() == 6)? COIN: int64FromValue(params[7]);

    // double-check branch exists
    marketBranch *branch = pmarkettree->GetBranch(obj.branchid);
    if (!branch) {
        string strError = std::string("Error: branchid ")
            + obj.branchid.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    delete branch;

    // double-check object is not a duplicate
    uint256 objid = obj.GetHash();
    marketDecision *decision = pmarkettree->GetDecision(objid);
    if (decision) {
        delete decision;
        string strError = std::string("Error: decisionid ")
            + objid.ToString() + " already exists!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    // ensure unlocked wallet
    string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("createdecision() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // create output script
    CScript scriptPubKey = obj.GetScript();

    // key to send the change to
    CReserveKey reservekey(pwalletMain);

    // fee to create decision
    CAmount nFeeRequired;
    CAmount nAmount = (int64_t) rounduint64(0.01*COIN);

    string strAccount;
    CWalletTx wtx;
    CTxDestination txDestChange;
    if (!pwalletMain->CreateTransaction(scriptPubKey, strAccount, nAmount, wtx,
        reservekey, nFeeRequired, strError, txDestChange))
    {
        if (nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(
                "Error: This transaction requires a transaction fee of at least %s!",
                FormatMoney(nFeeRequired));
        LogPrintf("createdecision() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtx, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
            "Error: The createdecision transaction was rejected!");

    Object entry;
    entry.push_back(Pair("txid", wtx.GetHash().ToString()));
    entry.push_back(Pair("decisionid", objid.ToString()));

    return entry;
}

Value createmarket(const Array& params, bool fHelp)
{
    string strHelp =
        "createmarket address decisionid[,...] B tradingfee address title"
        " description tags[,...] maturation"
        "\nCreates a new market on the decisions."
        "\n1. address             (base58 address)"
        "\n2. decisionid[,...]    (comma-separated list of decisions)"
        "\n3. B                   (numeric) liquidity parameter"
        "\n4. tradingfee          (numeric)"
        "\n5. max commission      (numeric)"
        "\n6. title               (string)"
        "\n7. description         (string)"
        "\n8. tags[,...]          (comma-separated list of strings)"
        "\n9. maturation          (block number)"
        "\n10. tx PoW hash id     (numeric)"
        "\n11. tx PoW difficulty  (numeric)"
        "\nEach decisionid is a hash of a decision optionally followed by a function code."
        "\nThe available function codes are"
        "\n    :X1   X, identity [default]"
        "\n    :X2   X^2"
        "\n    :X3   X^3"
        "\n    :LNX1  LN(X)";

    if (fHelp || (params.size() != 11))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    EnsureWalletIsUnlocked();

    CHivemindAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind address");
    struct marketMarket obj;
    address.GetKeyID(obj.keyID);
    uint32_t nStates = 1;
    std::string param_decision = params[1].get_str();
    vector<string> strs;
    boost::split(strs, param_decision, boost::algorithm::is_any_of(", "));
    for(uint32_t i=0; i < strs.size(); i++) {
        uint256 decisionID;
        int decisionFunctionID = DFID_X1;
        size_t separator = strs[i].find(":");
        if (separator != std::string::npos) {
            std::string function_code = strs[i].substr(separator+1);
            decisionFunctionID = decisionFunctionToInt(function_code);
            if (decisionFunctionID < 0) {
                string strError = std::string("Error: decision function ")
                    + function_code + " does not exist!";
                throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
            }
            strs[i].erase(separator);
        }
        decisionID.SetHex(strs[i].c_str());

        marketDecision *decision = pmarkettree->GetDecision(decisionID);
        if (!decision) {
            string strError = std::string("Error: decisionID ")
                + decisionID.ToString() + " does not exist!";
            throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
        }
        delete decision;

        obj.decisionIDs.push_back(decisionID);
        obj.decisionFunctionIDs.push_back(decisionFunctionID);

        nStates *= 2;
    }
    if ((!obj.decisionIDs.size()) || (nStates == 1)) {
        string strError = std::string("Error: There are no decisionids!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    obj.B = uint64FromValue(params[2], false);
    obj.tradingFee = uint64FromValue(params[3], false);
    obj.maxCommission = uint64FromValue(params[4], false);
    obj.title = params[5].get_str();
    obj.description = params[6].get_str();
    obj.tags = params[7].get_str();
    obj.maturation = (uint32_t)params[8].get_int();
    obj.txPoWh = (uint32_t)params[9].get_int();
    obj.txPoWd = (uint32_t)params[10].get_int();
    double capitalrequired = marketAccountValue(obj.maxCommission, 1e-8*obj.B, nStates, NULL);

    // double-check object is not a duplicate
    uint256 objid = obj.GetHash();
    marketMarket *tmp = pmarkettree->GetMarket(objid);
    if (tmp) {
        delete tmp;
        string strError = std::string("Error: marketid ")
            + objid.ToString() + " already exists!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    // ensure unlocked wallet
    string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("createmarket() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // create output script
    CScript scriptPubKey = obj.GetScript();

    // key to send the change to
    CReserveKey reservekey(pwalletMain);

    // fee to create market
    CAmount nFeeRequired;
    CAmount nAmount = (int64_t) (rounduint64(capitalrequired + 0.01*COIN));

    string strAccount;
    CWalletTx wtx;
    CTxDestination txDestChange;
    if (!pwalletMain->CreateTransaction(scriptPubKey, strAccount, nAmount, wtx,
        reservekey, nFeeRequired, strError, txDestChange))
    {
        if (nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(
                "Error: This transaction requires a transaction fee of at least %s!",
                FormatMoney(nFeeRequired));
        LogPrintf("createmarket() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtx, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
            "Error: The createmarket transaction was rejected!");

    Object entry;
    entry.push_back(Pair("txid", wtx.GetHash().ToString()));
    entry.push_back(Pair("marketid", objid.ToString()));
    return entry;
}

Value createtrade(const Array& params, bool fHelp)
{
    string strHelp =
        "createtrade address marketid buy_or_sell number_shares"
        " price decision_state [nonce]"
        "\nCreates a new trade order in the market."
        "\n1. address             (base58 address)"
        "\n2. marketid            (u256 string)"
        "\n3. buy_or_sell         (string)"
        "\n4. number_shares       (numeric)"
        "\n5. price               (numeric)"
        "\n6. decision_state      (string)"
        "\n7. nonce               (optional numeric)"
        "\n"
        "\nNote: for repeated trades, increase the nonce.";

    if (fHelp || ((params.size() != 6) && (params.size() != 7)))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    EnsureWalletIsUnlocked();

    /* double-check address and create trade object */
    marketTrade obj;
    CHivemindAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Hivemind address");
    address.GetKeyID(obj.keyID);

    /* Add market id to the object, checked later (performance) */
    obj.marketid.SetHex(params[1].get_str());

    /* double-check buy_or_sell */
    string buy_or_sell = params[2].get_str();
    if ((buy_or_sell != "buy") && (buy_or_sell != "sell")) {
        string strError = std::string("Error: '")
            + buy_or_sell + "' must be buy or sell!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* double-check nShares */
    if (params[3].get_real() <= 0.0) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%.8f", params[3].get_real());
        string strError = std::string("Error: number of shares ")
            + tmp + " must be positive!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* double-check price */
    if (params[4].get_real() <= 0.0) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%.8f", params[4].get_real());
        string strError = std::string("Error: price ")
            + tmp + " must be positive!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* double-check marketid */
    marketMarket *market = pmarkettree->GetMarket(obj.marketid);
    if (!market) {
        string strError = std::string("Error: marketid ")
            + obj.marketid.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    // Add params to the trade object
    obj.isBuy = (buy_or_sell == "buy")? true: false;
    obj.nShares = uint64FromValue(params[3], false);
    obj.price = uint64FromValue(params[4], false);
    obj.decisionState = (uint32_t)params[5].get_int();
    obj.nonce = (params.size() != 7)? rand(): (uint32_t)params[6].get_int();

    /* double-check decisionState */
    uint32_t nStates = marketNStates(market);
    if (obj.decisionState >= nStates) {
        char tmp0[32];
        snprintf(tmp0, sizeof(tmp0), "%u", obj.decisionState);
        char tmp1[32];
        snprintf(tmp1, sizeof(tmp1), "%u", nStates);
        delete market;
        string strError = std::string("Error: decision state ")
            + tmp0 + " is above the number of states "
            + tmp1 + " in the market!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 objid = obj.GetHash();
    while(true) {
        marketTrade *tmptrade = pmarkettree->GetTrade(objid);
        if (tmptrade) {
            delete tmptrade;
            if (params.size() == 7) {
                delete market;
                string strError = std::string("Error: tradeid ")
                    + objid.ToString() + " already exists!";
                throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
            }
            else {
                obj.nonce++;
                objid = obj.GetHash();
                continue;
            }
        }
        break;
    }

    /* trades of the market */
    vector<marketTrade *> trades = pmarkettree->GetTrades(market->GetHash());

    /* current shares of the market */
    double *nShares = new double [nStates];
    marketNShares(trades, nStates, nShares);
    double currAccount = marketAccountValue(market->maxCommission, 1e-8*market->B, nStates, nShares);

    /* new shares to be added to the market */
    nShares[obj.decisionState] += (obj.isBuy)? 1e-8*obj.nShares: -1e-8*obj.nShares;
    double nextAccount = marketAccountValue(market->maxCommission, 1e-8*market->B, nStates, nShares);

    /* the price difference to move from the current to the new */
    double price = (obj.isBuy)? (nextAccount - currAccount) / (1e-8 * obj.nShares)
        : (currAccount - nextAccount) / (1e-8 * obj.nShares);
    double totalCost = price * (1e-8 * obj.nShares);

    if ((obj.isBuy) && (1e-8*price > 1e-8*obj.price)) {
        delete market;
        string strError = std::string("Error: price needs to be at least ")
            + FormatMoney(price);
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* ensure unlocked wallet */
    string strError;
    if (pwalletMain->IsLocked()) {
        delete market;
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("createtrade() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    /* create output script */
    CScript scriptPubKey = obj.GetScript();

    /* key to send the change to */
    CReserveKey reservekey(pwalletMain);

    /* fee to create trade */
    CAmount nFeeRequired;
    CAmount baseFee = (int64_t) rounduint64(0.01 * COIN);
    CAmount tradeFee = (int64_t) rounduint64((1e-8*totalCost) * COIN);

    CAmount nAmount = (int64_t) (baseFee + tradeFee);

    string strAccount;
    CWalletTx wtx;
    CTxDestination txDestChange;
    if (!pwalletMain->CreateTransaction(scriptPubKey, strAccount, nAmount, wtx,
        reservekey, nFeeRequired, strError, txDestChange))
    {
        if (nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(
                "Error: This transaction requires a transaction fee of at least %s!",
                FormatMoney(nFeeRequired));
        LogPrintf("createtrade() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    if (!pwalletMain->CommitTransaction(wtx, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
            "Error: The createtrade transaction was rejected!");

    /* return value */
    Object entry;
    entry.push_back(Pair("txid", wtx.GetHash().ToString()));
    entry.push_back(Pair("tradeid", objid.ToString()));
    entry.push_back(Pair("B", 1e-8*market->B));
    entry.push_back(Pair("buy_or_sell", buy_or_sell));
    entry.push_back(Pair("nShares", nShares));
    entry.push_back(Pair("price", price));
    entry.push_back(Pair("total", ((1e-8 * obj.nShares))*price));

    /* clean up */
    delete market;
    delete nShares;
    for(uint32_t i=0; i < trades.size(); i++)
        delete trades[i];

    return entry;
}

Value createsealedvote(const Array& params, bool fHelp)
{
    string strHelp =
        "createsealedvote branchid height voteid"
        "\nCreates a new sealedvote for the branch height."
        "\n1. branchid            (u256 string)"
        "\n2. height              (numeric)"
        "\n3. voteid              (u256 string)";

    if (fHelp || (params.size() != 3))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    // Make sure wallet is unlocked
    EnsureWalletIsUnlocked();

    // Create sealed vote spirit object from params
    marketSealedVote obj;
    obj.branchid.SetHex(params[0].get_str());
    obj.height = params[1].get_int();
    obj.voteid.SetHex(params[2].get_str());

    // Validate branch ID
    marketBranch *branch = pmarkettree->GetBranch(obj.branchid);
    if (!branch) {
        string strError = std::string("Error: branchid ")
            + obj.branchid.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    uint32_t tau = branch->tau;
    delete branch; /* branch no longer needed */

    // Validate vote nHeight
    if (obj.height % tau != 0) {
        string strError = std::string("Error: Invalid height ")
            + " for the branch's tau!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    // ensure unlocked wallet
    string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("createsealedvote() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Create output script
    CScript scriptPubKey = obj.GetScript();

    // Key (changeaddr) to send the change to
    CReserveKey reservekey(pwalletMain);

    // Fee to create vote
    CAmount nFeeRequired;
    CAmount nAmount = (int64_t) rounduint64(0.01*COIN);

    // Use votecoins to pay for the vote
    string strAccount = obj.branchid.ToString();
    CHivemindAddress voterAddress = GetBranchAccountAddress(strAccount);

    CCoinControl *control = new CCoinControl;
    control->destChange = voterAddress.Get();

    CWalletTx wtx;
    CTxDestination txDestChange;
    if (!pwalletMain->CreateTransaction(scriptPubKey, strAccount, nAmount, wtx,
        reservekey, nFeeRequired, strError, txDestChange, control))
    {
        if (nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(
                "Error: This transaction requires a transaction fee of at least %s!",
                FormatMoney(nFeeRequired));
        LogPrintf("createsealedvote() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    delete control; /* control no longer needed */

    CPubKey key;
    reservekey.GetReservedKey(key);

    if (!pwalletMain->CommitTransaction(wtx, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
            "Error: The createsealedvote transaction was rejected!");

    Object entry;
    entry.push_back(Pair("txid", wtx.GetHash().ToString()));
    entry.push_back(Pair("sealedvoteid", obj.GetHash().ToString()));
    return entry;
}

Value createstealvote(const Array& params, bool fHelp)
{
    string strHelp =
        "createstealvote branchid height voteid "
        "\nCreates a new stealvote for the branch height."
        "\n1. branchid            (u256 string)"
        "\n2. height              (numeric)"
        "\n3. victim's voteid     (u256 string)";

    if (fHelp || (params.size() != 3))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    EnsureWalletIsUnlocked();

    marketStealVote obj;

    obj.branchid.SetHex(params[0].get_str());
    obj.height = params[1].get_int();
    obj.voteid.SetHex(params[2].get_str());

    /* double-check branchid */
    marketBranch *branch = pmarkettree->GetBranch(obj.branchid);
    if (!branch) {
        string strError = std::string("Error: branchid ")
            + obj.branchid.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    uint32_t tau = branch->tau;
    delete branch; /* branch no longer needed */

    /* double-check height */
    if (obj.height % tau != 0) {
        string strError = std::string("Error: Invalid height ")
            + " for the branch's tau!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* double-check voteid */
    marketSealedVote *sealedVote = pmarkettree->GetSealedVote(obj.voteid);
    if (!sealedVote) {
        string strError = std::string("Error: voteid ")
            + obj.voteid.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    delete sealedVote; /* sealedVote no longer needed */

    // ensure unlocked wallet
    string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("createstealvote() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // create output script
    CScript scriptPubKey = obj.GetScript();

    // key to send the change to
    CReserveKey reservekey(pwalletMain);

    // fee to create vote
    CAmount nFeeRequired;
    CAmount nAmount = (int64_t) rounduint64(0.01*COIN);

    // Use votecoins to pay for the vote
    string strAccount = obj.branchid.ToString();
    CHivemindAddress voterAddress = GetBranchAccountAddress(strAccount);

    CCoinControl *control = new CCoinControl;
    control->destChange = voterAddress.Get();

    CWalletTx wtx;
    CTxDestination txDestChange;
    if (!pwalletMain->CreateTransaction(scriptPubKey, strAccount, nAmount, wtx,
        reservekey, nFeeRequired, strError, txDestChange, control))
    {
        if (nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(
                "Error: This transaction requires a transaction fee of at least %s!",
                FormatMoney(nFeeRequired));
        LogPrintf("createstealvote() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtx, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
            "Error: The createstealvote transaction was rejected!");

    delete control; /* control no longer needed */

    Object entry;
    entry.push_back(Pair("txid", wtx.GetHash().ToString()));
    entry.push_back(Pair("stealvoteid", obj.GetHash().ToString()));
    return entry;
}

Value createrevealvote(const Array& params, bool fHelp)
{
    string strHelp =
        "createrevealvote address branchid height NA decisionid,vote [...]"
        "\nReveals the vote from a sealed vote."
        "\n1. votecoin_address    (base58 address)"
        "\n2. branchid            (u256 string)"
        "\n3. height              (numeric)"
        "\n4. voteid              (uint256)"
        "\n5. NA                  (numeric)"
        "\n6. decisionid,vote     (u256 string, numeric).";

    if (fHelp || (params.size() < 6))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    EnsureWalletIsUnlocked();

    marketRevealVote obj;

    CHivemindAddress address;
    address.is_votecoin = 1;
    address.SetString(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Votecoin address");
    address.GetKeyID(obj.keyID);
    obj.branchid.SetHex(params[1].get_str());
    obj.height = params[2].get_int();
    obj.voteid.SetHex(params[3].get_str());
    obj.NA = uint64FromValue(params[4], false);

    /* double-check branchid */
    marketBranch *branch = pmarkettree->GetBranch(obj.branchid);
    if (!branch) {
        string strError = std::string("Error: branchid ")
            + obj.branchid.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    uint32_t tau = branch->tau;
    delete branch; /* branch no longer needed */

    /* double-check height */
    if (obj.height % tau != 0) {
        string strError = std::string("Error: Invalid height ")
            + " for the branch's tau!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* double-check voteid */
    marketSealedVote *sealedVote = pmarkettree->GetSealedVote(obj.voteid);
    if (!sealedVote) {
        string strError = std::string("Error: voteid ")
            + obj.voteid.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    delete sealedVote; /* sealedVote no longer needed */

    /* record the ballot */
    vector<uint256> decisionIDs;
    vector<uint64_t> decisionVotes;
    for(uint32_t i=5; i < params.size(); i++) {
        string str = params[i].get_str();
        size_t separator = str.find(",");
        if (separator == std::string::npos) {
            string strError = std::string("Error: decisionid,vote ")
                + str + " is not in correct form!";
            throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
        }
        std::string vote = str.substr(separator+1);
        str.erase(separator);
        uint256 decisionid;
        decisionid.SetHex(str.c_str());
        obj.decisionIDs.push_back(decisionid);
        obj.decisionVotes.push_back(rounduint64(atof(vote.c_str())* COIN));
    }

    // ensure unlocked wallet
    string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("createrevealvote() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // create output script
    CScript scriptPubKey = obj.GetScript();

    // key to send the change to
    CReserveKey reservekey(pwalletMain);

    // fee to create vote
    CAmount nFeeRequired;
    CAmount nAmount = (int64_t) rounduint64(0.01*COIN);

    // Use votecoins to pay for the vote
    string strAccount = obj.branchid.ToString();
    CHivemindAddress voterAddress = GetBranchAccountAddress(strAccount);

    CCoinControl *control = new CCoinControl;
    control->destChange = voterAddress.Get();

    CWalletTx wtx;
    CTxDestination txDestChange;
    if (!pwalletMain->CreateTransaction(scriptPubKey, strAccount, nAmount, wtx,
        reservekey, nFeeRequired, strError, txDestChange, control))
    {
        if (nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(
                "Error: This transaction requires a transaction fee of at least %s!",
                FormatMoney(nFeeRequired));
        LogPrintf("createrevealvote() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtx, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
            "Error: The createrevealvote transaction was rejected!");

    delete control; /* control no longer needed */

    Object entry;
    entry.push_back(Pair("txid", wtx.GetHash().ToString()));
    entry.push_back(Pair("voteid", obj.GetHash().ToString()));
    return entry;
}

Value getbranch(const Array &params, bool fHelp)
{
    string strHelp =
        "getbranch branchid"
        "\nReturns the branch."
        "\n1. branchid            (u256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    marketBranch *obj = pmarkettree->GetBranch(id);
    if (!obj) {
        string strError = std::string("Error: branchid ")
            + id.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;
    entry.push_back(Pair("branchid", id.ToString()));
    entry.push_back(Pair("txid", obj->txid.ToString()));
    entry.push_back(Pair("name", obj->name));
    entry.push_back(Pair("description", obj->description));
    entry.push_back(Pair("baselistingfee", ValueFromAmount(obj->baseListingFee)));
    entry.push_back(Pair("freedecisions", (int)obj->freeDecisions));
    entry.push_back(Pair("targetdecisions", (int)obj->targetDecisions));
    entry.push_back(Pair("maxdecisions", (int)obj->maxDecisions));
    entry.push_back(Pair("mintradingfee", ValueFromAmount(obj->minTradingFee)));
    entry.push_back(Pair("tau", (int)obj->tau));
    entry.push_back(Pair("ballottime", (int)obj->ballotTime));
    entry.push_back(Pair("unsealtime", (int)obj->unsealTime));
    entry.push_back(Pair("consensusthreshold", ValueFromAmount(obj->consensusThreshold)));

    /* clean up */
    delete obj;

    return entry;
}

Value getdecision(const Array &params, bool fHelp)
{
    string strHelp =
        "getdecision decisionid"
        "\nReturns the decision."
        "\n1. decisionid          (u256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    marketDecision *obj = pmarkettree->GetDecision(id);
    if (!obj) {
        string strError = std::string("Error: decision for ")
            + id.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;
    entry.push_back(Pair("decisionid", id.ToString()));
    entry.push_back(Pair("txid", obj->txid.ToString()));
    CHivemindAddress addr;
    if (addr.Set(obj->keyID))
        entry.push_back(Pair("keyID", addr.ToString()));
    entry.push_back(Pair("branchid", obj->branchid.ToString()));
    entry.push_back(Pair("prompt", obj->prompt));
    entry.push_back(Pair("eventoverby", (int)obj->eventOverBy));
    entry.push_back(Pair("isScaled", (int)obj->isScaled));
    entry.push_back(Pair("min", ValueFromAmount(obj->min)));
    entry.push_back(Pair("max", ValueFromAmount(obj->max)));
    entry.push_back(Pair("answerOptionality", (int)obj->answerOptionality));
    return entry;
}

Value getmarket(const Array &params, bool fHelp)
{
    string strHelp =
        "getmarket marketid"
        "\nReturns the market."
        "\n1. marketid            (u256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    marketMarket *obj = pmarkettree->GetMarket(id);
    if (!obj) {
        string strError = std::string("Error: marketid for ")
            + id.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* trades of the market */
    vector<marketTrade *> trades = pmarkettree->GetTrades(obj->GetHash());

    /* current shares of the market */
    uint32_t nStates = marketNStates(obj);
    double *nShares = new double [nStates];
    marketNShares(trades, nStates, nShares);

    /* current account value */
    double currAccount = marketAccountValue(obj->maxCommission, 1e-8*obj->B, nStates, nShares);

    /* return value */
    Object entry;
    entry.push_back(Pair("marketid", id.ToString()));
    entry.push_back(Pair("txid", obj->txid.ToString()));
    CHivemindAddress addr;
    if (addr.Set(obj->keyID))
        entry.push_back(Pair("keyID", addr.ToString()));
    entry.push_back(Pair("B", ValueFromAmount(obj->B)));
    entry.push_back(Pair("tradingFee", ValueFromAmount(obj->tradingFee)));
    entry.push_back(Pair("maxCommission", ValueFromAmount(obj->maxCommission)));
    entry.push_back(Pair("title", obj->title));
    entry.push_back(Pair("description", obj->description));
    entry.push_back(Pair("tags", obj->tags));
    entry.push_back(Pair("maturation", (int)obj->maturation));
    entry.push_back(Pair("txPoWh", (int)obj->txPoWh));
    entry.push_back(Pair("txPoWd", (int)obj->txPoWd));
    Array array;
    for(uint32_t i=0; i < obj->decisionIDs.size(); i++) {
        string str = obj->decisionIDs[i].ToString();
        str += ":";
        if (i < obj->decisionFunctionIDs.size())
            str += decisionFunctionIDToString(obj->decisionFunctionIDs[i]);
        array.push_back(str);
    }
    entry.push_back(Pair("decisionIDs", array));
    for(uint32_t i=0; i < nStates; i++) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "nShares%u", i);
        entry.push_back(Pair(tmp, nShares[i]));
    }
    entry.push_back(Pair("currAccount", currAccount));

    /* clean up */
    delete obj;
    delete nShares;
    for(uint32_t i=0; i < trades.size(); i++)
        delete trades[i];

    return entry;
}

Value getoutcome(const Array &params, bool fHelp)
{
    string strHelp =
        "getoutcome outcomeid"
        "\nReturns the outcome."
        "\nArguments:"
        "\n1. outcomeid    (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    marketOutcome *obj = pmarkettree->GetOutcome(id);
    if (!obj) {
        string strError = std::string("Error: outcomeid for ")
            + id.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;
    entry.push_back(Pair("nVoters", (int)obj->nVoters));
    entry.push_back(Pair("nDecisions", (int)obj->nDecisions));
    entry.push_back(Pair("NA", ValueFromAmount(obj->NA)));
    entry.push_back(Pair("tol", ValueFromAmount(obj->tol)));
    entry.push_back(Pair("alpha", ValueFromAmount(obj->alpha)));

    Array voteMatrix;
    for(uint32_t i=0; i < obj->voteMatrix.size(); i++)
        voteMatrix.push_back(ValueFromAmount(obj->voteMatrix[i]));
    entry.push_back(Pair("voteMatrix", voteMatrix));

    /* Voter Vectors */
    Array voterIDs;
    for(uint32_t i=0; i < obj->voterIDs.size(); i++)
        voterIDs.push_back(obj->voterIDs[i].ToString());
    entry.push_back(Pair("voterIDs", voterIDs));

    Array oldRep;
    for(uint32_t i=0; i < obj->oldRep.size(); i++)
        oldRep.push_back(ValueFromAmount(obj->oldRep[i]));
    entry.push_back(Pair("oldRep", oldRep));

    Array thisRep;
    for(uint32_t i=0; i < obj->thisRep.size(); i++)
        thisRep.push_back(ValueFromAmount(obj->thisRep[i]));
    entry.push_back(Pair("thisRep", oldRep));

    Array smoothedRep;
    for(uint32_t i=0; i < obj->smoothedRep.size(); i++)
        smoothedRep.push_back(ValueFromAmount(obj->smoothedRep[i]));
    entry.push_back(Pair("smoothedRep", oldRep));

    Array NARow;
    for(uint32_t i=0; i < obj->NARow.size(); i++)
        NARow.push_back(ValueFromAmount(obj->NARow[i]));
    entry.push_back(Pair("NARow", oldRep));

    Array particRow;
    for(uint32_t i=0; i < obj->particRow.size(); i++)
        particRow.push_back(ValueFromAmount(obj->particRow[i]));
    entry.push_back(Pair("particRow", oldRep));

    Array particRel;
    for(uint32_t i=0; i < obj->particRel.size(); i++)
        particRel.push_back(ValueFromAmount(obj->particRel[i]));
    entry.push_back(Pair("particRel", oldRep));

    Array rowBonus;
    for(uint32_t i=0; i < obj->rowBonus.size(); i++)
        rowBonus.push_back(ValueFromAmount(obj->rowBonus[i]));
    entry.push_back(Pair("rowBonus", oldRep));

    /* Decision Vectors */

    Array decisionIDs;
    for(uint32_t i=0; i < obj->decisionIDs.size(); i++)
        decisionIDs.push_back(obj->decisionIDs[i].ToString());
    entry.push_back(Pair("decisionIDs", decisionIDs));

    Array isScaled;
    for(uint32_t i=0; i < obj->isScaled.size(); i++)
        isScaled.push_back(ValueFromAmount(obj->isScaled[i]));
    entry.push_back(Pair("isScaled", isScaled));

    Array firstLoading;
    for(uint32_t i=0; i < obj->firstLoading.size(); i++)
        firstLoading.push_back(ValueFromAmount(obj->firstLoading[i]));
    entry.push_back(Pair("firstLoading", firstLoading));

    Array decisionsRaw;
    for(uint32_t i=0; i < obj->decisionsRaw.size(); i++)
        decisionsRaw.push_back(ValueFromAmount(obj->decisionsRaw[i]));
    entry.push_back(Pair("decisionsRaw", decisionsRaw));

    Array consensusReward;
    for(uint32_t i=0; i < obj->consensusReward.size(); i++)
        consensusReward.push_back(ValueFromAmount(obj->consensusReward[i]));
    entry.push_back(Pair("consensusReward", consensusReward));

    Array certainty;
    for(uint32_t i=0; i < obj->certainty.size(); i++)
        certainty.push_back(ValueFromAmount(obj->certainty[i]));
    entry.push_back(Pair("certainty", certainty));

    Array NACol;
    for(uint32_t i=0; i < obj->NACol.size(); i++)
        NACol.push_back(ValueFromAmount(obj->NACol[i]));
    entry.push_back(Pair("NACol", NACol));

    Array particCol;
    for(uint32_t i=0; i < obj->particCol.size(); i++)
        particCol.push_back(ValueFromAmount(obj->particCol[i]));
    entry.push_back(Pair("particCol", particCol));

    Array authorBonus;
    for(uint32_t i=0; i < obj->authorBonus.size(); i++)
        authorBonus.push_back(ValueFromAmount(obj->authorBonus[i]));
    entry.push_back(Pair("authorBonus", authorBonus));

    Array decisionsFinal;
    for(uint32_t i=0; i < obj->decisionsFinal.size(); i++)
        decisionsFinal.push_back(ValueFromAmount(obj->decisionsFinal[i]));
    entry.push_back(Pair("decisionsFinal", decisionsFinal));

    delete obj;

    return entry;
}

Value gettrade(const Array& params, bool fHelp)
{
    string strHelp =
        "gettrade tradeid"
        "\nReturns the trade."
        "\nArguments:"
        "\n1. tradeid      (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    // TODO

    Object entry;
    entry.push_back(Pair("tradeid", id.ToString()));

    return entry;
}

Value getsealedvote(const Array& params, bool fHelp)
{
    string strHelp =
        "getsealedvote voteid"
        "\nReturns the vote."
        "\nArguments:"
        "\n1. voteid       (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    marketSealedVote *sealedVote = pmarkettree->GetSealedVote(id);

    if (!sealedVote) {
        string strError = std::string("Error: sealedvote does not exist!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;
    entry.push_back(Pair("voteid", id.ToString()));
    entry.push_back(Pair("branchid", sealedVote->branchid.ToString()));
    entry.push_back(Pair("height", (int)sealedVote->nHeight));
    entry.push_back(Pair("txid", sealedVote->txid.ToString()));

    return entry;
}

Value getrevealvote(const Array& params, bool fHelp)
{
    string strHelp =
        "getrevealvote voteid"
        "\nReturns the vote."
        "\nArguments:"
        "\n1. voteid       (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    marketRevealVote *revealVote = pmarkettree->GetRevealVote(id);

    if (!revealVote) {
        string strError = std::string("Error: revealvote does not exist!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;
    entry.push_back(Pair("voteid", id.ToString()));
    entry.push_back(Pair("branchid", revealVote->branchid.ToString()));
    entry.push_back(Pair("height", (int)revealVote->nHeight));
    entry.push_back(Pair("txid", revealVote->txid.ToString()));

    // Get decision ID(s)
    Array decisionArray;
    for (unsigned int i = 0; i < revealVote->decisionIDs.size(); i++) {
        decisionArray.push_back(revealVote->decisionIDs.at(i).GetHex());
    }

    entry.push_back(Pair("decisions", decisionArray));

    // Get decision vote(s)
    Array voteArray;
    for (unsigned int i = 0; i < revealVote->decisionVotes.size(); i++) {
        voteArray.push_back((int)revealVote->decisionVotes.at(i));
    }

    entry.push_back(Pair("votes", voteArray));

    return entry;
}

Value getstealvote(const Array& params, bool fHelp)
{
    string strHelp =
        "getstealvote voteid"
        "\nReturns the vote."
        "\nArguments:"
        "\n1. voteid       (uint256 string)";

    if (fHelp || (params.size() != 1))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    marketStealVote *stealVote = pmarkettree->GetStealVote(id);

    if (!stealVote) {
        string strError = std::string("Error: stealvote does not exist!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;
    entry.push_back(Pair("voteid", id.ToString()));
    entry.push_back(Pair("branchid", stealVote->branchid.ToString()));
    entry.push_back(Pair("height", (int)stealVote->nHeight));
    entry.push_back(Pair("txid", stealVote->txid.ToString()));

    return entry;
}

Value getballot(const Array &params, bool fHelp)
{
    string strHelp =
        "getballot branchid [height]"
        "\nReturns the ballot (terminated decisionids) for the given branchid."
        "\nArguments:"
        "\n1. branchid     (uint256 string)"
        "\n2. height       (optional, numeric)";

    if (fHelp || ((params.size() != 1) && (params.size() != 2)))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    Object entry;

    uint256 branchid;
    branchid.SetHex(params[0].get_str());
    marketBranch *branch = pmarkettree->GetBranch(branchid);
    if (!branch) {
        string strError = std::string("Error: branchid ")
            + branchid.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    entry.push_back(Pair("branchid", branchid.ToString()));

    uint32_t block_num = 0;
    if (params.size() == 2)
        block_num = (uint32_t) params[1].get_int();
    else {
        LOCK(cs_main);
        block_num = chainActive.Height();
    }
    uint32_t minblock = branch->tau * ((block_num - 1)/ branch->tau) + 1;
    uint32_t maxblock = minblock + branch->tau - 1;
    entry.push_back(Pair("minblock", (int)minblock));
    entry.push_back(Pair("maxblock", (int)maxblock));

    Array array;
    vector<marketDecision *> vec = pmarkettree->GetDecisions(branchid);
    for(size_t i=0; i < vec.size(); i++) {
        const marketDecision *obj = vec[i];
        if ((obj->eventOverBy < minblock)
            || (obj->eventOverBy > maxblock))
            continue;

        Object item;
        item.push_back(Pair("decisionid", obj->GetHash().ToString()));
        item.push_back(Pair("txid", obj->txid.ToString()));
        CHivemindAddress addr;
        if (addr.Set(obj->keyID))
            item.push_back(Pair("keyID", addr.ToString()));
        item.push_back(Pair("branchid", obj->branchid.ToString()));
        item.push_back(Pair("prompt", obj->prompt));
        item.push_back(Pair("eventoverby", (int)obj->eventOverBy));
        item.push_back(Pair("isScaled", (int)obj->isScaled));
        item.push_back(Pair("min", ValueFromAmount(obj->min)));
        item.push_back(Pair("max", ValueFromAmount(obj->max)));
        item.push_back(Pair("answerOptionality", (int)obj->answerOptionality));
        array.push_back(item);
    }
    entry.push_back(Pair("decisions", array));

    /* clean up */
    for(size_t i=0; i < vec.size(); i++)
        delete vec[i];
    delete branch;

    return entry;
}

Value getnewvotecoinaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewvotecoinaddress ( \"account\" )\n"
            "\nReturns a new Votecoin address\n"
            "If 'account' is specified (recommended), it is added to the address book \n"
            "so payments received with the address will be credited to 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"         (string, optional) The account name for the address to be linked to.\n"
            "\nResult:\n"
            "\"votecoinaddress\"    (string) The new votecoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewvotecoinaddress", "")
            + HelpExampleCli("getnewvotecoinaddress", "\"\"")
            + HelpExampleCli("getnewvotecoinaddress", "\"myaccount\"")
            + HelpExampleRpc("getnewvotecoinaddress", "\"myaccount\"")
        );

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBook(keyID, strAccount, "receive");

    // Create a Votecoin address from the new key
    CHivemindAddress votecoinAddress;
    votecoinAddress.is_votecoin = 1;
    votecoinAddress.Set(keyID);

    return votecoinAddress.ToString();
}

Value getcreatetradecapitalrequired(const Array& params, bool fHelp)
{
    string strHelp =
            "getcreatetradecapitalrequired marketid number_shares decision_state"
            "\nReturns the capital required to trade."
            "\n1. marketid            (u256 string)"
            "\n2. buy or sell         (string)"
            "\n3. number_shares       (numeric)"
            "\n4. decision_state      (string)";

    if (fHelp || (params.size() != 4))
        throw runtime_error(strHelp);

    if (!pmarkettree) {
        string strError = std::string("Error: NULL pmarkettree!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    uint256 id;
    id.SetHex(params[0].get_str());

    /* double-check buy_or_sell */
    string buy_or_sell = params[1].get_str();
    bool isBuy = (buy_or_sell == "buy");
    bool isSell = (buy_or_sell == "sell");
    if (!isBuy && !isSell) {
        string strError = std::string("Error: '")
            + buy_or_sell + "' must be buy or sell!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* double-check dnShares */
    if (params[2].get_real() <= 0.0) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%.8f", params[2].get_real());
        string strError = std::string("Error: number of shares ")
            + tmp + " must be positive!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    uint64_t dnShares = uint64FromValue(params[2], false);

    /* double-check marketid */
    marketMarket *market = pmarkettree->GetMarket(id);
    if (!market) {
        string strError = std::string("Error: marketid")
            + id.ToString() + " does not exist!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }
    uint32_t nStates = marketNStates(market);

    /* double-check decisionState */
    uint32_t decisionState = params[3].get_int();
    if (decisionState >= nStates) {
        char tmp0[32];
        snprintf(tmp0, sizeof(tmp0), "%u", decisionState);
        char tmp1[32];
        snprintf(tmp1, sizeof(tmp1), "%u", nStates);
        delete market;
        string strError = std::string("Error: decision state ")
            + tmp0 + " is above the number of states "
            + tmp1 + " in the market!";
        throw JSONRPCError(RPC_WALLET_ERROR, strError.c_str());
    }

    /* trades of the market */
    vector<marketTrade *> trades = pmarkettree->GetTrades(market->GetHash());

    /* current share totals of the market */
    double *nShares = new double [nStates];
    marketNShares(trades, nStates, nShares);

    /* current account value */
    double currAccount = marketAccountValue(market->maxCommission, 1e-8*market->B, nStates, nShares);

    /* new shares to be added to the market */
    nShares[decisionState] += (isBuy)? 1e-8*dnShares: -1e-8*dnShares;
    double nextAccount = marketAccountValue(market->maxCommission, 1e-8*market->B, nStates, nShares);

    /* the price difference to move from the current to the new */
    double price = (isBuy)? (nextAccount - currAccount) / (1e-8*dnShares)
        : (currAccount - nextAccount) / (1e-8*dnShares);

    /* round up because returing value is shown as a string */
    price += 1e-8;

    /* return value */
    Object entry;
    entry.push_back(Pair("B", 1e-8*market->B));
    entry.push_back(Pair("buy_or_sell", buy_or_sell));
    entry.push_back(Pair("nShares", 1e-8*dnShares));
    entry.push_back(Pair("price", price));
    entry.push_back(Pair("total", (1e-8*dnShares)*price));

    /* clean up */
    delete market;
    delete nShares;
    for(uint32_t i=0; i < trades.size(); i++)
        delete trades[i];

    return entry;
}

