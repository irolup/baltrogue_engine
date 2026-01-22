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

#if BT_THREADSAFE && defined(VITA_BUILD)

#include "btThreadSupportVita.h"
#include "LinearMath/btMinMax.h"
#include "LinearMath/btThreads.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifndef btAssert
#define btAssert assert
#endif

static int g_desiredVitaThreadCount = 1;

extern "C" void btSetDesiredVitaThreadCount(int count) {
	g_desiredVitaThreadCount = count;
}

#define checkVitaFunction(returnValue) \
	if (returnValue < 0) \
	{ \
		printf("Vita API error at line %i in file %s: 0x%x\n", __LINE__, __FILE__, returnValue); \
	}

static SceUID createVitaSem(const char* baseName, int initialValue)
{
	static int semCount = 0;
	char name[32];
	snprintf(name, 32, "%s_%d", baseName, semCount++);
	
	SceUID semId = sceKernelCreateSema(name, 0, initialValue, 1, nullptr);
	if (semId < 0) {
		printf("Failed to create Vita semaphore '%s': 0x%x\n", name, semId);
		return -1;
	}
	return semId;
}

static void destroyVitaSem(SceUID semId)
{
	if (semId >= 0) {
		sceKernelDeleteSema(semId);
	}
}

class btCriticalSectionVita : public btCriticalSection
{
	SceUID m_mutexId;
	const char* m_name;

public:
	btCriticalSectionVita(const char* name = "BulletCS")
		: m_name(name)
	{
		char mutexName[32];
		static int mutexCount = 0;
		snprintf(mutexName, 32, "%s_%d", name, mutexCount++);
		
		m_mutexId = sceKernelCreateMutex(mutexName, 0x02, 1, nullptr);
		if (m_mutexId < 0) {
			printf("Failed to create Vita mutex '%s': 0x%x\n", mutexName, m_mutexId);
		}
	}

	virtual ~btCriticalSectionVita()
	{
		if (m_mutexId >= 0) {
			sceKernelDeleteMutex(m_mutexId);
		}
	}

	virtual void lock() BT_OVERRIDE
	{
		if (m_mutexId >= 0) {
			sceKernelLockMutex(m_mutexId, 1, nullptr);
		}
	}

	virtual void unlock() BT_OVERRIDE
	{
		if (m_mutexId >= 0) {
			sceKernelUnlockMutex(m_mutexId, 1);
		}
	}
};

static int vitaThreadFunction(SceSize args, void* argp)
{
	btThreadSupportVita::btThreadStatus* status = nullptr;
	if (args >= sizeof(btThreadSupportVita::btThreadStatus*)) {
		memcpy(&status, argp, sizeof(btThreadSupportVita::btThreadStatus*));
	}

	if (!status) {
		sceKernelExitDeleteThread(-1);
		return -1;
	}

	while (1)
	{
		sceKernelWaitSema(status->startSemaphore, 1, nullptr);
		
		void* userPtr = status->m_userPtr;

		if (userPtr)
		{
			btAssert(status->m_status);
			status->m_userThreadFunc(userPtr);
			status->m_cs->lock();
			status->m_status = 2;
			status->m_cs->unlock();
			sceKernelSignalSema(status->m_mainSemaphore, 1);
			status->threadUsed++;
		}
		else
		{
			status->m_cs->lock();
			status->m_status = 3;
			status->m_cs->unlock();
			sceKernelSignalSema(status->m_mainSemaphore, 1);
			break;
		}
	}

	sceKernelExitDeleteThread(0);
	return 0;
}

btThreadSupportVita::btThreadSupportVita(const ConstructionInfo& threadConstructionInfo)
{
	m_cs = createCriticalSection();
	startThreads(threadConstructionInfo);
}

btThreadSupportVita::~btThreadSupportVita()
{
	stopThreads();
	deleteCriticalSection(m_cs);
	m_cs = 0;
}

void btThreadSupportVita::runTask(int threadIndex, void* userData)
{
	btThreadStatus& threadStatus = m_activeThreadStatus[threadIndex];
	btAssert(threadIndex >= 0);
	btAssert(threadIndex < m_activeThreadStatus.size());
	threadStatus.m_cs = m_cs;
	threadStatus.m_commandId = 1;
	threadStatus.m_status = 1;
	threadStatus.m_userPtr = userData;
	m_startedThreadsMask |= UINT64(1) << threadIndex;

	sceKernelSignalSema(threadStatus.startSemaphore, 1);
}

