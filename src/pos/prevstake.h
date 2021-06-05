// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef POS_PREVSTAKE_H
#define POS_PREVSTAKE_H

#include <chain.h>
#include <chainparams.h>
#include <consensus/params.h>
#include <pos/kernel.h>
#include <uint256.h>
#include <util.h>
#include <validation.h>

#include <deque>

void addPrevStake(uint256& proofHash);
void maintainPrevCache();
bool checkPrevStake(uint256& proofHash, const CChainParams& chainparams);

#endif // POS_PREVSTAKE_H
