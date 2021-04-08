// Copyright (c) 2018-2020 The PACGlobal developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BANNED_H
#define BANNED_H

#include <validation.h>

void initBanned();
bool areBannedInputs(const uint256& txid, const unsigned int& vout);

#endif // BANNED_H
