// Copyright (c) 2009-2019 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Developers
// Copyright (c) 2013-2017 Emercoin Developers
// Copyright (c) 2014-2017 The Dash Developers
// Copyright (c) 2016-2019 Duality Blockchain Solutions Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DYNAMIC_PRIMITIVES_TRANSACTION_H
#define DYNAMIC_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"

/** ASSET START */
class CNullAssetTxVerifierString;
/** ASSET END */

enum DataOutputTypes
{
    DO_NULL                 = 0, // reserved
    DO_STEALTH              = 1,
    DO_STEALTH_PREFIX       = 2,
};

class CTransaction;

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, uint32_t nIn)
    {
        hash = hashIn;
        n = nIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(hash);
        READWRITE(n);
    }

    void SetNull()
    {
        hash.SetNull();
        n = (uint32_t)-1;
    }
    bool IsNull() const { return (hash.IsNull() && n == (uint32_t)-1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        int cmp = a.hash.Compare(b.hash);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
    std::string ToStringShort() const;
    std::string ToSerializedString() const;
    bool IsDynodeReward(const CTransaction* tx) const;

};

/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;

    /* Setting nSequence to this value for every input in a transaction
     * disables nLockTime. */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /* If this flag set, CTxIn::nSequence is NOT interpreted as a
     * relative lock-time. */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1 << 31);

    /* If CTxIn::nSequence encodes a relative lock-time and this flag
     * is set, the relative lock-time has units of 512 seconds,
     * otherwise it specifies blocks with a granularity of 1. */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /* If CTxIn::nSequence encodes a relative lock-time, this mask is
     * applied to extract that lock-time from the sequence field. */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /* In order to use the same number of bits to encode roughly the
     * same wall-clock duration, and because blocks are naturally
     * limited to occur every 600s on average, the minimum granularity
     * for time-based relative lock-time is fixed at 512 seconds.
     * Converting from CTxIn::nSequence to seconds is performed by
     * multiplying by 512 = 2^9, or equivalently shifting up by
     * 9 bits. */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn()
    {
        nSequence = SEQUENCE_FINAL;
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn = CScript(), uint32_t nSequenceIn = SEQUENCE_FINAL);
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn = CScript(), uint32_t nSequenceIn = SEQUENCE_FINAL);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(prevout);
        READWRITE(*(CScriptBase*)(&scriptSig));
        READWRITE(nSequence);
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    friend bool operator<(const CTxIn& a, const CTxIn& b)
    {
        return a.prevout < b.prevout;
    }

    std::string ToString() const;
};

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;
    int nRounds;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, int nRoundsIn = -10);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nValue);
        READWRITE(*(CScriptBase*)(&scriptPubKey));
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
        nRounds = -10; // an initial value, should be no way to get this by calculations
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    void SetEmpty()
    {
        nValue = 0;
        scriptPubKey.clear();
    }

    bool IsEmpty() const
    {
        return (nValue == 0 && scriptPubKey.empty());
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue == b.nValue &&
                a.scriptPubKey == b.scriptPubKey &&
                a.nRounds == b.nRounds);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    bool IsBDAP() const;
    bool IsData() const;
    bool GetBDAPOpCodes(int& opCode1, int& opCode2) const;
    bool GetBDAPOpCodes(int& opCode1, int& opCode2, std::vector<std::vector<unsigned char>>& vvch) const;

    std::string ToString() const;
};

struct CMutableTransaction;
/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
public:
    // Default transaction version.
    static const int32_t CURRENT_VERSION = 1;

    // Changing the default transaction version requires a two step process: first
    // adapting relay policy by bumping MAX_STANDARD_VERSION, and then later date
    // bumping the default CURRENT_VERSION at which point both CURRENT_VERSION and
    // MAX_STANDARD_VERSION will be equal.
    static const int32_t MAX_STANDARD_VERSION = 2;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const int32_t nVersion;
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const uint32_t nLockTime;

private:
    /** Memory only. */
    const uint256 hash;

    uint256 ComputeHash() const;

