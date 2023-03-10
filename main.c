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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "clown68000/clown68000.h"

#include "thread.h"

typedef struct KatyState
{
	Clown68000_State m68k;
	cc_u8l rom[0x78000];
	cc_u8l ram[0x80000];
	char fifo[0x100];
	cc_u16f fifo_write;
	cc_u16f fifo_read;
	cc_bool breadboard_compatibility;
} KatyState;

static KatyState katy_state;

static Thread m68k_thread, timer_thread;
static Mutex mutex;

/* clown68000 callbacks */

static void ErrorCallback(const char* const format, va_list arg)
{
	vfprintf(stderr, format, arg);
	fputc('\n', stderr);
	fflush(stderr);
}

#define CALLBACK_ERROR(ACTION, LOCATION) fprintf(stderr, "[%08" CC_PRIXLEAST32 "] Attempted to " ACTION " " LOCATION " at address 0x%" CC_PRIXFAST32 "\n", state->m68k.program_counter, address);
#define READ_CALLBACK_ERROR(LOCATION) CALLBACK_ERROR("read from", LOCATION)
#define WRITE_CALLBACK_ERROR(LOCATION) CALLBACK_ERROR("write to", LOCATION)

static cc_u16f ReadCallback(const void* const user_data, const cc_u32f address, const cc_bool do_high_byte, const cc_bool do_low_byte)
{
	cc_u16f value;

	KatyState* const state = (KatyState*)user_data;

	value = 0;

	if ((address & 0x80000) != 0)
	{
		/* 0x80000 - 0xFFFFF : RAM */
		if (do_high_byte)
			value |= (state->ram[(address & 0x7FFFF) + 0] << 8);

		if (do_low_byte)
			value |= (state->ram[(address & 0x7FFFF) + 1] << 0);
	}
	else if ((address & 0x78000) != 0x78000)
	{
		/* 0x00000 - 0x77FFF : ROM */
		if (do_high_byte)
			value |= (state->rom[(address & 0x7FFFF) + 0] << 8);

		if (do_low_byte)
			value |= (state->rom[(address & 0x7FFFF) + 1] << 0);
	}
	else
	{
		/* 0x78000 - 0x7FFFF : IO */
		switch ((address / 0x2000) & 3)
		{
			case (0x78000 / 0x2000) & 3:
				/* 78000 - 79FFF : Serial in */
				if (do_high_byte)
				{
					value |= state->fifo[state->fifo_read] << 8;

					state->fifo_read = (state->fifo_read + 1) % CC_COUNT_OF(state->fifo);
				}

				if (do_low_byte)
				{
					value |= state->fifo[state->fifo_read] << 0;

					state->fifo_read = (state->fifo_read + 1) % CC_COUNT_OF(state->fifo);
				}

				break;

			case (0x7A000 / 0x2000) & 3:
				/* 7A000 - 7BFFF : Serial out */
				READ_CALLBACK_ERROR("serial out");
				break;

			case (0x7C000 / 0x2000) & 3:
			{
				const cc_bool rdf = state->fifo_write == state->fifo_read;
				const cc_bool txe = cc_false;

				if (state->breadboard_compatibility)
				{
					/* 7C000 - 7DFFF : Serial status RDF & TXE */
					const cc_u16f rdf_and_txe = (txe << 1) | (rdf << 0);

					value = (rdf_and_txe << 8) | (rdf_and_txe << 0);
				}
				else
				{
					if ((address & 0x1000) == 0)
					{
						/* 7C000 - 7CFFF : Serial status RDF */
						value = (rdf << 8) | (rdf << 0);
					}
					else
					{
						/* 7D000 - 7DFFF : Serial status TXE */
						value = (txe << 8) | (txe << 0);
					}
				}

				break;
			}

			case (0x7E000 / 0x2000) & 3:
				/* 7E000 - 7FFFF : LED register */
				READ_CALLBACK_ERROR("LED register");
				break;

			default:
				assert(cc_false);
				break;
		}
	}

	return value;
}

static void WriteCallback(const void* const user_data, const cc_u32f address, const cc_bool do_high_byte, const cc_bool do_low_byte, const cc_u16f value)
{
	KatyState* const state = (KatyState*)user_data;

	if ((address & 0x80000) != 0)
	{
		/* 0x80000 - 0xFFFFF : RAM */
		if (do_high_byte)
			state->ram[(address & 0x7FFFF) + 0] = value >> 8;

		if (do_low_byte)
			state->ram[(address & 0x7FFFF) + 1] = value & 0xFF;
	}
	else if ((address & 0x78000) != 0x78000)
	{
		/* 0x00000 - 0x77FFF : ROM */
		WRITE_CALLBACK_ERROR("ROM");
	}
	else
	{
		/* 0x78000 - 0x7FFFF : IO */
		switch ((address / 0x2000) & 3)
		{
			case (0x78000 / 0x2000) & 3:
				/* 78000 - 79FFF : Serial in */
				WRITE_CALLBACK_ERROR("serial in");
				break;

			case (0x7A000 / 0x2000) & 3:
				/* 7A000 - 7BFFF : Serial out */
				if (do_high_byte)
					fputc(value >> 8, stdout);

				if (do_low_byte)
					fputc(value & 0xFF, stdout);

				break;

			case (0x7C000 / 0x2000) & 3:
				if (state->breadboard_compatibility)
				{
					/* 7C000 - 7DFFF : Serial status RDF & TXE */
					WRITE_CALLBACK_ERROR("serial status RDF/TXE");
				}
				else
				{
					if ((address & 0x1000) == 0)
					{
						/* 7C000 - 7CFFF : Serial status RDF */
						WRITE_CALLBACK_ERROR("serial status RDF");
					}
					else
					{
						/* 7D000 - 7DFFF : Serial status TXE */
						WRITE_CALLBACK_ERROR("serial status TXE");
					}
				}

				break;

			case (0x7E000 / 0x2000) & 3:
				/* 7E000 - 7FFFF : LED register */
				break;

			default:
				assert(cc_false);
				break;
		}
	}
}

