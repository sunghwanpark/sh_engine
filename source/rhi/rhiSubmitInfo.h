#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"

class rhiCommandList;
class rhiSemaphore;
class rhiFence;
struct rhiSubmitInfo 
{
    rhiCommandList* cmds;
    rhiSemaphore* wait_semaphore;
    rhiSemaphore* signal_semaphore;
    rhiFence* fence = nullptr;
};