public:
    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    CTransaction(const CMutableTransaction& tx);
    CTransaction(CMutableTransaction&& tx);

    CTransaction& operator=(const CTransaction& tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const
    {
        s << this->nVersion;
        s << vin;
        s << vout;
        s << nLockTime;
    }

    /** This deserializing constructor is provided instead of an Unserialize method.
     *  Unserialize is not possible, since it would require overwriting const fields. */
    template <typename Stream>
    CTransaction(deserialize_type, Stream& s) : CTransaction(CMutableTransaction(deserialize, s))
    {
    }

    bool IsNull() const
    {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const
    {
        return hash;
    }

    // Return sum of txouts.
    CAmount GetValueOut() const;
    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    // Compute priority, given priority of inputs and (optionally) tx size
    double ComputePriority(double dPriorityInputs, unsigned int nTxSize = 0) const;

    // Compute modified tx size for priority calculation (optionally given tx size)
    unsigned int CalculateModifiedSize(unsigned int nTxSize = 0) const;

    /** ASSET START */
    bool IsNewAsset() const;
    bool VerifyNewAsset(std::string& strError) const;
    bool IsNewUniqueAsset() const;
    bool VerifyNewUniqueAsset(std::string& strError) const;
    bool IsReissueAsset() const;
    bool VerifyReissueAsset(std::string& strError) const;
    bool IsNewMsgChannelAsset() const;
    bool VerifyNewMsgChannelAsset(std::string& strError) const;
    bool IsNewQualifierAsset() const;
    bool VerifyNewQualfierAsset(std::string &strError) const;
    bool IsNewRestrictedAsset() const;
    bool VerifyNewRestrictedAsset(std::string& strError) const;

    bool CheckAddingTagBurnFee(const int& count) const;

    bool GetVerifierStringFromTx(CNullAssetTxVerifierString& verifier, std::string& strError) const;
    bool GetVerifierStringFromTx(CNullAssetTxVerifierString& verifier, std::string& strError, bool& fNotFound) const;

    /** ASSET END */

    /**
     * Get the total transaction size in bytes.
     * "Total Size" defined in BIP141 and BIP144.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const;

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    bool IsCoinStake() const;

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    std::string ToString() const;
};

/** A mutable version of CTransaction. */
struct CMutableTransaction {
    int32_t nVersion;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    uint32_t nLockTime;

    CMutableTransaction();
    CMutableTransaction(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(this->nVersion);
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream& s)
    {
        Unserialize(s);
    }

    /** Compute the hash of this CMutableTransaction. This is computed on the
     * fly, as opposed to GetHash() in CTransaction, which uses a cached result.
     */
    uint256 GetHash() const;

    std::string ToString() const;

    friend bool operator==(const CMutableTransaction& a, const CMutableTransaction& b)
    {
        return a.GetHash() == b.GetHash();
    }

    friend bool operator!=(const CMutableTransaction& a, const CMutableTransaction& b)
    {
        return !(a == b);
    }
};

typedef std::shared_ptr<const CTransaction> CTransactionRef;
static inline CTransactionRef MakeTransactionRef() { return std::make_shared<const CTransaction>(); }
template <typename Tx>
static inline CTransactionRef MakeTransactionRef(Tx&& txIn)
{
    return std::make_shared<const CTransaction>(std::forward<Tx>(txIn));
}

/** Implementation of BIP69
 * https://github.com/bitcoin/bips/blob/master/bip-0069.mediawiki
 */
struct CompareInputBIP69 {
    inline bool operator()(const CTxIn& a, const CTxIn& b) const
    {
        if (a.prevout.hash == b.prevout.hash)
            return a.prevout.n < b.prevout.n;

        uint256 hasha = a.prevout.hash;
        uint256 hashb = b.prevout.hash;

        typedef std::reverse_iterator<const unsigned char*> rev_it;
        rev_it rita = rev_it(hasha.end());
        rev_it ritb = rev_it(hashb.end());

        return std::lexicographical_compare(rita, rita + hasha.size(), ritb, ritb + hashb.size());
    }
};

struct CompareOutputBIP69 {
    inline bool operator()(const CTxOut& a, const CTxOut& b) const
    {
        return a.nValue < b.nValue || (a.nValue == b.nValue && a.scriptPubKey < b.scriptPubKey);
    }
};

#endif // DYNAMIC_PRIMITIVES_TRANSACTION_H
