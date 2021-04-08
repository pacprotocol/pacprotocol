// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include <uint256.h>
#include <streams.h>
#include <arith_uint256.h>
#include <primitives/transaction.h>

#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>

class CBlock;
class CWallet;
class COutPoint;
class CBlockIndex;
class CBlockHeader;

extern int64_t nLastCoinStakeSearchInterval;

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

// Check whether stake kernel meets hash target
// Sets hashProofOfStake on success return
bool CheckStakeKernelHash(unsigned int nBits, const CBlockIndex* pindexPrev, const CBlockHeader& blockFrom, unsigned int nTxPrevOffset, const CTransactionRef& txPrev, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake);

// wrapper for checkstakekernelhash (bitcoin routine) for traditional method
bool CheckStake(unsigned int nBits, const CBlock blockFrom, const CTransaction txPrev, const COutPoint prevout, unsigned int& nTimeTx, unsigned int nHashDrift, bool fCheck, uint256& hashProofOfStake, bool fPrintProofOfStake);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(const CBlock &block, uint256& hashProofOfStake, const CBlockIndex* pindexPrev);

#endif // BITCOIN_KERNEL_H
