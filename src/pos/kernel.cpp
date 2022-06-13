// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/kernel.h>

#include <chainparams.h>
#include <db.h>
#include <init.h>
#include <policy/policy.h>
#include <script/interpreter.h>
#include <timedata.h>
#include <txdb.h>
#include <util.h>
#include <utiltime.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <numeric>

using namespace std;

const int oldModifierInterval = 60 * 20;

int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd)
{
    return nIntervalEnd - nIntervalBeginning - Params().GetConsensus().nStakeMinAge;
}

static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier()) {
        nStakeModifier = 0;
        return true;
    }
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert(nSection >= 0 && nSection < 64);
    return (Params().GetConsensus().nModifierInterval * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

static int64_t GetStakeModifierSelectionInterval()
{
    int64_t nSelectionInterval = 0;
    for (int nSection = 0; nSection < 64; nSection++)
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection);
    return nSelectionInterval;
}

static bool SelectBlockFromCandidates(
    vector<pair<int64_t, uint256>>& vSortedByTimestamp,
    map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev,
    const CBlockIndex** pindexSelected)
{
    bool fSelected = false;
    arith_uint256 hashBest = 0;
    *pindexSelected = nullptr;
    for (const auto& item : vSortedByTimestamp) {
        if (!mapBlockIndex.count(item.second))
            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s", item.second.ToString().c_str());
        const CBlockIndex* pindex = mapBlockIndex[item.second];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;
        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;
        uint256 hashProof = pindex->IsProofOfStake() ? pindex->hashProofOfStake : pindex->GetBlockHash();
        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        arith_uint256 hashSelection = UintToArith256(Hash(ss.begin(), ss.end()));
        if (pindex->IsProofOfStake())
            hashSelection >>= 32;
        if (fSelected && hashSelection < hashBest) {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        } else if (!fSelected) {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        }
    }
    if (gArgs.GetBoolArg("-printstakemodifier", false))
        LogPrintf("%s : selection hash=%s\n", __func__, hashBest.ToString().c_str());
    return fSelected;
}

bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier)
{
    const Consensus::Params& params = Params().GetConsensus();
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev) {
        fGeneratedStakeModifier = true;
        return true;
    }
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("ComputeNextStakeModifier: unable to get last modifier");
    if (gArgs.GetBoolArg("-debug", false))
        LogPrintf("ComputeNextStakeModifier: prev modifier=0x%016x time=%s epoch=%u\n", nStakeModifier, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nModifierTime).c_str(), (unsigned int)nModifierTime);
    if (nModifierTime / oldModifierInterval >= pindexPrev->GetBlockTime() / oldModifierInterval)
        return true;

    // Sort candidate blocks by timestamp
    vector<pair<int64_t, uint256>> vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * oldModifierInterval / params.nPosTargetSpacing);
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval();
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / oldModifierInterval) * oldModifierInterval - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart) {
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound = 0; nRound < min(64, (int)vSortedByTimestamp.size()); nRound++) {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);
        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);
        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        if (gArgs.GetBoolArg("-printstakemodifier", false))
            LogPrintf("%s : selected round %d stop=%s height=%d bit=%d\n", __func__, nRound, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nSelectionIntervalStop).c_str(), pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    // Print selection map for visualization of the selected blocks
    if (gArgs.GetBoolArg("-debug", false) && gArgs.GetBoolArg("-printstakemodifier", false)) {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate) {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (const auto& item : mapSelectedBlocks) {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->IsProofOfStake() ? "S" : "W");
        }
        LogPrintf("ComputeNextStakeModifier: selection height [%d, %d] map %s\n", nHeightFirstCandidate, pindexPrev->nHeight, strSelectionMap);
    }
    //! LogPrintf("ComputeNextStakeModifier: new modifier=0x%016x time=%s\n", nStakeModifierNew, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", pindexPrev->GetBlockTime()).c_str());

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

