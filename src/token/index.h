// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PACPROTOCOL_TOKEN_INDEX_H
#define PACPROTOCOL_TOKEN_INDEX_H

#include <chainparams.h>
#include <token/verify.h>
#include <validation.h>

bool ScanForTokenMetadata(int lastHeight, const Consensus::Params& params);
bool BlockUntilTokenMetadataSynced(const Consensus::Params& params);

#endif // PACPROTOCOL_TOKEN_INDEX_H
