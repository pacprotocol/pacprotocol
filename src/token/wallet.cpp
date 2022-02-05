// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/policy.h>
#include <rpc/protocol.h>
#include <token/util.h>
#include <txmempool.h>
#include <wallet/wallet.h>
#include <validation.h>

extern CTxMemPool mempool;
extern std::unique_ptr<CCoinsViewCache> pcoinsTip;

bool CWallet::AvailableToken(std::string& tokenname, uint64_t& tokenid, CAmount& amountmin, CAmount& amountout, CTxIn& ret)
{
    for (auto it : mapWallet) {
        const CWalletTx& wtx = it.second;
        if (wtx.IsCoinBase()) {
            continue;
        }
        int n = 0;
        uint256 tx_hash = wtx.tx->GetHash();
        for (const auto& out : wtx.tx->vout) {
            COutPoint wtx_out(tx_hash, n);
            if (!is_output_unspent(wtx_out)) {
                continue;
            }
            CAmount nValue = out.nValue;
            CScript pk = out.scriptPubKey;
            if (pk.IsPayToToken()) {
                CToken token;
                if (!build_token_from_script(pk, token)) {
                    continue;
                }
                if (tokenname == token.getName()) {
                    //! we could opt to use the smallest output found, but once we're rolling..
                    if (nValue >= amountmin) {
                        tokenid = token.getId();
                        amountout = nValue;
                        ret = CTxIn(COutPoint(tx_hash, n));
                        LogPrint(BCLog::TOKEN, "%s - returning COutPoint(%s, %d) containing %d %s\n", __func__, tx_hash.ToString(), n, nValue, token.getName());
                        return true;
                    }
                }
            }
            n++;
        }
    }
    return false;
}

bool CWallet::SignTokenTransaction(CMutableTransaction& rawTx, std::string& strError)
{
    strError = "No error";

    // Fetch previous transactions (inputs):
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewCache& viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        for (const CTxIn& txin : rawTx.vin) {
            view.AccessCoin(txin.prevout); // Load entries from viewChain into view; can fail.
        }

        view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
    }

    const CKeyStore& keystore = *this;

    int nHashType = SIGHASH_ALL;
    bool fHashSingle = ((nHashType & ~(SIGHASH_ANYONECANPAY)) == SIGHASH_SINGLE);

    UniValue vErrors(UniValue::VARR);
    const CTransaction txConst(rawTx);
    for (unsigned int i = 0; i < rawTx.vin.size(); i++) {

        CTxIn& txin = rawTx.vin[i];
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            strError = "Input not found or already spent";
            return false;
        }
        const CScript& prevPubKey = coin.out.scriptPubKey;
        const CAmount& amount = coin.out.nValue;

        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < rawTx.vout.size())) {
            SignSignature(keystore, prevPubKey, rawTx, i, amount, nHashType);
        }

        // ... and merge in other signatures:
        SignatureData sigdata;
        sigdata = CombineSignatures(prevPubKey, TransactionSignatureChecker(&txConst, i, amount), sigdata, DataFromTransaction(rawTx, i));
        ScriptError serror = SCRIPT_ERR_OK;

        if (!VerifyScript(txin.scriptSig, prevPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker(&rawTx, i, amount), &serror)) {
            strError = ScriptErrorString(serror);
            return false;
        }
    }

    return true;
}
