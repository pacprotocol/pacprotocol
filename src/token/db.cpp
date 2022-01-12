// Copyright (c) 2021 pacprotocol
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <token/db.h>

class CTokenDB;

// this is annoying, but params isnt ready in time otherwise
CTokenDB* tokendb = nullptr;

CTokenDB::CTokenDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "token", nCacheSize, fMemory, fWipe) {
}

const std::vector<CToken> CTokenDB::LoadKnownIssuances()
{
    std::vector<CToken> info;
    uint64_t skipTokenCount = 0;
    uint64_t tokenId = ISSUANCE_ID_BEGIN;

    while (true) {
        tokenId++;
        CToken token;
        if (!tokendb->ReadToken(tokenId, token)) {
            skipTokenCount++;
            if (skipTokenCount > TOKEN_MAX_SKIP)
                break;
        } else {
            info.push_back(token);
        }
    }

    return info;
}

uint64_t CTokenDB::SaveKnownIssuances()
{
    uint64_t counter = 0;
    for (auto& l : known_issuances) {
        CToken& token = l;
        tokendb->WriteToken(token);
        counter++;
    }

    return counter;
}

void CTokenDB::ResetIssuanceState()
{
    known_issuances.clear();
    tokendb->Flush();
}

bool CTokenDB::ReadToken(uint64_t& tokenId, CToken& token)
{
    return Read(std::make_pair(DB_TOKEN, tokenId), token);
}

bool CTokenDB::WriteToken(CToken& token)
{
    uint64_t tokenId = token.getId();
    return Write(std::make_pair(DB_TOKEN, tokenId), token);
}

bool CTokenDB::EraseToken(uint64_t& tokenId)
{
    return Erase(std::make_pair(DB_TOKEN, tokenId));
}

bool CTokenDB::ExistsToken(uint64_t& tokenId)
{
    return Exists(std::make_pair(DB_TOKEN, tokenId));
}

void CTokenDB::Init()
{
    tokendb = new CTokenDB(4194304);
    known_issuances = LoadKnownIssuances();
    LogPrint(BCLog::TOKEN, "%s - Loaded %d token issuances from disk..\n", __func__, known_issuances.size());
}

void CTokenDB::Flush()
{
    uint64_t savedTokens = SaveKnownIssuances();
    LogPrint(BCLog::TOKEN, "%s - Saved %d token issuances to disk..\n", __func__, savedTokens);
}

void LoadDB()
{
    tokendb->Init();
}

void SaveDB()
{
    tokendb->Flush();
}

