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

#ifndef THREAD_H
#define THREAD_H

#ifdef _WIN32

 #include <windows.h>

 typedef HANDLE Mutex;
 typedef HANDLE Thread;

#elif defined(__unix__)

 #include <unistd.h>

 #if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L
  #include <pthread.h>

  typedef pthread_mutex_t Mutex;
  typedef pthread_t Thread;
 #endif

#else

 #error "Define your platforms's mutex and thread types here!"

#endif

void Mutex_Create(Mutex *mutex);
void Mutex_Destroy(Mutex *mutex);
void Mutex_Lock(Mutex *mutex);
void Mutex_Unlock(Mutex *mutex);

void Thread_Create(Thread *thread, void (*function)(void *user_data), const void *user_data);
void Thread_Destroy(Thread *thread);
void Thread_Sleep(unsigned int milliseconds);

#endif  /* THREAD_H */
