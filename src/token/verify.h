// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_VERIFY_H
#define TOKEN_VERIFY_H

#include <amount.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <token/issuances.h>
#include <token/token.h>
#include <token/util.h>
#include <validation.h>

class CToken;

bool are_tokens_active(int height = 0);
bool CheckTokenMempool(CTxMemPool& pool, const CTransactionRef& tokenTx, std::string& strError);
bool CheckTokenIssuance(const CTransactionRef& tx, std::string& strError, bool onlyCheck);
bool CheckTokenInputs(const CTransactionRef& tx, const CBlockIndex* pindex, const CCoinsViewCache& view, std::string& strError);
bool ContextualCheckToken(CScript& token_script, CToken& token, std::string& strError, bool debug = false);
bool CheckToken(const CTransactionRef& tx, const CBlockIndex* pindex, const CCoinsViewCache& view, std::string& strError, const Consensus::Params& params, bool onlyCheck);
bool FindLastTokenUse(std::string& name, COutPoint& token_spend, int lastHeight, const Consensus::Params& params);
void UndoTokenIssuance(uint64_t& id, std::string& name);
void UndoTokenIssuancesInBlock(const CBlock& block);

#endif // TOKEN_VERIFY_H
