// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/validation.h>
#include <rpc/server.h>
#include <token/util.h>
#include <token/verify.h>
#include <validation.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

UniValue tokendecode(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "tokendecode \"script\"\n"
            "\nDecode a token script.\n"
            "\nArguments:\n"
            "1. \"script\"            (string, required) The token script to decode.\n");
    }

    // Script
    std::string scriptDecode = request.params[0].get_str();
    if (!scriptDecode.size()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid script length");
    }

    // Convert string to script
    CScript script;
    std::vector<unsigned char> scriptData(ParseHexV(request.params[0], "argument"));
    script = CScript(scriptData.begin(), scriptData.end());

    // Decode token into elements
    uint8_t version;
    uint16_t type;
    uint64_t identifier;
    std::string name;
    CPubKey ownerKey;
    decode_token_script(script, version, type, identifier, name, ownerKey, true);

    // Decode destination
    CTxDestination dest;
    ExtractDestination(script, dest);

    // Print output
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("version", version);
    ret.pushKV("type", type);
    ret.pushKV("identifier", identifier);
    ret.pushKV("name", name);
    ret.pushKV("pubkey", EncodeDestination(dest));

    return ret;
}

UniValue tokenmint(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4) {
        throw std::runtime_error(
            "tokenmint \"address\" \"name\" amount \"checksum\"\n"
            "\nMint an amount of token, to a given address.\n"
            + HelpRequiringPassphrase(pwallet) + "\nArguments:\n"
                                                 "1. \"address\"            (string, required) The PAC address to send to.\n"
                                                 "2. \"name\"               (string, required) The token name.\n"
                                                 "3. \"amount\"             (numeric or string, required) The amount to mint.\n"
                                                 "4. \"checksum\"           (string, optional) The checksum to associate with this token.\n"
                                                 "\nResult:\n"
                                                 "\"txid\"                  (string) The transaction id.\n"
                                                 "\nExamples:\n"
            + HelpExampleCli("tokenmint", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\" \"BAZ\" 100000")
            + HelpExampleRpc("tokenmint", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\", \"BAZ\", 10000"));
    }

    // Prevent tokenmint while still in blocksync
    if (IsInitialBlockDownload()) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot perform token action while still in Initial Block Download");
    }

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, mempool.cs);
    LOCK(pwallet->cs_wallet);

    // Address
    std::string strOwner = request.params[0].get_str();
    CTxDestination dest = DecodeDestination(strOwner);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    // Name
    std::string strToken = request.params[1].get_str();

    std::string strError;
    strip_control_chars(strToken);
    if (!check_token_name(strToken, strError)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid token name");
    }

    // Amount
    CAmount nAmount = AmountFromValue(request.params[2]) / COIN;
    if (nAmount < 1 || nAmount > TOKEN_VALUEMAX) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid token amount");
    }

    // Checksum
    std::string strChecksum;
    bool usingChecksum = true;
    if (request.params.size() == 4) {
        strChecksum = request.params[3].get_str();
        if (strChecksum.size() != 40 || !IsHex(strChecksum)) {
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid checksum string specified");
        }
    } else {
        usingChecksum = false;
    }

    // Build script
    uint64_t identifier;
    CScript issuance_script;
    get_next_issuance_id(identifier);
    CScript token_destination = GetScriptForDestination(dest);
    build_token_script(issuance_script, CToken::CURRENT_VERSION, CToken::ISSUANCE, identifier, strToken, token_destination);

    // Build checksum script (if required)
    CScript checksum_script;
    if (usingChecksum) {
        std::vector<unsigned char> vecChecksum = ParseHex(strChecksum.c_str());
        uint160 checksum;
        memcpy(&checksum, vecChecksum.data(), 20);
        build_checksum_script(checksum_script, checksum);
    }

    // Extract balances from wallet
    CAmount valueOut;
    std::vector<CTxIn> ret_input;
    CAmount required_funds = nAmount + (usingChecksum ? 1000 : 0);
    if (!pwallet->FundMintTransaction(required_funds, valueOut, ret_input)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Could not find enough token to create transaction.");
    }
    print_txin_funds(ret_input);

    // Generate new change address
    bool change_was_used = (valueOut - required_funds) > 0;
    CPubKey newKey;
    CReserveKey reservekey(pwallet);
    if (!reservekey.GetReservedKey(newKey, true)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }
    CKeyID keyID = newKey.GetID();

    // Create transaction
    CMutableTransaction tx;
    tx.nLockTime = chainActive.Height();
    tx.vin = ret_input;
    tx.vout.push_back(CTxOut(nAmount, issuance_script));

    if (usingChecksum) {
        tx.vout.push_back(CTxOut(1000, checksum_script));
    }

    if (change_was_used) {
        CAmount change_amount = valueOut - required_funds;
        CScript change_script = GetScriptForDestination(keyID);
        tx.vout.push_back(CTxOut(change_amount, change_script));
    }

    // Sign transaction
    if (!pwallet->SignTokenTransaction(tx, strError)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Error signing token transaction (%s)", strError));
    }

    // Broadcast transaction
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    if (!wtx.RelayWalletTransaction(g_connman.get())) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error broadcasting token transaction");
    }

    // return change key if not used
    if (!change_was_used) {
        reservekey.ReturnKey();
    }

    return tx.GetHash().ToString();
}

