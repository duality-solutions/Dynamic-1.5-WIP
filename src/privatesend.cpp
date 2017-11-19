// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "privatesend.h"

#include "activedynode.h"
#include "consensus/validation.h"
#include "governance.h"
#include "init.h"
#include "instantsend.h"
#include "dynode-payments.h"
#include "dynode-sync.h"
#include "dynodeman.h"
#include "messagesigner.h"
#include "script/sign.h"
#include "txmempool.h"
#include "util.h"
#include "utilmoneystr.h"

#include <boost/lexical_cast.hpp>

CPrivateSendEntry::CPrivateSendEntry(const std::vector<CTxIn>& vecTxIn, const std::vector<CTxOut>& vecTxOut, const CTransaction& txCollateral) :
    txCollateral(txCollateral), addr(CService())
{
    BOOST_FOREACH(CTxIn txin, vecTxIn)
        vecTxPSIn.push_back(txin);
    BOOST_FOREACH(CTxOut txout, vecTxOut)
        vecTxPSOut.push_back(txout);
}

bool CPrivateSendEntry::AddScriptSig(const CTxIn& txin)
{
    BOOST_FOREACH(CTxPSIn& txdsin, vecTxPSIn) {
        if(txdsin.prevout == txin.prevout && txdsin.nSequence == txin.nSequence) {
            if(txdsin.fHasSig) return false;

            txdsin.scriptSig = txin.scriptSig;
            txdsin.prevPubKey = txin.prevPubKey;
            txdsin.fHasSig = true;

            return true;
        }
    }

    return false;
}

bool CPrivatesendQueue::Sign()
{
    if(!fDyNode) return false;

    std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(nTime) + boost::lexical_cast<std::string>(fReady);

    if(!CMessageSigner::SignMessage(strMessage, vchSig, activeDynode.keyDynode)) {
        LogPrintf("CPrivatesendQueue::Sign -- SignMessage() failed, %s\n", ToString());
        return false;
    }

    return CheckSignature(activeDynode.pubKeyDynode);
}

bool CPrivatesendQueue::CheckSignature(const CPubKey& pubKeyDynode)
{
    std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(nTime) + boost::lexical_cast<std::string>(fReady);
    std::string strError = "";

    if(!CMessageSigner::VerifyMessage(pubKeyDynode, vchSig, strMessage, strError)) {
        LogPrintf("CPrivatesendQueue::CheckSignature -- Got bad Dynode queue signature: %s; error: %s\n", ToString(), strError);
        return false;
    }

    return true;
}

bool CPrivatesendQueue::Relay(CConnman& connman)
{
    std::vector<CNode*> vNodesCopy = g_connman->CopyNodeVector();
    BOOST_FOREACH(CNode* pnode, vNodesCopy)
        if(pnode->nVersion >= MIN_PRIVATESEND_PEER_PROTO_VERSION)
            g_connman->PushMessage(pnode, NetMsgType::PSQUEUE, (*this));

    g_connman->ReleaseNodeVector(vNodesCopy);
    return true;
}

bool CPrivatesendBroadcastTx::Sign()
{
    if(!fDyNode) return false;

    std::string strMessage = tx.GetHash().ToString() + boost::lexical_cast<std::string>(sigTime);

    if(!CMessageSigner::SignMessage(strMessage, vchSig, activeDynode.keyDynode)) {
        LogPrintf("CPrivatesendBroadcastTx::Sign -- SignMessage() failed\n");
        return false;
    }

    return CheckSignature(activeDynode.pubKeyDynode);
}