static void M68kThread(void* const user_data)
{
	/* This thread runs the 68k emulator. */
	Clown68000_ReadWriteCallbacks callbacks;

	KatyState* const state = (KatyState*)user_data;

	callbacks.read_callback = ReadCallback;
	callbacks.write_callback = WriteCallback;
	callbacks.user_data = state;

	Mutex_Lock(&mutex);
	Clown68000_SetErrorCallback(ErrorCallback);
	Clown68000_Reset(&state->m68k, &callbacks);
	Mutex_Unlock(&mutex);

	for (;;)
	{
		Mutex_Lock(&mutex);
		Clown68000_DoCycle(&state->m68k, &callbacks);
		Mutex_Unlock(&mutex);
	}
}

static void TimerThread(void* const user_data)
{
	/* This thread exists to interrupt the 68k. */
	Clown68000_ReadWriteCallbacks callbacks;

	KatyState* const state = (KatyState*)user_data;

	callbacks.read_callback = ReadCallback;
	callbacks.write_callback = WriteCallback;
	callbacks.user_data = state;

	for (;;)
	{
		Mutex_Lock(&mutex);

		/* I would use interrupt 7 here (which is supposed to combine the FIFO read with a timer update),
		   but it doesn't appear to work. The 68 Katy's schematic seems to explicitly prevent interrupt 7
		   from firing anyway. */
		if (state->fifo_write != state->fifo_read)
			Clown68000_Interrupt(&state->m68k, &callbacks, 2);

		Clown68000_Interrupt(&state->m68k, &callbacks, 5);

		Mutex_Unlock(&mutex);

		Thread_Sleep(1000 / 100); /* 100Hz */
	}
}

/* Raw console input stuff */

#ifdef _WIN32
 #include <conio.h>
#elif defined(__unix__)
 #include <unistd.h>

 #if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L
  #define POSIX

  #include <string.h>
  #include <sys/select.h>
  #include <termios.h>
  #include <unistd.h>

  static struct termios original_terminal_attributes;

static void DisableRawConsoleInput(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &original_terminal_attributes);
}
 #endif
#endif

int main(const int argc, char** const argv)
{
	int exit_code;

	exit_code = EXIT_FAILURE;

#ifdef POSIX
	/* Disable STDIN's line buffering, allowing inputs to be fed directly to the serial port. */
	{
	struct termios new_terminal_attributes;

	tcgetattr(STDIN_FILENO, &original_terminal_attributes);

	atexit(DisableRawConsoleInput);

	new_terminal_attributes = original_terminal_attributes;

	new_terminal_attributes.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	new_terminal_attributes.c_oflag &= ~OPOST;
	new_terminal_attributes.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	new_terminal_attributes.c_cflag &= ~(CSIZE | PARENB);
	new_terminal_attributes.c_cflag |= CS8;

	tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal_attributes);
	}
#endif

	if (argc < 2)
	{
		fputs("Usage: [executable] [path to firmware] [options]\n"
		      "\n"
		      "Options:\n"
		      "  '-b' - Emulate a breadboard 68 Katy instead of a PCB 68 Katy.\n", stderr);
	}
	else
	{
		FILE* const file = fopen(argv[1], "rb");

		if (file == NULL)
		{
			fputs("Could not read firmware file.\n", stderr);
		}
		else
		{
			int last_character = 0;

			fread(katy_state.rom, 1, sizeof(katy_state.rom), file);
			fclose(file);

			katy_state.breadboard_compatibility = argc > 2 && argv[2][0] == '-' && argv[2][1] == 'b' && argv[2][2] == '\0';

			/* Set-up threads. */
			Mutex_Create(&mutex);
			Thread_Create(&m68k_thread, M68kThread, &katy_state);
			Thread_Create(&timer_thread, TimerThread, &katy_state);

			/* Infinitely grab input, feeding it to the serial port FIFO. */
			for (;;)
			{
			#ifdef _WIN32
				const int character = _getch();
			#elif defined(POSIX)
				const int character = getchar();
			#else
				#error "Add your platform's non-line-buffered character-getting here!"
			#endif

				/* Exit program upon pressing ESC twice. */
				if (character == 0x1B && last_character == 0x1B)
					break;

				last_character = character;

				katy_state.fifo[katy_state.fifo_write % CC_COUNT_OF(katy_state.fifo)] = character;
				katy_state.fifo_write = (katy_state.fifo_write + 1) % CC_COUNT_OF(katy_state.fifo);
			}

			/* Destroy threads. */
			Thread_Destroy(&m68k_thread);
			Thread_Destroy(&timer_thread);
			Mutex_Destroy(&mutex);

			exit_code = EXIT_SUCCESS;
		}
	}

	return exit_code;
}