UniValue tokenbalance(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "tokenbalance \"name\"\n"
            "\nList received tokens and their amount.\n"
            "\nArguments:\n"
            "1. \"name\"            (string, optional) Only show tokens matching name.\n");

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    // Name
    bool use_filter = false;
    std::string filter_name;
    if (!request.params[0].isNull()) {
        use_filter = true;
        filter_name = request.params[0].get_str();
        strip_control_chars(filter_name);
    } else {
        filter_name.clear();
    }

    LOCK(pwallet->cs_wallet);

    std::map<std::string, CAmount> token_balances_confirmed;
    std::map<std::string, CAmount> token_balances_unconfirmed;

    // Iterate wallet txes
    std::string strError;
    UniValue result(UniValue::VOBJ);
    {
        LOCK(pwallet->cs_wallet);
        for (auto it : pwallet->mapWallet) {

            const CWalletTx& wtx = it.second;
            if (wtx.IsCoinBase())
                continue;

            //! covers conflicted wtx's
            if (!wtx.IsTrusted()) {
                continue;
            }

            uint256 tx_hash = wtx.tx->GetHash();
            for (int n = 0; n < wtx.tx->vout.size(); n++) {
                CTxOut out = wtx.tx->vout[n];
                CScript pk = out.scriptPubKey;
                CAmount nValue = out.nValue;

                //! dont count checksum output value
                if (pk.IsChecksumData()) {
                    continue;
                }

                //! wallet may show existing spent entries
                if (pwallet->IsSpent(wtx.tx->GetHash(), n)) {
                    continue;
                }

                //! account for token in mempool, but not stale wallet sends
                bool in_mempool = false;
                if (wtx.GetDepthInMainChain() == 0) {
                    if (is_in_mempool(tx_hash)) {
                        in_mempool = true;
                    } else {
                        continue;
                    }
                }

                if (pk.IsPayToToken()) {
                    CToken token;
                    if (!build_token_from_script(pk, token)) {
                        continue;
                    }
                    CTxDestination address;
                    ExtractDestination(pk, address);

                    //! make sure we only display items 'to' us
                    if (!IsMine(*pwallet, address)) {
                        continue;
                    }

                    //! create and fill entry
                    std::string name = token.getName();
                    if (!in_mempool) {
                        token_balances_confirmed[name] += nValue;
                    }
                }
            }
        }
    }

    pwallet->GetUnconfirmedTokenBalance(mempool, token_balances_unconfirmed, strError);

    UniValue confirmed(UniValue::VOBJ);
    for (const auto& l : token_balances_confirmed) {
        if (!use_filter) {
            confirmed.pushKV(l.first, l.second);
        } else if (use_filter && compare_token_name(filter_name, REF(l.first))) {
            confirmed.pushKV(l.first, l.second);
        }
    }
    result.pushKV("confirmed", confirmed);

    UniValue unconfirmed(UniValue::VOBJ);
    for (const auto& l : token_balances_unconfirmed) {
        if (!use_filter) {
            unconfirmed.pushKV(l.first, l.second);
        } else if (use_filter && compare_token_name(filter_name, REF(l.first))) {
            unconfirmed.pushKV(l.first, l.second);
        }
    }
    result.pushKV("unconfirmed", unconfirmed);

    return result;
}

