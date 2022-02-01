// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_UTIL_H
#define TOKEN_UTIL_H

#include <amount.h>
#include <key_io.h>
#include <logging.h>
#include <serialize.h>
#include <script/script.h>
#include <token/token.h>
#include <utilstrencodings.h>
#include <wallet/wallet.h>

class CToken;

const int ISSUANCE_ID_BEGIN = 16;
extern std::vector<CToken> known_issuances;

void get_next_issuance_id(uint64_t& id);
bool is_name_in_issuances(std::string& name);
bool is_identifier_in_issuances(uint64_t& identifier);
bool compare_token_name(std::string& prev_token_name, std::string& token_name);
bool check_token_name(std::string& tokenName, std::string& errorReason);
void strip_control_chars(std::string& instr);
opcodetype GetOpcode(int n);
int GetIntFromOpcode(opcodetype n);

#endif // TOKEN_UTIL_H
