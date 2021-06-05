// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/prevstake.h>

bool prevStakeInit{false};
std::deque<uint256> prevStake;

const int prevStakeHistory = 128;

void addPrevStake(uint256& proofHash)
{
    prevStake.push_back(proofHash);
}

void maintainPrevCache()
{
    while (prevStake.size() > prevStakeHistory) {
        prevStake.pop_front();
    }
}

bool checkPrevStake(uint256& proofHash, const CChainParams& chainparams)
{
    if (!prevStakeInit) {
        prevStake.clear();
        prevStakeInit = true;
    }

    if (pindexBestHeader->nHeight < chainparams.GetConsensus().nPrevStakeChecks) {
        return true;
    }

    maintainPrevCache();

    const auto it = find(prevStake.begin(), prevStake.end(), proofHash);
    if (it != prevStake.end()) {
        return false;
    }

    addPrevStake(proofHash);
    return true;
}

