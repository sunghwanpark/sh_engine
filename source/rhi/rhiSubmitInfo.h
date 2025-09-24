#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"

class rhiCommandList;
class rhiSemaphore;
class rhiFence;

struct rhiSemaphoreSubmitInfo
{    
    rhiSemaphore* semaphore;
    u64 value; // timeline 
    rhiPipelineStage stage;
};

struct rhiSubmitInfo 
{
    std::vector<rhiCommandList*> cmd_lists;
    std::vector<rhiSemaphoreSubmitInfo> waits;
    std::vector<rhiSemaphoreSubmitInfo> signals;
    rhiFence* fence = nullptr;
};