UniValue tokenlist(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "tokenlist\n"
            "\nList all token transactions in wallet.\n"
            "\nArguments:\n"
            "\nNone.\n");

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    // Get current height
    int height = chainActive.Height();

    // Iterate wallet txes
    UniValue result(UniValue::VARR);
    {
        LOCK(pwallet->cs_wallet);

        for (auto it : pwallet->mapWallet) {

            const CWalletTx& wtx = it.second;

            uint256 wtx_hash = wtx.GetHash();
            if (is_in_mempool(wtx_hash))
                continue;

            if (wtx.IsCoinBase())
                continue;

            for (int n = 0; n < wtx.tx->vout.size(); n++) {
                CTxOut out = wtx.tx->vout[n];
                CScript pk = out.scriptPubKey;
                CAmount nValue = out.nValue;

                if (pk.IsPayToToken()) {

                    CToken token;
                    if (!build_token_from_script(pk, token)) {
                        continue;
                    }
                    CTxDestination address;
                    ExtractDestination(pk, address);

                    //! wtx_type false (received), true (sent)
                    bool wtx_type = false;
                    if (!IsMine(*pwallet, address)) {
                        wtx_type = true;
                    }

                    //! create and fill entry
                    std::string name = token.getName();

                    UniValue entry(UniValue::VOBJ);
                    entry.pushKV("token", name);
                    entry.pushKV("address", EncodeDestination(address));
                    entry.pushKV("category", wtx_type ? "send" : "receive");
                    entry.pushKV("amount", nValue);
                    if (!mapBlockIndex[wtx.hashBlock]) {
                        entry.pushKV("confirmations", -1);
                    } else {
                        entry.pushKV("confirmations", height - mapBlockIndex[wtx.hashBlock]->nHeight);
                    }
                    entry.pushKV("time", wtx.GetTxTime());
                    entry.pushKV("block", wtx.hashBlock.ToString());
                    UniValue outpoint(UniValue::VOBJ);
                    outpoint.pushKV(wtx.tx->GetHash().ToString(), n);
                    entry.pushKV("outpoint", outpoint);

                    result.push_back(entry);
                }
            }
        }
    }

    return result;
}

