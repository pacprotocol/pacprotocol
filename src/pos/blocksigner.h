#ifndef BLOCKSIGNER_H
#define BLOCKSIGNER_H

class CBlock;
class CPubKey;
class CKey;
class CKeyStore;

bool SignBlock(CBlock& block, const CKeyStore& keystore);
bool CheckBlockSignature(const CBlock& block);

#endif // BLOCKSIGNER_H