bool CPrivatesendBroadcastTx::CheckSignature(const CPubKey& pubKeyDynode)
{
    std::string strMessage = tx.GetHash().ToString() + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";

    if(!CMessageSigner::VerifyMessage(pubKeyDynode, vchSig, strMessage, strError)) {
        LogPrintf("CPrivatesendBroadcastTx::CheckSignature -- Got bad pstx signature, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CPrivatesendBroadcastTx::IsExpired(int nHeight)
{
    // expire confirmed PSTXes after ~1h since confirmation
    return (nConfirmedHeight != -1) && (nHeight - nConfirmedHeight > 24);
}

void CPrivateSendBase::SetNull()
{
    // Both sides
    nState = POOL_STATE_IDLE;
    nSessionID = 0;
    nSessionDenom = 0;
    vecEntries.clear();
    finalMutableTransaction.vin.clear();
    finalMutableTransaction.vout.clear();
    nTimeLastSuccessfulStep = GetTimeMillis();
}

std::string CPrivateSendBase::GetStateString() const
{
    switch(nState) {
        case POOL_STATE_IDLE:                   return "IDLE";
        case POOL_STATE_QUEUE:                  return "QUEUE";
        case POOL_STATE_ACCEPTING_ENTRIES:      return "ACCEPTING_ENTRIES";
        case POOL_STATE_SIGNING:                return "SIGNING";
        case POOL_STATE_ERROR:                  return "ERROR";
        case POOL_STATE_SUCCESS:                return "SUCCESS";
        default:                                return "UNKNOWN";
    }
}

// Definitions for static data members
std::vector<CAmount> CPrivateSend::vecStandardDenominations;
std::map<uint256, CPrivatesendBroadcastTx> CPrivateSend::mapPSTX;
CCriticalSection CPrivateSend::cs_mappstx;

void CPrivateSend::InitStandardDenominations()
{
    vecStandardDenominations.clear();
    /* Denominations

        A note about convertability. Within mixing pools, each denomination
        is convertable to another.

        For example:
        1DRK+1000 == (.1DRK+100)*10
        10DRK+10000 == (1DRK+1000)*10
    */
    /* Disabled
    vecStandardDenominations.push_back( (100      * COIN)+100000 );
    */
    vecStandardDenominations.push_back( (10       * COIN)+10000 );
    vecStandardDenominations.push_back( (1        * COIN)+1000 );
    vecStandardDenominations.push_back( (.1       * COIN)+100 );
    vecStandardDenominations.push_back( (.01      * COIN)+10 );
    /* Disabled till we need them
    vecStandardDenominations.push_back( (.001     * COIN)+1 );
    */
}

// check to make sure the collateral provided by the client is valid
bool CPrivateSend::IsCollateralValid(const CTransaction& txCollateral)
{
    if(txCollateral.vout.empty()) return false;
    if(txCollateral.nLockTime != 0) return false;

    CAmount nValueIn = 0;
    CAmount nValueOut = 0;

    BOOST_FOREACH(const CTxOut txout, txCollateral.vout) {
        nValueOut += txout.nValue;

        if(!txout.scriptPubKey.IsNormalPaymentScript()) {
            LogPrintf ("CPrivateSend::IsCollateralValid -- Invalid Script, txCollateral=%s", txCollateral.ToString());
            return false;
        }
    }

    BOOST_FOREACH(const CTxIn txin, txCollateral.vin) {
        CCoins coins;
        if(!GetUTXOCoins(txin.prevout, coins)) {
            LogPrint("privatesend", "CPrivateSend::IsCollateralValid -- Unknown inputs in collateral transaction, txCollateral=%s", txCollateral.ToString());
            return false;
        }
        nValueIn += coins.vout[txin.prevout.n].nValue;
    }

    //collateral transactions are required to pay out a small fee to the miners
    if(nValueIn - nValueOut < GetCollateralAmount()) {
        LogPrint("privatesend", "CPrivateSend::IsCollateralValid -- did not include enough fees in transaction: fees: %d, txCollateral=%s", nValueOut - nValueIn, txCollateral.ToString());
        return false;
    }

    LogPrint("privatesend", "CPrivateSend::IsCollateralValid -- %s", txCollateral.ToString());

    {
        LOCK(cs_main);
        CValidationState validationState;
        if(!AcceptToMemoryPool(mempool, validationState, txCollateral, false, NULL, false, true, true)) {
            LogPrint("privatesend", "CPrivateSend::IsCollateralValid -- didn't pass AcceptToMemoryPool()\n");
            return false;
        }
    }

    return true;
}

/*  Create a nice string to show the denominations
    Function returns as follows (for 4 denominations):
        ( bit on if present )
        bit 0           - 100
        bit 1           - 10
        bit 2           - 1
        bit 3           - .1
        bit 4 and so on - out-of-bounds
        none of above   - non-denom
*/
std::string CPrivateSend::GetDenominationsToString(int nDenom)
{
    std::string strDenom = "";
    int nMaxDenoms = vecStandardDenominations.size();

    if(nDenom >= (1 << nMaxDenoms)) {
        return "out-of-bounds";
    }

    for (int i = 0; i < nMaxDenoms; ++i) {
        if(nDenom & (1 << i)) {
            strDenom += (strDenom.empty() ? "" : "+") + FormatMoney(vecStandardDenominations[i]);
        }
    }

    if(strDenom.empty()) {
        return "non-denom";
    }

    return strDenom;
}

int CPrivateSend::GetDenominations(const std::vector<CTxPSOut>& vecTxPSOut)
{
    std::vector<CTxOut> vecTxOut;

    BOOST_FOREACH(CTxPSOut out, vecTxPSOut)
        vecTxOut.push_back(out);

    return GetDenominations(vecTxOut);
}

/*  Return a bitshifted integer representing the denominations in this list
    Function returns as follows (for 4 denominations):
        ( bit on if present )
        100       - bit 0
        10        - bit 1
        1         - bit 2
        .1        - bit 3
        non-denom - 0, all bits off
*/
int CPrivateSend::GetDenominations(const std::vector<CTxOut>& vecTxOut, bool fSingleRandomDenom)
{
    std::vector<std::pair<CAmount, int> > vecDenomUsed;

    // make a list of denominations, with zero uses
    BOOST_FOREACH(CAmount nDenomValue, vecStandardDenominations)
        vecDenomUsed.push_back(std::make_pair(nDenomValue, 0));

    // look for denominations and update uses to 1
    BOOST_FOREACH(CTxOut txout, vecTxOut) {
        bool found = false;
        BOOST_FOREACH (PAIRTYPE(CAmount, int)& s, vecDenomUsed) {
            if(txout.nValue == s.first) {
                s.second = 1;
                found = true;
            }
        }
        if(!found) return 0;
    }

    int nDenom = 0;
    int c = 0;
    // if the denomination is used, shift the bit on
    BOOST_FOREACH (PAIRTYPE(CAmount, int)& s, vecDenomUsed) {
        int bit = (fSingleRandomDenom ? GetRandInt(2) : 1) & s.second;
        nDenom |= bit << c++;
        if(fSingleRandomDenom && bit) break; // use just one random denomination
    }

    return nDenom;
}

bool CPrivateSend::GetDenominationsBits(int nDenom, std::vector<int> &vecBitsRet)
{
    // ( bit on if present, 4 denominations example )
    // bit 0 - 100DYN+1
    // bit 1 - 10DYN+1
    // bit 2 - 1DYN+1
    // bit 3 - .1DYN+1

    int nMaxDenoms = vecStandardDenominations.size();

    if(nDenom >= (1 << nMaxDenoms)) return false;

    vecBitsRet.clear();

    for (int i = 0; i < nMaxDenoms; ++i) {
        if(nDenom & (1 << i)) {
            vecBitsRet.push_back(i);
        }
    }

    return !vecBitsRet.empty();
}

int CPrivateSend::GetDenominationsByAmounts(const std::vector<CAmount>& vecAmount)
{
    CScript scriptTmp = CScript();
    std::vector<CTxOut> vecTxOut;

    BOOST_REVERSE_FOREACH(CAmount nAmount, vecAmount) {
        CTxOut txout(nAmount, scriptTmp);
        vecTxOut.push_back(txout);
    }

    return GetDenominations(vecTxOut, true);
}

std::string CPrivateSend::GetMessageByID(PoolMessage nMessageID)
{
    switch (nMessageID) {
        case ERR_ALREADY_HAVE:          return _("Already have that input.");
        case ERR_DENOM:                 return _("No matching denominations found for mixing.");
        case ERR_ENTRIES_FULL:          return _("Entries are full.");
        case ERR_EXISTING_TX:           return _("Not compatible with existing transactions.");
        case ERR_FEES:                  return _("Transaction fees are too high.");
        case ERR_INVALID_COLLATERAL:    return _("Collateral not valid.");
        case ERR_INVALID_INPUT:         return _("Input is not valid.");
        case ERR_INVALID_SCRIPT:        return _("Invalid script detected.");
        case ERR_INVALID_TX:            return _("Transaction not valid.");
        case ERR_MAXIMUM:               return _("Entry exceeds maximum size.");
        case ERR_DN_LIST:               return _("Not in the Dynode list.");
        case ERR_MODE:                  return _("Incompatible mode.");
        case ERR_NON_STANDARD_PUBKEY:   return _("Non-standard public key detected.");
        case ERR_NOT_A_DN:              return _("This is not a Dynode."); // not used
        case ERR_QUEUE_FULL:            return _("Dynode queue is full.");
        case ERR_RECENT:                return _("Last PrivateSend was too recent.");
        case ERR_SESSION:               return _("Session not complete!");
        case ERR_MISSING_TX:            return _("Missing input transaction information.");
        case ERR_VERSION:               return _("Incompatible version.");
        case MSG_NOERR:                 return _("No errors detected.");
        case MSG_SUCCESS:               return _("Transaction created successfully.");
        case MSG_ENTRIES_ADDED:         return _("Your entries added successfully.");
        default:                        return _("Unknown response.");
    }
}

void CPrivateSend::AddPSTX(const CPrivatesendBroadcastTx& pstx)
{
    LOCK(cs_mappstx);
    mapPSTX.insert(std::make_pair(pstx.tx.GetHash(), pstx));
}

CPrivatesendBroadcastTx CPrivateSend::GetPSTX(const uint256& hash)
{
    LOCK(cs_mappstx);
    auto it = mapPSTX.find(hash);
    return (it == mapPSTX.end()) ? CPrivatesendBroadcastTx() : it->second;
}

void CPrivateSend::CheckPSTXes(int nHeight)
{
    LOCK(cs_mappstx);
    std::map<uint256, CPrivatesendBroadcastTx>::iterator it = mapPSTX.begin();
    while(it != mapPSTX.end()) {
        if (it->second.IsExpired(nHeight)) {
            mapPSTX.erase(it++);
        } else {
            ++it;
        }
    }
    LogPrint("privatesend", "CPrivateSend::CheckPSTXes -- mapPSTX.size()=%llu\n", mapPSTX.size());
}

void CPrivateSend::SyncTransaction(const CTransaction& tx, const CBlock* pblock)
{
    if (tx.IsCoinBase()) return;

    LOCK2(cs_main, cs_mappstx);

    uint256 txHash = tx.GetHash();
    if (!mapPSTX.count(txHash)) return;

    // When tx is 0-confirmed or conflicted, pblock is NULL and nConfirmedHeight should be set to -1
    CBlockIndex* pblockindex = NULL;
    if(pblock) {
        uint256 blockHash = pblock->GetHash();
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if(mi == mapBlockIndex.end() || !mi->second) {
            // shouldn't happen
            LogPrint("privatesend", "CPrivateSendClient::SyncTransaction -- Failed to find block %s\n", blockHash.ToString());
            return;
        }
        pblockindex = mi->second;
    }
    mapPSTX[txHash].SetConfirmedHeight(pblockindex ? pblockindex->nHeight : -1);
    LogPrint("privatesend", "CPrivateSendClient::SyncTransaction -- txid=%s\n", txHash.ToString());
}

//TODO: Rename/move to core
void ThreadCheckPrivateSend(CConnman& connman)
{
    if(fLiteMode) return; // disable all Dynamic specific functionality

    static bool fOneThread;
    if(fOneThread) return;
    fOneThread = true;

    // Make this thread recognisable as the PrivateSend thread
    RenameThread("dynamic-ps");

    unsigned int nTick = 0;

    while (true)
    {
        MilliSleep(1000);

        // try to sync from all available nodes, one step at a time
        dynodeSync.ProcessTick(connman);

        if(dynodeSync.IsBlockchainSynced() && !ShutdownRequested()) {

            nTick++;

            // make sure to check all dynodes first
            dnodeman.Check();

            // check if we should activate or ping every few minutes,
            // slightly postpone first run to give net thread a chance to connect to some peers
            if(nTick % DYNODE_MIN_DNP_SECONDS == 15)
                activeDynode.ManageState(connman);

            if(nTick % 60 == 0) {
                dnodeman.ProcessDynodeConnections(connman);
                dnodeman.CheckAndRemove(connman);
                dnpayments.CheckAndRemove();
                instantsend.CheckAndRemove();
            }
            if(fDyNode && (nTick % (60 * 5) == 0)) {
                dnodeman.DoFullVerificationStep(connman);
            }

            if(nTick % (60 * 5) == 0) {
                governance.DoMaintenance(connman);
            }
        }
    }
}