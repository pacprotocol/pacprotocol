// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <token/issuances.h>

std::mutex issuances_mutex;
std::vector<CToken> known_issuances;

void get_next_issuance_id(uint64_t& id)
{
    std::lock_guard<std::mutex> lock(issuances_mutex);

    id = ISSUANCE_ID_BEGIN;
    for (CToken& token : known_issuances) {
        uint64_t next_id = token.getId();
        if (next_id >= id) {
            id = next_id;
        }
    }

    uint64_t mempoolId;
    std::string strError;
    if (!CheckMempoolId(mempoolId, strError)) {
        return;
    }
    if (mempoolId > id) {
        id = mempoolId;
    }

    id++;
}

bool is_name_in_issuances(std::string& name)
{
    std::lock_guard<std::mutex> lock(issuances_mutex);

    for (CToken& token : known_issuances) {
        if (token.getName() == name) {
            return true;
        }
    }
    return false;
}

bool is_identifier_in_issuances(uint64_t& identifier)
{
    std::lock_guard<std::mutex> lock(issuances_mutex);

    for (CToken& token : known_issuances) {
        if (token.getId() == identifier) {
            return true;
        }
    }
    return false;
}

bool get_id_for_token_name(std::string& name, uint64_t& id)
{
    std::lock_guard<std::mutex> lock(issuances_mutex);

    for (CToken& token : known_issuances) {
        if (name == token.getName()) {
            id = token.getId();
            return true;
        }
    }
    return false;
}

std::vector<CToken> copy_issuances_vector()
{
    std::lock_guard<std::mutex> lock(issuances_mutex);

    std::vector<CToken> temp_known_issuances = known_issuances;
    return temp_known_issuances;
}

uint64_t get_issuances_size()
{
    std::lock_guard<std::mutex> lock(issuances_mutex);

    return known_issuances.size();
}

void add_to_issuances(CToken& token)
{
    std::lock_guard<std::mutex> lock(issuances_mutex);

    known_issuances.push_back(token);
}
