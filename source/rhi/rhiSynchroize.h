#pragma once

#include "pch.h"

class rhiSemaphore 
{ 
public:
	virtual ~rhiSemaphore() = default;
};

class rhiFence
{ 
public: 
	virtual ~rhiFence() = default; 
};