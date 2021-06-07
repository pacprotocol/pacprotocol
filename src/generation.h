// Copyright (c) 2021 pacprotocol
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GENERATION_H
#define GENERATION_H

#include <amount.h>
#include <primitives/transaction.h>
#include <utilstrencodings.h>

#include <string>
#include <map>

bool isGenerationBlock(int nHeight);
CAmount getGenerationAmount(int nHeight);
bool isGenerationRecipient(std::string recipient);
bool testGenerationBlock(int nBlockHeight, const CTxOut& payee);

#endif // GENERATION_H
