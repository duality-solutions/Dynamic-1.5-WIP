// Copyright (c) 2016-2018 Duality Blockchain Solutions Developers
// Copyright (c) 2014-2018 The Dash Core Developers
// Copyright (c) 2009-2018 The Bitcoin Developers
// Copyright (c) 2009-2018 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner/miner.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "miner/internal/miners-controller.h"
#include "net.h"
#include "primitives/transaction.h"
#include "utilmoneystr.h"
#include "validation.h"
#include "validationinterface.h"

void StartMiners() { gMiners->Start(); };

void StartCPUMiners() { gMiners->group_cpu().Start(); };

void StartGPUMiners()
{
#ifdef ENABLE_GPU
    gMiners->group_gpu().Start();
#endif // ENABLE_GPU
};

void ShutdownMiners() { gMiners->Shutdown(); };

void ShutdownCPUMiners() { gMiners->group_cpu().Shutdown(); };

void ShutdownGPUMiners()
{
#ifdef ENABLE_GPU
    gMiners->group_gpu().Shutdown();
#endif // ENABLE_GPU
};

int64_t GetHashRate() { return gMiners->GetHashRate(); };

int64_t GetCPUHashRate() { return gMiners->group_cpu().GetHashRate(); };

int64_t GetGPUHashRate()
{
#ifdef ENABLE_GPU
    return gMiners->group_gpu().GetHashRate();
#else
    return 0;
#endif // ENABLE_GPU
};

void SetCPUMinerThreads(uint8_t target) { gMiners->group_cpu().SetNumThreads(target); };

void SetGPUMinerThreads(uint8_t target)
{
#ifdef ENABLE_GPU
    gMiners->group_gpu().SetNumThreads(target);
#endif // ENABLE_GPU
};

std::unique_ptr<MinersController> gMiners = {nullptr};