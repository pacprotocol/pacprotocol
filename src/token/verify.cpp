// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <token/verify.h>

bool are_tokens_active(int height)
{
    const Consensus::Params& params = Params().GetConsensus();
    //! check against provided height
    if (height != 0) {
        return height >= params.nTokenHeight;
    }
    //! otherwise use active chainheight
    return chainActive.Height() >= params.nTokenHeight;
}

bool CheckTokenIssuance(const CTransactionRef& tx, bool onlyCheck, std::string& strError)
{
    uint256 hash = tx->GetHash();
    for (unsigned int i = 0; i < tx->vout.size(); i++) {
        if (tx->vout[i].scriptPubKey.IsPayToToken()) {
            CToken token;
            CScript token_script = tx->vout[i].scriptPubKey;
            if (!ContextualCheckToken(token_script, token, strError)) {
                return false;
            }
            token.setOriginTx(hash);
            if (token.getType() == CToken::ISSUANCE) {
                for (CToken& issued : known_issuances) {
                    if (issued.getOriginTx() != token.getOriginTx()) {
                        if (issued.getName() == token.getName()) {
                            strError = "issuance-name-exists";
                            return false;
                        } else if (issued.getId() == token.getId()) {
                            strError = "issuance-id-exists";
                            return false;
                        }
                    }
                }
                std::string name = token.getName();
                uint64_t identifier = token.getId();
                if (!onlyCheck && (!is_name_in_issuances(name) && !is_identifier_in_issuances(identifier))) {
                    known_issuances.push_back(token);
                }
            } else if (token.getType() == CToken::NONE) {
                return false;
            }
        }
    }
    return true;
}

bool ContextualCheckToken(CScript& token_script, CToken& token, std::string& strError)
{
    build_token_from_script(token_script, token);

    if (token.getVersion() != CToken::CURRENT_VERSION) {
        strError = "bad-token-version";
        return false;
    }

    if (token.getType() == CToken::NONE) {
        strError = "bad-token-uninit";
        return false;
    }

    if (token.getType() != CToken::ISSUANCE && token.getType() != CToken::TRANSFER) {
        strError = "bad-token-type";
        return false;
    }

    std::string name = token.getName();
    if (!check_token_name(name, strError)) {
        return false;
    }

    return true;
}

bool CheckToken(const CTransactionRef& tx, bool onlyCheck, std::string& strError, const Consensus::Params& params)
{
    uint256 hash = tx->GetHash();

    //! ensure only one issuance per tx
    int issuance_total = 0;
    for (unsigned int i = 0; i < tx->vout.size(); i++) {
        if (tx->vout[i].scriptPubKey.IsPayToToken()) {
            CToken token;
            CScript tokenData = tx->vout[i].scriptPubKey;
            if (token.isIssuance()) {
                ++issuance_total;
            }
        }
    }
    if (issuance_total > 1) {
        strError = "multiple-token-issuances";
        return false;
    }

    //! check to see if token has valid prevout
    for (unsigned int i = 0; i < tx->vout.size(); i++) {

        //! find the token outputs
        if (tx->vout[i].scriptPubKey.IsPayToToken()) {

            //! extract token data from output
            CToken token;
            CScript tokenData = tx->vout[i].scriptPubKey;
            if (!ContextualCheckToken(tokenData, token, strError)) {
                strError = "token-isinvalid";
                return false;
            }

            //! check if issuance token is unique
            if (token.getType() == CToken::ISSUANCE) {
                if (!CheckTokenIssuance(tx, onlyCheck, strError)) {
                    strError = "token-already-issued";
                    return false;
                }
            }

            //! keep identifier and name
            uint64_t tokenId = token.getId();
            std::string tokenName = token.getName();

            //! check token inputs
            for (unsigned int n = 0; n < tx->vin.size(); n++) {
                //! retrieve prevtx
                uint256 prevBlockHash;
                CTransactionRef inputPrev;
                if (!GetTransaction(tx->vin[n].prevout.hash, inputPrev, params, prevBlockHash)) {
                    strError = "token-prevtx-invalid";
                    return false;
                }

                // check if issuances inputs are token related
                uint16_t tokenType = token.getType();
                bool isPrevToken = inputPrev->vout[tx->vin[n].prevout.n].scriptPubKey.IsPayToToken();
                switch (tokenType) {
                case CToken::ISSUANCE:
                    if (isPrevToken) {
                        strError = "token-issuance-prevout-not-standard";
                        return false;
                    }
                    continue;
                case CToken::TRANSFER:
                    if (!isPrevToken) {
                        strError = "token-transfer-prevout-is-invalid";
                        return false;
                    }
                    break;
                case CToken::NONE:
                    strError = "token-type-unusable";
                    return false;
                }

                //! extract prevtoken data from the output
                CToken prevToken;
                CScript prevTokenData = inputPrev->vout[n].scriptPubKey;
                if (!ContextualCheckToken(prevTokenData, prevToken, strError)) {
                    strError = "token-prevtoken-isinvalid";
                    return false;
                }

                //! check if token name same as prevtoken name
                uint64_t prevIdentifier = prevToken.getId();
                std::string prevTokenName = prevToken.getName();
                if (!compare_token_name(prevTokenName, tokenName)) {
                    strError = "prevtoken-isunknown-name";
                    return false;
                }

                if (prevIdentifier != tokenId) {
                    strError = "prevtoken-isunknown-id";
                    return false;
                }
            }
        }
    }

    return true;
}

bool FindLastTokenUse(std::string& name, COutPoint& token_spend, int lastHeight, const Consensus::Params& params)
{
    for (int height = lastHeight; height > params.nTokenHeight; --height) {

        // fetch index for current height
        const CBlockIndex* pindex = chainActive[height];

        // read block from disk
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, params)) {
            continue;
        }

        for (unsigned int i = 0; i < block.vtx.size(); i++) {

            // search for token transactions
            const CTransactionRef& tx = block.vtx[i];
            if (!tx->HasTokenOutput()) {
                continue;
            }

            for (unsigned int j = 0; j < tx->vout.size(); j++) {

                // parse each token transaction
                CToken token;
                std::string strError;
                CScript token_script = tx->vout[j].scriptPubKey;
                if (!ContextualCheckToken(token_script, token, strError)) {
                    continue;
                }

                // check if it matches
                if (name == token.getName()) {
                    token_spend.hash = tx->GetHash();
                    token_spend.n = j;
                    return true;
                }
            }
        }
    }

    return false;
}
