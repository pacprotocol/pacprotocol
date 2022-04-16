// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_ISSUANCES_H
#define TOKEN_ISSUANCES_H

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

#include <mutex>

#include <boost/algorithm/string.hpp>

class CToken;
class CTxMemPool;

const int ISSUANCE_ID_BEGIN = 16;
extern std::vector<CToken> known_issuances;

void get_next_issuance_id(uint64_t& id);
bool is_name_in_issuances(std::string& name);
bool is_identifier_in_issuances(uint64_t& identifier);
bool get_id_for_token_name(std::string& name, uint64_t& id);
std::vector<CToken> copy_issuances_vector();
uint64_t get_issuances_size();
void add_to_issuances(CToken& token);

#endif // TOKEN_ISSUANCES_H