UniValue tokensend(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 3) {
        throw std::runtime_error(
            "tokensend \"address\" \"name\" amount\n"
            "\nSend an amount of token, to a given address.\n"
            + HelpRequiringPassphrase(pwallet) + "\nArguments:\n"
                                                 "1. \"address\"            (string, required) The PAC address to send to.\n"
                                                 "2. \"name\"               (string, required) The token name.\n"
                                                 "3. \"amount\"             (numeric or string, required) The amount to send.\n"
                                                 "\nResult:\n"
                                                 "\"txid\"                  (string) The transaction id.\n"
                                                 "\nExamples:\n"
            + HelpExampleCli("tokensend", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\" \"BAZ\" 100000")
            + HelpExampleRpc("tokensend", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\", \"BAZ\", 10000"));
    }

    // Prevent tokensend while still in blocksync
    if (IsInitialBlockDownload()) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot perform token action while still in Initial Block Download");
    }

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, mempool.cs);
    LOCK(pwallet->cs_wallet);

    // Address
    std::string strDest = request.params[0].get_str();
    CTxDestination dest = DecodeDestination(strDest);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    // Name
    std::string strToken = request.params[1].get_str();
    strip_control_chars(strToken);
    if (strToken.size() < TOKENNAME_MINLEN || strToken.size() > TOKENNAME_MAXLEN) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid token name");
    }

    // Amount
    CAmount nAmount = AmountFromValue(request.params[2]) / COIN;
    if (nAmount < 1 || nAmount > TOKEN_VALUEMAX) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid token amount");
    }

    // Extract token/balances from wallet
    CAmount valueOut;
    std::vector<CTxIn> ret_input;
    if (!pwallet->FundTokenTransaction(strToken, nAmount, valueOut, ret_input)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Could not find enough token to create transaction.");
    }
    print_txin_funds(ret_input);

    // Generate target destination 'out'
    CScript destPubKey;
    CScript destScript = GetScriptForDestination(dest);
    uint64_t id;
    if (!get_id_for_token_name(strToken, id)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Could not find token id from returned token inputs.");
    }
    build_token_script(destPubKey, CToken::CURRENT_VERSION, CToken::TRANSFER, id, strToken, destScript);
    CTxOut destOutput(nAmount, destPubKey);

    // Generate new change address
    bool change_was_used = false;
    CPubKey newKey;
    CReserveKey reservekey(pwallet);
    if (!reservekey.GetReservedKey(newKey, true)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }
    CKeyID keyID = newKey.GetID();

    // Create transaction
    CMutableTransaction tx;
    tx.nLockTime = chainActive.Height();
    tx.vin = ret_input;
    tx.vout.push_back(destOutput);

    // Generate target change 'out'
    if (valueOut - nAmount > 0) {
        CScript destChangePubKey;
        CScript destChangeScript = GetScriptForDestination(keyID);
        build_token_script(destChangePubKey, CToken::CURRENT_VERSION, CToken::TRANSFER, id, strToken, destChangeScript);
        CTxOut destChangeOutput(valueOut - nAmount, destChangePubKey);
        tx.vout.push_back(destChangeOutput);
        change_was_used = true;
    }

    // Sign transaction
    std::string strError;
    if (!pwallet->SignTokenTransaction(tx, strError)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Error signing token transaction (%s)", strError));
    }

    // Broadcast transaction
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    if (!wtx.RelayWalletTransaction(g_connman.get())) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error broadcasting token transaction");
    }

    // return change key if not used
    if (!change_was_used) {
        reservekey.ReturnKey();
    }

    return tx.GetHash().ToString();
}

UniValue tokenissuances(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "tokenissuances\n"
            "\nList known token issuances.\n"
            "\nArguments:\n"
            "none\n");
    }

    UniValue issuances(UniValue::VOBJ);
    {
        LOCK(cs_main);
        for (CToken& token : known_issuances) {
            UniValue issuance(UniValue::VOBJ);
            issuance.pushKV("version", strprintf("%02x", token.getVersion()));
            issuance.pushKV("type", strprintf("%04x", token.getType()));
            issuance.pushKV("identifier", strprintf("%016x", token.getId()));
            issuance.pushKV("origintx", token.getOriginTx().ToString());
            issuances.pushKV(token.getName(), issuance);
        }
    }

    return issuances;
}

UniValue tokenchecksum(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "tokenchecksum \"name\"\n"
            "\nRetrieve checksum hash for a given token.\n"
            "\nArguments:\n"
            "1. \"name\"            (string, required) The token to retrieve checksum from.\n");
    }

    // Name
    std::string strToken = request.params[0].get_str();
    strip_control_chars(strToken);
    if (strToken.size() < TOKENNAME_MINLEN || strToken.size() > TOKENNAME_MAXLEN) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid token name");
    }

    // Search and retrieve checksum
    {
        LOCK(cs_main);
        for (CToken& token : known_issuances) {
            if (strToken == token.getName()) {
                //! fetch token origin tx
                uint256 blockHash;
                CTransactionRef tx;
                uint256 origin = token.getOriginTx();
                if (!GetTransaction(origin, tx, Params().GetConsensus(), blockHash)) {
                    throw JSONRPCError(RPC_TYPE_ERROR, "Could not retrieve token origin transaction.");
                }
                //! fetch checksum output
                for (unsigned int i = 0; i < tx->vout.size(); i++) {
                    if (tx->vout[i].IsTokenChecksum()) {
                        uint160 checksum_output;
                        CScript checksum_script = tx->vout[i].scriptPubKey;
                        if (!decode_checksum_script(checksum_script, checksum_output)) {
                            throw JSONRPCError(RPC_TYPE_ERROR, "Could not retrieve checksum from token origin transaction.");
                        }
                        return HexStr(checksum_output);
                    }
                }
            }
        }
    }

    return NullUniValue;
}

