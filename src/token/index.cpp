// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <token/index.h>

#define MILLI 0.001

bool ScanForTokenMetadata(int lastHeight, const Consensus::Params& params)
{
    if (lastHeight < params.nTokenHeight) {
        LogPrint(BCLog::TOKEN, "%s - loaded chain hasnt entered token phase\n", __func__);
        return true;
    }

    for (int height = params.nTokenHeight; height < lastHeight; ++height) {
        const CBlockIndex* pindex = chainActive[height];
        if (!pindex) {
            LogPrint(BCLog::TOKEN, "%s - error reading blockindex for height %d\n", __func__, height);
            return false;
        }

        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, params)) {
            LogPrint(BCLog::TOKEN, "%s - error reading block %d from disk\n", __func__, height);
            return false;
        }

        CCoinsViewCache& view = *pcoinsTip;
        for (unsigned int i = 0; i < block.vtx.size(); i++) {
            const CTransactionRef& tx = block.vtx[i];
            if (!tx->HasTokenOutput()) {
                continue;
            }

            std::string strError;
            if (!CheckToken(tx, pindex, view, strError, params, false)) {
                LogPrint(BCLog::TOKEN, "%s - error %s (height %d)\n", __func__, strError, height);
                return false;
            }
        }
    }

    return true;
}

bool BlockUntilTokenMetadataSynced(const Consensus::Params& params)
{
    LOCK(cs_main);

    int currentHeight = chainActive.Height();

    int64_t nStart = GetTimeMillis();
    if (!ScanForTokenMetadata(currentHeight, params)) {
        return false;
    }
    int64_t nEnd = GetTimeMillis();

    LogPrint(BCLog::TOKEN, "%s - token index synced in %.2fms\n", __func__, MILLI * (nEnd - nStart));

    return true;
}
