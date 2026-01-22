/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2018 Erwin Coumans  http://bulletphysics.com

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

Vita-specific implementation: This file provides a PS Vita implementation of btThreadSupportInterface
using VitaSDK threading APIs (sceKernel* functions) instead of POSIX threads.

PS Vita implementation contributed by Charlie Savard.
*/

#ifndef BT_THREAD_SUPPORT_VITA_H
#define BT_THREAD_SUPPORT_VITA_H

#if BT_THREADSAFE && defined(VITA_BUILD)

#include "LinearMath/btScalar.h"
#include "LinearMath/btAlignedObjectArray.h"
#include "LinearMath/btThreads.h"
#include "btThreadSupportInterface.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>
#include <stdio.h>

class btThreadSupportVita : public btThreadSupportInterface
{
public:
	struct btThreadStatus
	{
		int m_taskId;
		int m_commandId;
		int m_status;

		ThreadFunc m_userThreadFunc;
		void* m_userPtr;

		SceUID threadId;
		SceUID startSemaphore;
		btCriticalSection* m_cs;
		SceUID m_mainSemaphore;
		unsigned long threadUsed;
	};

private:
	typedef unsigned long long UINT64;

	btAlignedObjectArray<btThreadStatus> m_activeThreadStatus;
	SceUID m_mainSemaphore;
	int m_numThreads;
	UINT64 m_startedThreadsMask;
	void startThreads(const ConstructionInfo& threadInfo);
	void stopThreads();
	int waitForResponse();
	btCriticalSection* m_cs;

public:
	btThreadSupportVita(const ConstructionInfo& threadConstructionInfo);
	virtual ~btThreadSupportVita();

	virtual int getNumWorkerThreads() const BT_OVERRIDE { return m_numThreads; }
	virtual int getCacheFriendlyNumThreads() const BT_OVERRIDE { return m_numThreads + 1; }
	virtual int getLogicalToPhysicalCoreRatio() const BT_OVERRIDE { return 1; }

	virtual void runTask(int threadIndex, void* userData) BT_OVERRIDE;
	virtual void waitForAllTasks() BT_OVERRIDE;

	virtual btCriticalSection* createCriticalSection() BT_OVERRIDE;
	virtual void deleteCriticalSection(btCriticalSection* criticalSection) BT_OVERRIDE;
};

#endif // BT_THREADSAFE && defined(VITA_BUILD)

#endif // BT_THREAD_SUPPORT_VITA_H