UniValue tokenhistory(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "tokenhistory \"name\"\n"
            "\nFind latest token of type name and trace it all the way back to issuance.\n"
            "\nArguments:\n"
            "1. \"name\"            (string, required) The token to display history for.\n");
    }

    // Get current height
    int height = chainActive.Height();

    // Name
    std::string strToken = request.params[0].get_str();
    strip_control_chars(strToken);
    if (strToken.size() < TOKENNAME_MINLEN || strToken.size() > TOKENNAME_MAXLEN) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid token name");
    }

    // Retrieve token history
    UniValue history(UniValue::VARR);
    {
        LOCK(cs_main);
        COutPoint token_spend;
        if (!FindLastTokenUse(strToken, token_spend, height, Params().GetConsensus())) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to find usage of token");
        }

        uint256 hash = token_spend.hash;
        int n = token_spend.n;

        while (true) {

            // fetch transaction
            uint256 blockHash;
            CTransactionRef tx;
            if (!GetTransaction(hash, tx, Params().GetConsensus(), blockHash)) {
                throw JSONRPCError(RPC_TYPE_ERROR, "Could not retrieve token transaction.");
            }

            // decode token
            CToken token;
            std::string strError;
            CScript token_script = tx->vout[n].scriptPubKey;
            if (!ContextualCheckToken(token_script, token, strError)) {
                throw JSONRPCError(RPC_TYPE_ERROR, "Token data inconsistent.");
            }

            // add entry to history
            UniValue entry(UniValue::VOBJ);
            entry.pushKV("name", strToken);
            entry.pushKV("type", token.isIssuance() ? "issuance" : "transfer");
            entry.pushKV("amount", tx->vout[n].nValue);
            entry.pushKV("height", mapBlockIndex[blockHash]->nHeight);
            UniValue outpoint(UniValue::VOBJ);
            outpoint.pushKV(hash.ToString(), n);
            entry.pushKV("outpoint", outpoint);
            history.push_back(entry);

            // check when to bail
            if (token.isIssuance()) {
                break;
            }

            // check token
            if (strToken != token.getName()) {
                throw JSONRPCError(RPC_TYPE_ERROR, "Token data inconsistent.");
            }

            // get prevout for token
            for (unsigned int i = 0; tx->vin.size(); i++) {
                hash = tx->vin[i].prevout.hash;
                n = tx->vin[i].prevout.n;
                break;
            }
        }
    }

    return history;
}

UniValue tokeninfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "tokeninfo \"name\"\n"
            "\nOutputs token's information.\n"
            "\nArguments:\n"
            "1. \"name\"            (string, required) The token to show information.\n");
    }

    // Name
    std::string strToken = request.params[0].get_str();
    strip_control_chars(strToken);
    if (strToken.size() < TOKENNAME_MINLEN || strToken.size() > TOKENNAME_MAXLEN) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid token name");
    }

    // Search and retrieve token and checksum
    UniValue result(UniValue::VOBJ);
    {
        LOCK(cs_main);
        for (CToken& token : known_issuances) {
            if (strToken == token.getName()) {
                //! fetch token origin tx
                uint256 blockHash;
                CTransactionRef tx;
                uint256 origin_tx = token.getOriginTx();
                if (!GetTransaction(origin_tx, tx, Params().GetConsensus(), blockHash)) {
                    throw JSONRPCError(RPC_TYPE_ERROR, "Could not retrieve token origin transaction.");
                }

                UniValue entry(UniValue::VOBJ);
                entry.pushKV("version", strprintf("%02x", token.getVersion()));
                entry.pushKV("type", strprintf("%04x", token.getType()));
                entry.pushKV("identifier", strprintf("%016x", token.getId()));

                UniValue origin(UniValue::VOBJ);
                origin.pushKV("tx", token.getOriginTx().ToString());

                //! fetch token and checksum output from origin transactions
                bool found_token = false;
                bool found_checksum = false;
                for (unsigned int i = 0; i < tx->vout.size(); i++) {
                    if (tx->vout[i].IsTokenOutput()) {
                        uint8_t version;
                        uint16_t type;
                        uint64_t identifier;
                        std::string name;
                        CPubKey ownerKey;
                        CScript token_script = tx->vout[i].scriptPubKey;
                        if (!decode_token_script(token_script, version, type, identifier, name, ownerKey, true)) {
                            throw JSONRPCError(RPC_TYPE_ERROR, "Could not retrieve token from origin transaction.");
                        }
                        CTxDestination address;
                        ExtractDestination(token_script, address);
                        CAmount amount = tx->vout[i].nValue;
                        origin.pushKV("address", EncodeDestination(address));
                        origin.pushKV("maxsupply", amount);
                        found_token = true;
                        if (found_token && found_checksum) {
                            break;
                        }
                    }
                    if (tx->vout[i].IsTokenChecksum()) {
                        uint160 checksum_output;
                        CScript checksum_script = tx->vout[i].scriptPubKey;
                        if (!decode_checksum_script(checksum_script, checksum_output)) {
                            throw JSONRPCError(RPC_TYPE_ERROR, "Could not retrieve checksum from token origin transaction.");
                        }
                        entry.pushKV("checksum", HexStr(checksum_output));
                        found_checksum = true;
                        if (found_token && found_checksum) {
                            break;
                        }
                    }
                }

                entry.pushKV("origin", origin);
                result.pushKV(token.getName(), entry);

                return result;
            }
        }
    }

    return NullUniValue;
}

