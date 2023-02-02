/*
    Virtual 68 Katy - A 68 Katy emulator.
    Copyright (C) 2023  Clownacy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "thread.h"

#include <stddef.h>
#include <stdlib.h>

typedef struct ThreadArguments
{
	void (*function)(void *user_data);
	void *user_data;
} ThreadArguments;

static void Thread_FunctionWrapperCommon(void* const user_data)
{
	const ThreadArguments thread_arguments = *(ThreadArguments*)user_data;

	free(user_data);

	thread_arguments.function(thread_arguments.user_data);
}

static ThreadArguments* Thread_CreateCommon(void (* const function)(void *user_data), const void* const user_data)
{
	ThreadArguments* const thread_arguments = (ThreadArguments*)malloc(sizeof(ThreadArguments));

	if (thread_arguments != NULL)
	{
		thread_arguments->function = function;
		thread_arguments->user_data = (void*)user_data;
	}

	return thread_arguments;
}

#ifdef _WIN32

#include <windows.h>

void Mutex_Create(Mutex* const mutex)
{
	*mutex = CreateMutex(NULL, FALSE, NULL);
}

void Mutex_Destroy(Mutex* const mutex)
{
	CloseHandle(*mutex);
}

void Mutex_Lock(Mutex* const mutex)
{
	WaitForSingleObject(*mutex, INFINITE);
}

void Mutex_Unlock(Mutex* const mutex)
{
	ReleaseMutex(*mutex);
}

static DWORD WINAPI Thread_FunctionWrapper(LPVOID user_data)
{
	Thread_FunctionWrapperCommon(user_data);
	return 0;
}

void Thread_Create(Thread* const thread, void (* const function)(void *user_data), const void* const user_data)
{
	ThreadArguments* const thread_arguments = Thread_CreateCommon(function, user_data);

	if (thread_arguments != NULL)
		*thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Thread_FunctionWrapper, thread_arguments, 0, NULL);
}

void Thread_Destroy(Thread* const thread)
{
	CloseHandle(*thread);
}

void Thread_Sleep(const unsigned int milliseconds)
{
	Sleep(milliseconds);
}

#elif _POSIX_VERSION >= 200112L

#include <pthread.h>
#include <unistd.h>

void CreateMutex(Mutex* const mutex)
{
	pthread_mutex_init(mutex, NULL);
}

void DestroyMutex(Mutex* const mutex)
{
	pthread_mutex_destroy(mutex);
}

void LockMutex(Mutex* const mutex)
{
	pthread_mutex_lock(mutex);
}

void UnlockMutex(Mutex* const mutex)
{
	pthread_mutex_unlock(mutex);
}

static void* Thread_FunctionWrapper(void* const user_data)
{
	Thread_FunctionWrapperCommon(user_data);
	return NULL;
}

void Thread_Create(Thread* const thread, void (* const function)(void *user_data), const void* const user_data)
{
	ThreadArguments* const thread_arguments = Thread_CreateCommon(function, user_data);

	if (thread_arguments != NULL)
		pthread_create(thread, NULL, Thread_FunctionWrapper, thread_arguments);
}

void Thread_Destroy(Thread* const thread)
{
	pthread_cancel(*thread);
}

void Thread_Sleep(const unsigned int milliseconds)
{
	usleep((useconds_t)1000 * milliseconds);
}

#else

#error "Define your platforms's mutex and thread functions here!"

#endif