static bool GetKernelStakeModifier(const CBlockIndex* pindexPrev, uint256 hashBlockFrom, unsigned int nTimeTx, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    const Consensus::Params& params = Params().GetConsensus();
    nStakeModifier = 0;
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("GetKernelStakeModifier() : block not indexed");
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval();

    // we need to iterate index forward but we cannot depend on chainActive.Next()
    // because there is no guarantee that we are checking blocks in active chain.
    // So, we construct a temporary chain that we will iterate over.
    // pindexFrom - this block contains coins that are used to generate PoS
    // pindexPrev - this is a block that is previous to PoS block that we are checking, you can think of it as tip of our chain
    std::vector<const CBlockIndex*> tmpChain;
    int32_t nDepth = pindexPrev->nHeight - (pindexFrom->nHeight - 1); // -1 is used to also include pindexFrom
    tmpChain.reserve(nDepth);
    const CBlockIndex* it = pindexPrev;
    for (int i = 1; i <= nDepth && !chainActive.Contains(it); i++) {
        tmpChain.push_back(it);
        it = it->pprev;
    }
    std::reverse(tmpChain.begin(), tmpChain.end());
    size_t n = 0;

    const CBlockIndex* pindex = pindexFrom;

    // loop to find the stake modifier later by a selection interval
    while (nStakeModifierTime < pindexFrom->GetBlockTime() + nStakeModifierSelectionInterval) {
        const CBlockIndex* old_pindex = pindex;
        pindex = (!tmpChain.empty() && pindex->nHeight >= tmpChain[0]->nHeight - 1) ? tmpChain[n++] : chainActive.Next(pindex);
        if (n > tmpChain.size() || pindex == NULL) // check if tmpChain[n+1] exists
        { // reached best block; may happen if node is behind on block chain
            if (fPrintProofOfStake || (old_pindex->GetBlockTime() + params.nStakeMinAge - nStakeModifierSelectionInterval > GetAdjustedTime()))
                return error("GetKernelStakeModifier() : reached best block %s at height %d from block %s",
                    old_pindex->GetBlockHash().ToString(), old_pindex->nHeight, hashBlockFrom.ToString());
            else
                return false;
        }
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
            nStakeModifier = pindex->nStakeModifier;
        }
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}

bool StakeKernelMode(const CBlockIndex* pindexPrev)
{
    if (pindexPrev->nHeight < Params().GetConsensus().nHardenedStakeCheckHeight)
        return false;
    return true;
}

