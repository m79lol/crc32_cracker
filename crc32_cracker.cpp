#include <stdio.h>
#include <string.h>

#ifdef _WIN32
	#include "windows.h"
	#include "process.h"

	typedef unsigned int THREAD_HANDLE;
	
	#define DEFINE_ATOM(ATOM_NAME) CRITICAL_SECTION ATOM_NAME;
	#define DEFINE_THREAD_PROCEDURE(PROC_NAME) unsigned int WINAPI PROC_NAME( void* arg )
	#define ATOM_LOCK(ATOM_NAME) EnterCriticalSection( &ATOM_NAME )
	#define ATOM_UNLOCK(ATOM_NAME) LeaveCriticalSection( &ATOM_NAME )

	#define START_THREAD(PROC_NAME,PARAM,THREAD_ID,INDEX) \
		threadsHandles[INDEX] = (HANDLE) _beginthreadex(NULL,0,PROC_NAME,PARAM,0,&THREAD_ID)
#else
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <stdarg.h>
	#include <errno.h>
	#include <pthread.h>
	#include <dlfcn.h>
	#include <unistd.h>

	typedef pthread_t THREAD_HANDLE;

	#define DEFINE_ATOM(ATOM_NAME)   pthread_mutex_t ATOM_NAME = PTHREAD_MUTEX_INITIALIZER;
	#define DEFINE_THREAD_PROCEDURE(PROC_NAME) void * PROC_NAME(void *arg)
	#define ATOM_LOCK(ATOM_NAME) pthread_mutex_lock( &ATOM_NAME )
	#define ATOM_UNLOCK(ATOM_NAME) pthread_mutex_unlock( &ATOM_NAME )
	#define START_THREAD(PROC_NAME,PARAM,THREAD_ID,INDEX) \
		pthread_create(&THREAD_ID,NULL,&PROC_NAME,PARAM)
#endif

char *word;
unsigned int word_len;
unsigned long original_crc;

unsigned long crc_table[256];
unsigned long starting_crc = 0x0UL; //0xFFFFFFFFUL;

char stopped = 0;
DEFINE_ATOM(mutex_CHANGE_CRC)

void generateTable() {
	unsigned long crc;
	for (int i = 0; i < 256; i++) {
		crc = i;
		for (int j = 0; j < 8; j++)
			crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
		crc_table[i] = crc;
	};
}

unsigned int CRC32_function(unsigned char *buf, unsigned long len, unsigned long in_starting_crc)
{
	unsigned long crc = in_starting_crc;
	while (len--)
		crc = crc_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
	return crc ^ 0xFFFFFFFFUL;
}

void printHelp(void) {
	printf("crc32_cracker <options> <crc32_result> <plain_text>\n");
	printf("-- options:\n");
	printf("     -t - count threads. default: 1\n");
	printf("\n");
	printf("-- <crc32_result> must be in hexademical format - 0xXXXXXXXX\n");
}


DEFINE_THREAD_PROCEDURE(brute_crc) {	
		
	ATOM_LOCK(mutex_CHANGE_CRC);
	while ((!stopped) && (starting_crc < 0xFFFFFFFFUL)) {
		unsigned long local_crc_start = starting_crc;
		ATOM_UNLOCK(mutex_CHANGE_CRC);

		unsigned long local_crc = CRC32_function((unsigned char *)word,word_len,local_crc_start);

		if ((local_crc == original_crc)||(local_crc_start >= 0xFFFFFFFFUL)) {
			ATOM_LOCK(mutex_CHANGE_CRC);
			stopped = 1;
		} else {
			ATOM_LOCK(mutex_CHANGE_CRC);
		}
		
		starting_crc++;
	}
	ATOM_UNLOCK(mutex_CHANGE_CRC);
	return 0;
}

int main(int argc, char *argv[]) {
	unsigned char index;
	unsigned int threadCount = 1;
	
	try {
		if (argc == 3) {
			index = 1;
		} else if (argc == 5) {
			index = 3;

			if (!strcmp(argv[1],"-t")) {
				sscanf(argv[2],"%d",&threadCount);
			} else {
				throw 1;
			}
		} else {
			throw 2;
		}
	
		if (strlen(argv[index]) == 10) {
			if (strcmp(argv[index],"0x") < 0) {
				throw 3;
			}
		} else {
			throw 4;
		}
	} catch(...) {
		printHelp();
		return 1;
	}

	sscanf(argv[index],"%i",&original_crc);
	index++;

	word = argv[index];
	word_len = strlen(word);

	generateTable();

#ifdef _WIN32
	InitializeCriticalSection(&mutex_CHANGE_CRC);
	HANDLE *threadsHandles = new HANDLE[threadCount];
#endif

	THREAD_HANDLE *tids = new unsigned int[threadCount];
	stopped = 0;

	for (unsigned int i = 0; i < threadCount; i++) {
		START_THREAD(brute_crc,NULL,tids[i],i);
		
		//threadsHandles[i] = (HANDLE) _beginthreadex(NULL,0,brute_crc,NULL,0,&tids[i]);
	}
	
#ifdef _WIN32
	WaitForMultipleObjects(threadCount,threadsHandles, true, INFINITE);
	for (unsigned int i = 0; i < threadCount; i++) {
		CloseHandle(threadsHandles[i]);
	}
	delete [] threadsHandles;

	DeleteCriticalSection(&mutex_CHANGE_CRC);
#else
	for (unsigned int i = 0; i < threadCount; i++) {
		pthread_join(tids[i],NULL);
	}
#endif

	delete [] tids;

	printf("0x%08x\n",starting_crc);
	return 0;
}