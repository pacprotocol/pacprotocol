// Copyright (c) 2021 pacprotocol
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <token/index.h>

#define MILLI 0.001

void ScanForTokenMetadata(int lastHeight, const Consensus::Params& params)
{
    if (params.nTokenHeight > lastHeight) {
        return;
    }

    for (int height = params.nTokenHeight; height < lastHeight; ++height) {

        const CBlockIndex* pindex = chainActive[height];

        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, params)) {
            continue;
        }

        for (unsigned int i = 0; i < block.vtx.size(); i++) {

            const CTransactionRef& tx = block.vtx[i];
            if (!tx->HasTokenOutput()) {
                continue;
            }

            std::string strError;
            if (!CheckToken(tx, false, strError, params)) {
                LogPrint(BCLog::TOKEN, "%s - error %s", __func__, strError);
                continue;
            }
        }
    }
}

void BlockUntilTokenMetadataSynced()
{
    LOCK(cs_main);

    const auto& consensus_params = Params().GetConsensus();
    int currentHeight = chainActive.Height();

    int64_t nStart = GetTimeMillis();
    ScanForTokenMetadata(currentHeight, consensus_params);
    int64_t nEnd = GetTimeMillis();

    LogPrint(BCLog::TOKEN, "%s - token index synced in %.2fms\n", __func__, MILLI * (nEnd - nStart));
}
