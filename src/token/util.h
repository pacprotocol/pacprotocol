// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_UTIL_H
#define TOKEN_UTIL_H

#include <amount.h>
#include <key_io.h>
#include <logging.h>
#include <script/script.h>
#include <serialize.h>
#include <token/token.h>
#include <token/verify.h>
#include <txmempool.h>
#include <utilstrencodings.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <boost/algorithm/string.hpp>

class CToken;
class CTxMemPool;

void reclaim_invalid_inputs();
bool compare_token_name(std::string& prev_token_name, std::string& token_name);
bool check_token_name(std::string& tokenName, std::string& errorReason);
void strip_control_chars(std::string& instr);
bool is_in_mempool(uint256& txhash);
void remove_from_mempool(CTransaction& tx);
bool is_output_unspent(const COutPoint& out);
bool is_output_in_mempool(const COutPoint& out);
void print_txin_funds(std::vector<CTxIn>& funds_ret);
opcodetype GetOpcode(int n);
int GetIntFromOpcode(opcodetype n);

#endif // TOKEN_UTIL_H