int btThreadSupportVita::waitForResponse()
{
	btAssert(m_activeThreadStatus.size());

	sceKernelWaitSema(m_mainSemaphore, 1, nullptr);
	
	size_t last = -1;
	for (size_t t = 0; t < size_t(m_activeThreadStatus.size()); ++t)
	{
		m_cs->lock();
		bool hasFinished = (2 == m_activeThreadStatus[t].m_status);
		m_cs->unlock();
		if (hasFinished)
		{
			last = t;
			break;
		}
	}

	btThreadStatus& threadStatus = m_activeThreadStatus[last];
	btAssert(threadStatus.m_status > 1);
	threadStatus.m_status = 0;
	btAssert(last >= 0);
	m_startedThreadsMask &= ~(UINT64(1) << last);

	return last;
}

void btThreadSupportVita::waitForAllTasks()
{
	while (m_startedThreadsMask)
	{
		waitForResponse();
	}
}

void btThreadSupportVita::startThreads(const ConstructionInfo& threadConstructionInfo)
{
	btITaskScheduler* scheduler = btGetTaskScheduler();
	if (scheduler) {
		m_numThreads = scheduler->getNumThreads();
		printf("[btThreadSupportVita] Creating %d worker thread(s) (from scheduler)\n", m_numThreads);
	} else {
		m_numThreads = g_desiredVitaThreadCount;
		printf("[btThreadSupportVita] Creating %d worker thread(s) (from static variable, scheduler not set yet)\n", m_numThreads);
	}
	
	m_activeThreadStatus.resize(m_numThreads);
	m_startedThreadsMask = 0;

	m_mainSemaphore = createVitaSem("BulletMainSem", 0);
	if (m_mainSemaphore < 0) {
		printf("Failed to create main semaphore\n");
		return;
	}

	for (int i = 0; i < m_numThreads; i++)
	{
		btThreadStatus& threadStatus = m_activeThreadStatus[i];
		threadStatus.startSemaphore = createVitaSem("BulletThreadSem", 0);
		if (threadStatus.startSemaphore < 0) {
			printf("Failed to create thread semaphore %d\n", i);
			continue;
		}

		threadStatus.m_userPtr = 0;
		threadStatus.m_cs = m_cs;
		threadStatus.m_taskId = i;
		threadStatus.m_commandId = 0;
		threadStatus.m_status = 0;
		threadStatus.m_mainSemaphore = m_mainSemaphore;
		threadStatus.m_userThreadFunc = threadConstructionInfo.m_userThreadFunc;
		threadStatus.threadUsed = 0;

		char threadName[32];
		snprintf(threadName, 32, "BulletThread%d", i);
		
		threadStatus.threadId = sceKernelCreateThread(
			threadName,
			vitaThreadFunction,
			0x10000100,
			512 * 1024,
			0,
			0,
			nullptr
		);

		if (threadStatus.threadId >= 0) {
			void* statusPtr = &threadStatus;
			int result = sceKernelStartThread(threadStatus.threadId, sizeof(void*), &statusPtr);
			if (result < 0) {
				printf("Failed to start thread %d: 0x%x\n", i, result);
				sceKernelDeleteThread(threadStatus.threadId);
				threadStatus.threadId = -1;
			}
		} else {
			printf("Failed to create thread %d: 0x%x\n", i, threadStatus.threadId);
		}
	}
}

void btThreadSupportVita::stopThreads()
{
	for (size_t t = 0; t < size_t(m_activeThreadStatus.size()); ++t)
	{
		btThreadStatus& threadStatus = m_activeThreadStatus[t];

		if (threadStatus.threadId >= 0) {
			threadStatus.m_userPtr = 0;
			sceKernelSignalSema(threadStatus.startSemaphore, 1);
			sceKernelWaitSema(m_mainSemaphore, 1, nullptr);

			int stat = 0;
			sceKernelWaitThreadEnd(threadStatus.threadId, &stat, nullptr);
			sceKernelDeleteThread(threadStatus.threadId);
		}

		destroyVitaSem(threadStatus.startSemaphore);
	}
	
	destroyVitaSem(m_mainSemaphore);
	m_activeThreadStatus.clear();
}

btCriticalSection* btThreadSupportVita::createCriticalSection()
{
	return new btCriticalSectionVita("BulletCS");
}

void btThreadSupportVita::deleteCriticalSection(btCriticalSection* criticalSection)
{
	delete criticalSection;
}

btThreadSupportInterface* btThreadSupportInterface::create(const ConstructionInfo& info)
{
	return new btThreadSupportVita(info);
}

#endif // BT_THREADSAFE && defined(VITA_BUILD)