bool CheckStakeKernelHash(unsigned int nBits, const CBlockIndex* pindexPrev, const CBlockHeader& blockFrom, unsigned int nTxPrevOffset, const CTransactionRef& txPrev, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake)
{
    bool fKernelMode = StakeKernelMode(pindexPrev);
    const Consensus::Params& params = Params().GetConsensus();

    // basic sanity checks
    auto txPrevTime = blockFrom.GetBlockTime();
    if (nTimeTx < txPrevTime)
        return error("CheckStakeKernelHash() : nTime violation");
    unsigned int nTimeBlockFrom = blockFrom.GetBlockTime();
    if (nTimeBlockFrom + params.nStakeMinAge > nTimeTx)
        return error("CheckStakeKernelHash() : min age violation");

    // discard stakes generated from inputs of less than x PAC
    CAmount nValueIn = txPrev->vout[prevout.n].nValue;
    if (nValueIn < params.nMinimumStakeValue)
        return error("CheckStakeKernelHash() : min amount violation");

    // old algorithm parameters
    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);
    int64_t nTimeWeight = std::min<int64_t>(nTimeTx - txPrevTime, params.nStakeMaxAge - params.nStakeMinAge);
    arith_uint256 bnCoinDayWeight = nValueIn * nTimeWeight / COIN / 200;

    // new algorithm parameters
    arith_uint256 bnTarget;
    if (fKernelMode) {
        bnTarget.SetCompact(nBits);
        arith_uint256 bnWeight = arith_uint256(nValueIn);
        bnTarget *= bnWeight;
    }

    // calculate hash
    CDataStream ss(SER_GETHASH, 0);
    uint64_t nStakeModifier = 0;
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;

    if (!GetKernelStakeModifier(pindexPrev, blockFrom.GetHash(), nTimeTx, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, false))
        return false;

    ss << nStakeModifier;
    ss << nTimeBlockFrom << nTxPrevOffset << txPrevTime << prevout.n << nTimeTx;
    hashProofOfStake = Hash(ss.begin(), ss.end());

    // LogPrintf("height: %d\n", pindexPrev->nHeight+1);
    // LogPrintf("ss << %016llx\n", nStakeModifier);
    // LogPrintf("ss << %08x << %08x << %08x << %08x << %08x\n", txPrevTime, nTxPrevOffset, txPrevTime, prevout.n, nTimeTx);
    // LogPrintf("(ss == %s, hashproof == %s)\n", HexStr(ss), hashProofOfStake.ToString());

    // choose appropriate target based on kernel mode
    arith_uint256 nTarget = fKernelMode ? bnTarget : bnCoinDayWeight * bnTargetPerCoinDay;

    // check if proof-of-stake hash meets target protocol
    if (hashProofOfStake == uint256())
        return false;
    if (UintToArith256(hashProofOfStake) > nTarget)
        return false;

    return true;
}

bool CheckKernelScript(CScript scriptVin, CScript scriptVout)
{
    auto extractKeyID = [](CScript scriptPubKey) {
        std::vector<std::vector<unsigned char>> vSolutions;
        CKeyID keyID;
        txnouttype whichType;
        if (Solver(scriptPubKey, whichType, vSolutions)) {
            if (whichType == TX_PUBKEYHASH) {
                keyID = CKeyID(uint160(vSolutions[0]));
            } else if (whichType == TX_PUBKEY) {
                keyID = CPubKey(vSolutions[0]).GetID();
            }
        }
        return keyID;
    };
    return extractKeyID(scriptVin) == extractKeyID(scriptVout);
}

bool CheckProofOfStake(const CBlock& block, uint256& hashProofOfStake, const CBlockIndex* pindexPrev)
{
    const CTransactionRef& tx = block.vtx[1];
    if (!tx->IsCoinStake())
        return error("CheckProofOfStake() : called on non-coinstake %s", tx->GetHash().ToString());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txin = tx->vin[0];

    // get txprev from the txindex
    uint256 blockHash {};
    CBlockHeader header;
    CTransactionRef txPrev;
    if (!pblocktree->FindTx(txin.prevout.hash, blockHash, txPrev))
        return error("CheckProofOfStake() : tx index not found");

    // retrieve blockheader
    CBlockIndex* pblockindex = mapBlockIndex[blockHash];
    if (!pblockindex)
        return false;
    header = pblockindex->GetBlockHeader();

    {
        // verify signature
        {
            int nIn = 0;
            const CTxOut& prevOut = txPrev->vout[tx->vin[nIn].prevout.n];
            if (ibd_complete && !CheckKernelScript(prevOut.scriptPubKey, tx->vout[1].scriptPubKey)) {
                return error("CheckProofOfStake: VerifyScript failed on coinstake %s", tx->GetHash().ToString());
            }
        }

        // check the stakehash
        unsigned int nBits = block.nBits;
        unsigned int nTime = block.nTime;
        if (!CheckStakeKernelHash(nBits, pindexPrev, header, sizeof(CBlock), txPrev, txin.prevout, nTime, hashProofOfStake)) {
            return error("CheckProofOfStake() : INFO: check kernel failed on coinstake %s, hashProof=%s (errorlevel %d)\n",
                tx->GetHash().ToString(), hashProofOfStake.ToString());
        }
    }

    return true;
}
