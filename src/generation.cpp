// Copyright (c) 2021 pacprotocol
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <generation.h>

typedef std::map<int, CAmount> GeneratedFunds;

const GeneratedFunds creationPoints = {
    { 686750, 1320000000 * COIN }
};

bool isGenerationBlock(int nHeight)
{
    for (auto genpairs : creationPoints)
        if (genpairs.first == nHeight)
            return true;
    return false;
}

CAmount getGenerationAmount(int nHeight)
{
    for (auto genpairs : creationPoints)
        if (genpairs.first == nHeight)
            return genpairs.second;
    return 0;
}

bool isGenerationRecipient(std::string recipient)
{
    const std::string testRecipient = "5b1c713017e9e6e019264b0e6e3e8c3a5e03a3db";
    if (recipient.find(testRecipient) != std::string::npos)
        return true;
    return false;
}

bool testGenerationBlock(int nBlockHeight, const CTxOut& payee)
{
    bool correctRecipient = false;
    if (isGenerationRecipient(HexStr(payee.scriptPubKey))) {
        if (getGenerationAmount(nBlockHeight) == payee.nValue) {
            correctRecipient = true;
        }
    }
    return correctRecipient;
}
