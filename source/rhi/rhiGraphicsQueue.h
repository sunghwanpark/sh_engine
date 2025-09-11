#pragma once

#include "pch.h"

struct rhiSubmitInfo;
class rhiGraphicsQueue
{
public:
	virtual void submit(const rhiSubmitInfo& info) = 0;
};