UniValue tokenunspent(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "tokenunspent\n"
            "\nList all unspent token outputs.\n");

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK(pwallet->cs_wallet);

    // Iterate wallet txes
    UniValue result(UniValue::VARR);
    {
        LOCK(pwallet->cs_wallet);
        for (auto it : pwallet->mapWallet) {
            const CWalletTx& wtx = it.second;
            if (wtx.IsCoinBase())
                continue;

            if (!mapBlockIndex[wtx.hashBlock])
                continue;

            if (!wtx.IsTrusted())
                continue;

            uint256 tx_hash = wtx.tx->GetHash();
            for (int n = 0; n < wtx.tx->vout.size(); n++) {
                CTxOut out = wtx.tx->vout[n];
                CScript pk = out.scriptPubKey;
                CAmount nValue = out.nValue;

                //! wallet may show existing spent entries
                if (pwallet->IsSpent(wtx.tx->GetHash(), n)) {
                    continue;
                }

                if (pk.IsPayToToken()) {
                    CToken token;
                    if (!build_token_from_script(pk, token)) {
                        continue;
                    }

                    CTxDestination address;
                    ExtractDestination(pk, address);

                    //! make sure we only display items 'to' us
                    if (!IsMine(*pwallet, address)) {
                        continue;
                    }

                    //! create and fill entry
                    UniValue entry(UniValue::VOBJ);
                    if (nValue > 0) {
                        entry.pushKV("token", token.getName());
                        entry.pushKV("data", HexStr(pk));
                        entry.pushKV("amount", nValue);
                        result.push_back(entry);
                    }
                }
            }
        }
    }

    return result;
}


static const CRPCCommand commands[] =
{ //  category              name                      actor (function)
  //  --------------------- ------------------------  -----------------------
    { "token",              "tokendecode",            &tokendecode,             {"script" } },
    { "token",              "tokenmint",              &tokenmint,               {"address", "name", "amount", "checksum" } },
    { "token",              "tokenbalance",           &tokenbalance,            {"name" } },
    { "token",              "tokenhistory",           &tokenhistory,            {"name" } },
    { "token",              "tokenlist",              &tokenlist,               { } },
    { "token",              "tokensend",              &tokensend,               {"address", "name", "amount" } },
    { "token",              "tokenissuances",         &tokenissuances,          { } },
    { "token",              "tokenchecksum",          &tokenchecksum,           {"name" } },
    { "token",              "tokeninfo",              &tokeninfo,               {"name" } },
    { "token",              "tokenunspent",           &tokenunspent,            { } },
};

void RegisterTokenRPCCommands(CRPCTable& tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
