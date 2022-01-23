// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/validation.h>
#include <rpc/server.h>
#include <token/db.h>
#include <token/util.h>
#include <token/verify.h>
#include <wallet/wallet.h>
#include <wallet/rpcwallet.h>
#include <validation.h>

UniValue tokendecode(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "tokendecode \"script\"\n"
            "\nDecode a token script.\n"
            "\nArguments:\n"
            "1. \"script\"            (string, required) The token script to decode.\n"
        );
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
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"address\"            (string, required) The PAC address to send to.\n"
            "2. \"name\"               (string, required) The token name.\n"
            "3. \"amount\"             (numeric or string, required) The amount to mint.\n"
            "4. \"checksum\"           (string, optional) The checksum to associate with this token.\n"
            "\nResult:\n"
            "\"txid\"                  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("tokenmint", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\" \"BAZ\" 100000")
            + HelpExampleRpc("tokenmint", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\", \"BAZ\", 10000")
        );
    }

    // Prevent tokenmint while still in blocksync
    if (IsInitialBlockDownload()) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot perform token action while still in Initial Block Download");
    }

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    // Address
    std::string strOwner = request.params[0].get_str();
    CTxDestination dest = DecodeDestination(strOwner);
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

    // Checksum
    bool usingChecksum = true;
    std::string strChecksum = request.params[3].get_str();
    if (strChecksum.size() != 40 || !IsHex(strChecksum) || strChecksum.empty()) {
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

    // Create and send the transaction
    CReserveKey reservekey(pwallet);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {issuance_script, usingChecksum ? nAmount + 1000 : nAmount , false};
    vecSend.push_back(recipient);
    if (usingChecksum) {
        CRecipient checksum_recipient = {checksum_script, 1000, false};
        vecSend.push_back(checksum_recipient);
    }
    CTransactionRef tx;
    mapValue_t mapValue;
    CCoinControl coin_control;
    if (!pwallet->CreateTransaction(vecSend, tx, reservekey, nFeeRequired, nChangePosRet, strError, coin_control)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    CValidationState state;
    if (!pwallet->CommitTransaction(tx, std::move(mapValue), {} /* orderForm */, std::string("\"\""), reservekey, g_connman.get(), state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    CWalletTx wtx(pwallet, tx);
    if (!wtx.RelayWalletTransaction(g_connman.get())) {
        LogPrintf("RelayWalletTransaction(): Error during transaction broadcast!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().ToString();
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

    // Iterate wallet txes
    UniValue result(UniValue::VOBJ);
    {
        LOCK(pwallet->cs_wallet);
        for (auto it : pwallet->mapWallet) {
            int n = 0;
            const CWalletTx& wtx = it.second;
            if (wtx.IsCoinBase())
                continue;
            for (const auto& out : wtx.tx->vout) {
                CScript pk = out.scriptPubKey;
                CAmount nValue = out.nValue;

                //! only show entries with sufficient confirms
                if (wtx.GetDepthInMainChain() == 0) {
                    continue;
                }

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
                    std::string name = token.getName();

                    UniValue entry(UniValue::VOBJ);
                    entry.pushKV("identifier", strprintf("%016x", token.getId()));
                    entry.pushKV("address", EncodeDestination(address));
                    entry.pushKV("amount", nValue);

                    UniValue outpoint(UniValue::VOBJ);
                    outpoint.pushKV(wtx.tx->GetHash().ToString(), n);
                    entry.pushKV("outpoint", outpoint);

                    if (!use_filter) {
                        result.pushKV(token.getName(), entry);
                    } else if (use_filter && compare_token_name(filter_name, name)) {
                        result.pushKV(token.getName(), entry);
                    }
                }
                ++n;
            }
        }
    }

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
            "tokenlist \"name\"\n"
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

            if (wtx.IsCoinBase())
                continue;

            int n = 0;
            for (const auto& out : wtx.tx->vout) {

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
                    entry.pushKV("confirmations", height - mapBlockIndex[wtx.hashBlock]->nHeight);
                    entry.pushKV("time", wtx.GetTxTime());
                    entry.pushKV("block", wtx.hashBlock.ToString());
                    UniValue outpoint(UniValue::VOBJ);
                    outpoint.pushKV(wtx.tx->GetHash().ToString(), n);
                    entry.pushKV("outpoint", outpoint);

                    result.push_back(entry);
                }
                ++n;
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
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"address\"            (string, required) The PAC address to send to.\n"
            "2. \"name\"               (string, required) The token name.\n"
            "3. \"amount\"             (numeric or string, required) The amount to send.\n"
            "\nResult:\n"
            "\"txid\"                  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("tokensend", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\" \"BAZ\" 100000")
            + HelpExampleRpc("tokensend", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\", \"BAZ\", 10000")
        );
    }

    // Prevent tokensend while still in blocksync
    if (IsInitialBlockDownload()) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot perform token action while still in Initial Block Download");
    }

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

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
    uint64_t id;
    CTxIn ret_input;
    CAmount valueOut;
    if (!pwallet->AvailableToken(strToken, id, nAmount, valueOut, ret_input)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Could not find enough token to create transaction.");
    }

    // Generate target destination 'out'
    CScript destPubKey;
    CScript destScript = GetScriptForDestination(dest);
    build_token_script(destPubKey, CToken::CURRENT_VERSION, CToken::TRANSFER, id, strToken, destScript);
    CTxOut destOutput(nAmount, destPubKey);

    // Generate new change address
    CPubKey newKey;
    if (!pwallet->GetKeyFromPool(newKey, false)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }
    CKeyID keyID = newKey.GetID();

    // Generate target change 'out'
    CScript destChangePubKey;
    CScript destChangeScript = GetScriptForDestination(keyID);
    build_token_script(destChangePubKey, CToken::CURRENT_VERSION, CToken::TRANSFER, id, strToken, destChangeScript);
    CTxOut destChangeOutput(valueOut - nAmount, destChangePubKey);

    // Create transaction
    CMutableTransaction tx;
    tx.nLockTime = chainActive.Height();
    tx.vin.push_back(ret_input);
    tx.vout.push_back(destOutput);
    tx.vout.push_back(destChangeOutput);

    // Sign transaction
    std::string strError;
    if (!pwallet->SignTokenTransaction(tx, strError)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Issue signing token transaction");
    }

    // Broadcast transaction
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    if (!wtx.RelayWalletTransaction(g_connman.get())) {
        LogPrintf("RelayWalletTransaction(): Error during transaction broadcast!");
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().ToString();
}

UniValue tokenrebuild(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "tokenrebuild\n"
            "\nRebuild issuances/token database by rescanning the chain.\n"
            "\nArguments:\n"
            "none\n"
        );
    }

    {
        LOCK(cs_main);
        int last_height = chainActive.Height();
        const Consensus::Params& params = Params().GetConsensus();
        tokendb->ResetIssuanceState();
        RescanBlocksForTokenData(last_height, params);
    }

    return NullUniValue;
}

