// Copyright (c) 2021 pacprotocol
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_DB_H
#define TOKEN_DB_H

#include <util.h>
#include <dbwrapper.h>
#include <sync.h>
#include <token/db.h>
#include <token/token.h>
#include <token/util.h>

class CTokenDB;
extern CTokenDB *tokendb;

class CTokenDB : public CDBWrapper {
public:
    explicit CTokenDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    static const char DB_TOKEN = 'I';

public:
    void Init();
    void Flush();
    const std::vector<CToken> LoadKnownIssuances();
    uint64_t SaveKnownIssuances();
    bool ReadToken(uint64_t& tokenId, CToken& token);
    bool WriteToken(CToken& token);
    bool EraseToken(uint64_t& tokenId);
    bool ExistsToken(uint64_t& tokenId);
};

void LoadDB();
void SaveDB();

#endif // TOKEN_DB_H
