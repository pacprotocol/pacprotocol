// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <token/verify.h>

bool CheckTokenIssuance(const CTransactionRef& tx, std::string& strError)
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
                bool alreadySeen = false;
                for (CToken& issued : known_issuances) {
                    if (issued.getOriginTx() != token.getOriginTx()) {
                        if (issued.getName() == token.getName()) {
                            strError = "issuance-name-exists";
                            return false;
                        } else if (issued.getId() == token.getId()) {
                            strError = "issuance-id-exists";
                            return false;
                        }
                    } else {
                        //! here because we've been called again from connectblock
                        alreadySeen = true;
                    }
                }
                if (!alreadySeen) {
                    known_issuances.push_back(token);
                    SaveDB();
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

bool CheckToken(const CTransactionRef& tx, std::string& strError, const Consensus::Params& params)
{
    int tokenIssuance = 0;
    uint256 hash = tx->GetHash();

    //! ensure only one issuance per tx
    int issuance_total = 0;
    for (unsigned int i = 0; i < tx->vout.size(); i++) {
        if (tx->vout[i].scriptPubKey.IsPayToToken()) {
            CToken token;
            CScript tokenData = tx->vout[i].scriptPubKey;
            if (token.isIssuance()) {
                ++tokenIssuance;
            }
        }
    }
    if (tokenIssuance > 1) {
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

            //! issuance token cant check previn
            if (token.getType() == CToken::ISSUANCE) {
                if (!CheckTokenIssuance(tx, strError)) {
                    strError = "token-already-issued";
                    return false;
                }
                continue;
            }

            //! keep identifier and name
            uint64_t tokenId = token.getId();
            std::string tokenName = token.getName();

            //! check token inputs
            for (unsigned int n = 0; n < tx->vin.size(); n++)
            {
                //! retrieve prevtx
                uint256 prevBlockHash;
                CTransactionRef inputPrev;
                if (!GetTransaction(tx->vin[n].prevout.hash, inputPrev, params, prevBlockHash)) {
                    strError = "token-prevtx-invalid";
                    return false;
                }

                //! is the output a token?
                if (!inputPrev->vout[tx->vin[n].prevout.n].scriptPubKey.IsPayToToken()) {
                    strError = "token-prevout-isinvalid";
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
