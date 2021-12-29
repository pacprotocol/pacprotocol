// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_VERIFY_H
#define TOKEN_VERIFY_H

#include <amount.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <token/db.h>
#include <token/token.h>
#include <token/util.h>
#include <validation.h>

bool CheckTokenIssuance(const CTransactionRef& tx, std::string& strError);
bool ContextualCheckToken(CScript& token_script, CToken& token, std::string& strError);
bool CheckToken(const CTransactionRef& tx, std::string& strError, const Consensus::Params& params);

#endif // TOKEN_VERIFY_H