UniValue tokenissuances(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "tokenissuances\n"
            "\nList known token issuances.\n"
            "\nArguments:\n"
            "none\n"
        );
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
            "1. \"name\"            (string, required) The token to retrieve checksum from.\n"
        );
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
            "1. \"name\"            (string, required) The token to display history for.\n"
        );
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
            "1. \"name\"            (string, required) The token to show information.\n"
        );
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
                    if (tx->vout[i].IsTokenOutput()){
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

                        // The creator / origin address
                        origin.pushKV("address", EncodeDestination(address));

                        // @Barry
                        // Not sure if this is correct. $YAN won't have max supply (I assume?). 
                        // So perhaps maxsupply = -1 (or 0?) -> no max supply or not showing it at all make it sense
                        // I'm not quite sure how to detect if there is no max supply or if we even have dynamic max supply.
                        origin.pushKV("maxsupply", amount);
                        found_token = true;
                        if (found_token && found_checksum){
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
                        if (found_token && found_checksum){
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


static const CRPCCommand commands[] =
{ //  category              name                      actor (function)
  //  --------------------- ------------------------  -----------------------
    { "token",              "tokendecode",            &tokendecode,             {"script" } },
    { "token",              "tokenmint",              &tokenmint,               {"address", "name", "amount", "checksum" } },
    { "token",              "tokenbalance",           &tokenbalance,            {"name" } },
    { "token",              "tokenhistory",           &tokenhistory,            {"name" } },
    { "token",              "tokenlist",              &tokenlist,               { } },
    { "token",              "tokensend",              &tokensend,               {"address", "name", "amount" } },
    { "token",              "tokenrebuild",           &tokenrebuild,            { } },
    { "token",              "tokenissuances",         &tokenissuances,          { } },
    { "token",              "tokenchecksum",          &tokenchecksum,           {"name" } },
    { "token",              "tokeninfo",              &tokeninfo,               {"name" } },
};

void RegisterTokenRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

