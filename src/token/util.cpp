// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <token/util.h>

extern CTxMemPool mempool;
std::vector<CToken> known_issuances;

void get_next_issuance_id(uint64_t& id)
{
    id = ISSUANCE_ID_BEGIN;
    for (CToken& token : known_issuances) {
        uint64_t next_id = token.getId();
        if (next_id >= id) {
            id = next_id;
        }
    }
    id++;
}

bool is_name_in_issuances(std::string& name)
{
    for (CToken& token : known_issuances) {
        if (token.getName() == name) {
            return true;
        }
    }
    return false;
}

bool is_identifier_in_issuances(uint64_t& identifier)
{
    for (CToken& token : known_issuances) {
        if (token.getId() == identifier) {
            return true;
        }
    }
    return false;
}

bool compare_token_name(std::string& prev_token_name, std::string& token_name)
{
    return (prev_token_name.compare(token_name) == 0);
}

bool check_token_name(std::string& tokenName, std::string& errorReason)
{
    if (tokenName.length() < TOKENNAME_MINLEN || tokenName.length() > TOKENNAME_MAXLEN) {
        errorReason = "tokenname-bounds-exceeded";
        return false;
    }

    std::string cleanedName = SanitizeString(tokenName);
    if (cleanedName.length() != tokenName.length()) {
        errorReason = "tokenname-bounds-inconsistent";
        return false;
    }

    if (cleanedName.compare(tokenName) != 0) {
        errorReason = "tokenname-payload-inconsistent";
        return false;
    }

    return true;
}

void strip_control_chars(std::string& instr)
{
    std::string outstr;
    outstr.clear();
    for (int i = 0; i < instr.size(); i++) {
        if (std::isalnum(instr[i])) {
            outstr += instr[i];
        }
    }
    instr = outstr;
}

bool is_in_mempool(uint256& txhash) {
    LOCK(mempool.cs);
    if (mempool.exists(txhash)) {
        return true;
    }
    return false;
}

void remove_from_mempool(CTransaction& tx) {
    LOCK(mempool.cs);
    mempool.removeRecursive(tx, MemPoolRemovalReason::CONFLICT);
}

bool is_output_unspent(const COutPoint& out)
{
    Coin coin;
    if (!GetUTXOCoin(out, coin)) {
        return false;
    }
    return true;
}

opcodetype GetOpcode(int n)
{
    opcodetype ret = OP_0;
    switch (n) {
    case 1:
        ret = OP_1;
        break;
    case 2:
        ret = OP_2;
        break;
    case 3:
        ret = OP_3;
        break;
    case 4:
        ret = OP_4;
        break;
    case 5:
        ret = OP_5;
        break;
    case 6:
        ret = OP_6;
        break;
    case 7:
        ret = OP_7;
        break;
    case 8:
        ret = OP_8;
        break;
    case 9:
        ret = OP_9;
        break;
    case 10:
        ret = OP_10;
        break;
    case 11:
        ret = OP_11;
        break;
    case 12:
        ret = OP_12;
        break;
    case 13:
        ret = OP_13;
        break;
    case 14:
        ret = OP_14;
        break;
    case 15:
        ret = OP_15;
        break;
    case 16:
        ret = OP_16;
        break;
    default:
        break;
    }
    return ret;
}

int GetIntFromOpcode(opcodetype n)
{
    int ret = 0;

    switch (n) {
    case OP_1:
        ret = 1;
        break;
    case OP_2:
        ret = 2;
        break;
    case OP_3:
        ret = 3;
        break;
    case OP_4:
        ret = 4;
        break;
    case OP_5:
        ret = 5;
        break;
    case OP_6:
        ret = 6;
        break;
    case OP_7:
        ret = 7;
        break;
    case OP_8:
        ret = 8;
        break;
    case OP_9:
        ret = 9;
        break;
    case OP_10:
        ret = 10;
        break;
    case OP_11:
        ret = 11;
        break;
    case OP_12:
        ret = 12;
        break;
    case OP_13:
        ret = 13;
        break;
    case OP_14:
        ret = 14;
        break;
    case OP_15:
        ret = 15;
        break;
    case OP_16:
        ret = 16;
        break;
    default:
        break;
    }
    return ret;
}
