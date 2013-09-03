#include <stdio.h>
#include <string.h>
#include "windows.h"
#include "process.h"

unsigned int word_len = 4;

char * word = "coat";
unsigned long original_crc = 0x0DA77D88;

unsigned long crc_table[256];
unsigned long starting_crc = 0x0UL; //0xFFFFFFFFUL;
char stopped;
CRITICAL_SECTION mutex_CHANGE_CRC;



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

unsigned int WINAPI brute_crc (void *arg) {
	
	EnterCriticalSection(&mutex_CHANGE_CRC);
	while ((!stopped) && (starting_crc < 0xFFFFFFFFUL)) {
		unsigned long local_crc_start = starting_crc;
		LeaveCriticalSection(&mutex_CHANGE_CRC);

		unsigned long local_crc = CRC32_function((unsigned char *)word,word_len,local_crc_start);

		if ((local_crc == original_crc)||(local_crc_start >= 0xFFFFFFFFUL)) {
			stopped = 1;
		}

		EnterCriticalSection(&mutex_CHANGE_CRC);
		starting_crc++;
	}
	LeaveCriticalSection(&mutex_CHANGE_CRC);
	return 0;
}


int main(void) {
	generateTable();

	InitializeCriticalSection(&mutex_CHANGE_CRC);
	unsigned int *tids = new unsigned int[2];
	HANDLE *threadsHandles = new HANDLE[2];

	stopped = 0;

	threadsHandles[0] = (HANDLE) _beginthreadex(NULL,0,brute_crc,NULL,0,&tids[0]);
	threadsHandles[1] = (HANDLE) _beginthreadex(NULL,0,brute_crc,NULL,0,&tids[1]);
	
	while (!stopped) {
		Sleep(1000);
		printf("%08x \n",starting_crc);
	}

	WaitForMultipleObjects(2,threadsHandles, true, INFINITE);
	CloseHandle(threadsHandles[0]);
	CloseHandle(threadsHandles[1]);
	DeleteCriticalSection(&mutex_CHANGE_CRC);

	printf("word %s \n",word);
	printf("original crc %08x \n",original_crc);
	printf("start crc %08x \n",starting_crc);

	return 0;
}