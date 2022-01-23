#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include "MinHook.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define DEVELOPER_MODE

#define VECTOR_MIN 16

typedef struct {
	unsigned int first;
	unsigned int second;
} Pair;

typedef struct {
	size_t nelems;
	size_t melems;
	Pair * data;
} Vector;

int Vector_init(Vector * vect) {
	vect->nelems = 0;
	vect->melems = VECTOR_MIN;
	vect->data = malloc(vect->melems * sizeof(Pair));
	if (vect->data == NULL) {
		printf("malloc failed in Vector_init\n");
		return 1;
	}
	return 0;
}

int Vector_append(Vector * vect, Pair elem) {
	if (vect->nelems == vect->melems) {
		if ((vect->melems * sizeof(Pair)) <= (SIZE_MAX >> 1)) {
			vect->melems <<= 1;
		} else {
			return 1;
		}
		
		Pair * newData = realloc(vect->data, vect->melems * sizeof(Pair));
		if (newData == NULL) {
			return 1;
		}
		vect->data = newData;
	}
	vect->data[vect->nelems++] = elem;
	return 0;
}

void Vector_destroy(Vector * vect) {
	free(vect->data);
}

void toLower(char * str) {
	while (*str != '\0') {
		*str = (char)tolower(*str);
		str++;
	}
}

int (* origCommandHandler)					(char*);
int (* const origCommandHandlerLocation)	(char*) 											= (	int(*)	(char*))											(0x401970);

int (* originalMain)						(int, char* []);
int (* const originalMainLocation)			(int, char* [])										= (	int(*)	(int, char* []))									(0x402900);

int (* const __BBC_CheckHandle)				(int) 												= (	int(*)	(int))												(0x403A20);
int (* const __BBC_ObjectSize)				(int, const char*) 									= (	int(*)	(int, const char*))									(0x407460);
int (* const __BBC_GetObject)				(int, const char*, void*, int) 						= (	int(*)	(int, const char*, void*, int))						(0x4074D0);
int (* const __BBC_StoreObject)				(int, const char*, const char*, const void*, int) 	= (	int(*)	(int, const char*, const char*, const void*, int))	(0x407550);
int (* const __BBC_FReadDir)				(void*, void*, unsigned int) 						= (	int(*)	(void*, void*, unsigned int))						(0x4096B0);
int (* const __BBC_FRename)					(void*, const char*, const char*)					= (	int(*)	(void*, const char*, const char*))					(0x409780);
int (* const __BBC_FDelete)					(void*, char*)  									= (	int(*)	(void*, char*))										(0x40A130);
int (* const __bbc_direct_read_blocks)		(void*, unsigned int, int, void*, void*) 			= (	int(*)	(void*, unsigned int, int, void*, void*))			(0x40AD80);
int (* const __bbc_direct_write_blocks)		(void*, unsigned int, int, const void*, void*) 		= (	int(*)	(void*, unsigned int, int, const void*, void*))		(0x40B050);

int * const diag_handle 																		= (	int*	)													 0x40E198;
void* * const handlesBase 																		= (	void**	)													 0x4326C0;

int CmdDumpNandRaw()
{
	int res = __BBC_CheckHandle(*diag_handle);
	if (res)
	{
		printf("__BBC_CheckHandle failed, did you init with BBCInit (B)?\n");
		return 1;
	}

	int* handle2 = (int*)(handlesBase[*diag_handle]);

	void** direct_ptrs = *(void***)(handle2 + 0x110);
	
	FILE* nand;
	FILE* spare;
	if ((res = fopen_s(&nand, "nand.bin", "wb")) != 0)
	{
		printf("error: failed to open nand.bin for writing: %d\n", res);
		return 1;
	}
	if ((res = fopen_s(&spare, "spare.bin", "wb")) != 0)
	{
		printf("error: failed to open spare.bin for writing: %d\n", res);
		return 1;
	}

	printf("reading nand.bin/spare.bin from device...\n");

	int numBlocks = *(int*)(handle2 + 0xB);
	if (numBlocks < 0) {
		return 1;
	}

	unsigned char* buff = malloc(0x4000);
	if (buff == NULL) {
		return 1;
	}
	unsigned char* sparebuff = malloc(0x10);
	if (sparebuff == NULL) {
		free(buff);
		return 1;
	}

	for (unsigned int i = 0; i < (unsigned int)numBlocks; i++)
	{
		__bbc_direct_read_blocks(direct_ptrs[0], i, 1, buff, sparebuff);
		fwrite(buff, 1, 0x4000, nand);
		fwrite(sparebuff, 1, 0x10, spare);

		fflush(nand);
		fflush(spare);

		if (i % 0x10 == 0) // progress update every 16 blocks
		{
			float progress = ((float)i / (float)numBlocks) * 100.f;
			printf("%d/%d blocks read, %0.2f %%\n", i, numBlocks, progress);
		}
	}

	fclose(nand);
	fclose(spare);

	free(buff);
	free(sparebuff);

	printf("dump complete!\n");

	return 0;
}

int CmdWriteNandRaw(char* args)
{
	int res = __BBC_CheckHandle(*diag_handle);
	if (res)
	{
		printf("__BBC_CheckHandle failed, did you init with BBCInit (B)?\n");
		return 1;
	}

	int* handle2 = (int*)(handlesBase[*diag_handle]);

	void** direct_ptrs = *(void***)(handle2 + 0x110);

	FILE* nand;
	FILE* spare;
	if ((res = fopen_s(&nand, "nand.bin", "rb")) != 0)
	{
		printf("error: failed to open nand.bin for reading: %d\n", res);
		return 1;
	}
	if ((res = fopen_s(&spare, "spare.bin", "rb")) != 0)
	{
		printf("error: failed to open spare.bin for reading: %d\n", res);
		return 1;
	}
	
	int _numBlocks = *(int*)(handle2 + 0xB);
	if (_numBlocks < 0) {
		return 1;
	}
	unsigned int numBlocks = (unsigned int)_numBlocks;
	
	// nand.bin size check
	fseek(nand, 0, SEEK_END);
	long size = ftell(nand);
	fseek(nand, 0, SEEK_SET);
	if (size != 16 * 1024 * (long)numBlocks)
	{
		printf("error: nand.bin size %ld, expected %d\n", size, 16 * 1024 * numBlocks);
		return 0;
	}

	// spare.bin size check
	fseek(spare, 0, SEEK_END);
	size = ftell(spare);
	fseek(spare, 0, SEEK_SET);
	if (size != 16 * (long)numBlocks)
	{
		printf("error: spare.bin size %ld, expected %d\n", size, 16 * numBlocks);
		return 0;
	}

	printf("write nand.bin/spare.bin to the device? (y/n)\n");
	int answer = getchar();
	if (answer != 'Y' && answer != 'y')
	{
		fclose(nand);
		fclose(spare);
		printf("write aborted\n");
		return 0;
	}

	printf("writing nand.bin/spare.bin...\n");

	unsigned char* buff = malloc(0x4000);
	if (buff == NULL) {
		fclose(nand);
		fclose(spare);
		return 1;
	}
	unsigned char* sparebuff = malloc(0x10);
	if (sparebuff == NULL) {
		free(buff);
		fclose(nand);
		fclose(spare);
		return 1;
	}
	if (args[0] == ' ')
		args++;

	// if we've been given ranges, deserialize them and go through them
	if (strlen(args) > 0)
	{
		Vector ranges;
		Vector_init(&ranges);
		
		int numBlocksToWrite = 0;

		char* curPtr = args;
		char* endPtr = args + strlen(args);
		while (curPtr < endPtr)
		{
			char cur[256];
			memset(cur, 0, 256);

			size_t splIdx = strcspn(curPtr, ",");
			memcpy(cur, curPtr, splIdx);
			curPtr += (splIdx + 1);

			char start[256];
			char end[256];
			memset(start, 0, 256);
			memset(end, 0, 256);
			strcpy_s(start, _countof(start), "0x0");
			sprintf_s(end, _countof(end), "0x%x", numBlocks);

			if (cur[0] == '-')
				strcpy_s(end, _countof(end), cur + 1); // range is only giving the end block
			else if (cur[strlen(cur) - 1] == '-')
				memcpy(start, cur, strlen(cur) - 1); // range is only giving the start block
			else
			{
				splIdx = strcspn(cur, "-");
				if (splIdx == strlen(cur)) // theres no "-", so just a single block specified
				{
					strcpy_s(start, _countof(start), cur);
					long num = strtol(start, NULL, 0);
					if (num > UINT_MAX || num < 0) {
						return 1;
					}
					sprintf_s(end, _countof(end), "0x%x", (unsigned int)num + 1);
				}
				else
				{
					// is specifying both start block and end block
					memcpy(start, cur, splIdx);
					strcpy_s(end, _countof(end), cur + splIdx + 1);
				}
			}

			long startNum = strtol(start, NULL, 0);
			long endNum = strtol(end, NULL, 0);			
			numBlocksToWrite += (endNum - startNum);
			
			if (startNum > UINT_MAX || startNum < 0 || endNum > UINT_MAX || endNum < 0) {
				printf("invalid start/end\n");
				return 1;
			}
			
			Vector_append(&ranges, (Pair){ (unsigned int)startNum, (unsigned int)endNum });
		}

		int numBlocksWritten = 0;

		for(size_t range = 0; range < ranges.nelems; range++)
		{
			for (unsigned int i = ranges.data[range].first; i < ranges.data[range].second; i++)
			{
				// go to correct offset in nand/spare
				fseek(nand, (long)i * 0x4000, SEEK_SET);
				fseek(spare, (long)i * 0x10, SEEK_SET);

				fread(buff, 1, 0x4000, nand);
				fread(sparebuff, 1, 0x10, spare);

				if (sparebuff[5] != 0xFF)
					continue; // skip trying to write bad blocks

				// when writing spare, only first 3 bytes (SA block info) need to be populated, rest can be all 0xFF
				for (int j = 3; j < 0x10; j++)
					sparebuff[j] = 0xFF;

				__bbc_direct_write_blocks(direct_ptrs[0], i, 1, buff, sparebuff);

				numBlocksWritten++;

				if (numBlocksWritten % 0x10 == 0) // progress update every 16 blocks
				{
					float progress = ((float)numBlocksWritten / (float)numBlocksToWrite) * 100.f;
					printf("%d/%d blocks written, %0.2f %%\n", numBlocksWritten, numBlocksToWrite, progress);
				}
			}
		}
		
		Vector_destroy(&ranges);
	}
	else
	{
		// haven't been given any ranges to write, so just write the file sequentially
		for (unsigned int i = 0; i < numBlocks; i++)
		{
			fread(buff, 1, 0x4000, nand);
			fread(sparebuff, 1, 0x10, spare);

			if (sparebuff[5] != 0xFF)
				continue; // skip trying to write bad blocks

			// when writing spare, only first 3 bytes (SA block info) need to be populated, rest can be all 0xFF
			for (int j = 3; j < 0x10; j++)
				sparebuff[j] = 0xFF;

			__bbc_direct_write_blocks(direct_ptrs[0], i, 1, buff, sparebuff);

			if (i % 0x10 == 0) // progress update every 16 blocks
			{
				float progress = ((float)i / (float)numBlocks) * 100.f;
				printf("%d/%d blocks written, %0.2f %%\n", i, numBlocks, progress);
			}
		}
	}

	fclose(nand);
	fclose(spare);

	free(buff);
	free(sparebuff);

	printf("write complete!\n");

	return 0;
}

int CmdDumpFile(char *str)
{
	int res = __BBC_CheckHandle(*diag_handle);
	if (res)
	{
		printf("__BBC_CheckHandle failed, did you init with BBCInit (B)?\n");
		return 0;
	}

	char filename[256];
	if (sscanf_s(str, "%255s", &filename, _countof(filename)) >= 1)
	{
		int fileSize = __BBC_ObjectSize(*diag_handle, filename);
		printf("fileSize = %08X (%d) bytes\n", fileSize, fileSize);
		if (fileSize > 0)
		{
			unsigned char *buffer = malloc((size_t)fileSize);
			if (buffer == NULL) {
				return 1;
			}

			printf("__BBC_GetObject started\n");
			__BBC_GetObject(*diag_handle, filename, buffer, fileSize);
			printf("__BBC_GetObject finished\n");

			FILE *f;
			if (fopen_s(&f, filename, "wb")) {
				return 1;
			}
			fwrite(buffer, 1, (size_t)fileSize, f);
			fclose(f);

		}
	}

	return 0;
}

int CmdWriteFile(char *str)
{
	int res = __BBC_CheckHandle(*diag_handle);
	if (res)
	{
		printf("__BBC_CheckHandle failed, did you init with BBCInit (B)?\n");
		return 1;
	}

	char filename[256] = { 0 };
	if (sscanf_s(str, "%255s", &filename, _countof(filename)) >= 1)
	{
		FILE *f;
		if (fopen_s(&f, filename, "rb")) {
			return 1;
		}

		if (f)
		{
			fseek(f, 0, SEEK_END);
			long int _fileSize = ftell(f);
			fseek(f, 0, SEEK_SET);
			
			if (_fileSize > INT_MAX) {
				return 1;
			}
			
			int fileSize = (int)_fileSize;

			printf("__BBC_StoreObject fileSize = %d bytes\n", fileSize);

			unsigned char *buffer = malloc((size_t)fileSize);
			if (buffer == NULL) {
				fclose(f);
				return 1;
			}
			fread(buffer, 1, (size_t)fileSize, f);
			fclose(f);

			printf("__BBC_StoreObject started\n");
			toLower(filename);
			__BBC_StoreObject(*diag_handle, filename, "temp.tmp", buffer, fileSize);
			printf("__BBC_StoreObject finished\n");

			free(buffer);
		}
		else
		{
			printf("%s was not found.\n", filename);
		}
	}

	return 0;
}

int CmdGetDirListing()
{
	int res = __BBC_CheckHandle(*diag_handle);
	if (res)
	{
		printf("__BBC_CheckHandle failed, did you init with BBCInit (B)?\n");
		return 0;
	}

	unsigned char *buffer = calloc(0x2008, sizeof(unsigned char));
	if (buffer)
	{
		printf("__BBC_FReadDir started\n");
		__BBC_FReadDir(handlesBase[*diag_handle], buffer, 410);
		printf("__BBC_FReadDir finished\n");

		int idx = 0;
		while (buffer[idx] != 0)
		{
			unsigned int tmp;
			memcpy(&tmp, &buffer[idx + 0x10], sizeof(tmp));

			printf("\"%s\" \t\t size = %d bytes\n", &buffer[idx], tmp);
			idx += 0x14;
		}

		//FILE *f = fopen("readdir.bin", "wb");
		//fwrite(buffer, 1, 0x2008, f);
		//fclose(f);
	}
	free(buffer);

	return 0;
}

int CmdDeleteFile(char *str)
{
	int res = __BBC_CheckHandle(*diag_handle);
	if (res)
	{
		printf("__BBC_CheckHandle failed, did you init with BBCInit (B)?\n");
		return 1;
	}

	char filename[256] = { 0 };
	if (sscanf_s(str, "%255s", &filename, _countof(filename)) >= 1)
	{
		printf("__BBC_FDelete \"%s\" started\n", filename);
		__BBC_FDelete(handlesBase[*diag_handle], filename);
		printf("__BBC_FDelete finished\n");
	}

	return 0;
}

int CmdRenameFile(char *str)
{
	int res = __BBC_CheckHandle(*diag_handle);
	if (res)
	{
		printf("__BBC_CheckHandle failed, did you init with BBCInit (B)?\n");
		return 1;
	}

	char old[256] = { 0 };
	char new[256] = { 0 };
	if (sscanf_s(str, "%255s %255s", &old, _countof(old), &new, _countof(new)) >= 1)
	{
		printf("__BBC_FRename \"%s\" to \"%s\" started\n", old, new);
		__BBC_FRename(handlesBase[*diag_handle], old, new);
		printf("__BBC_FRename finished\n");
	}

	return 0;
}

int __cdecl CommandHandlerHook(char* input)
{
	char inputChar = *input;

	switch (inputChar)
	{
	case 'X':
	case 'x':
		printf("\tiQueDiagExtend Commands:\n");
		printf("\t1\t   - reads nand from ique to nand.bin/spare.bin\n");
#ifdef DEVELOPER_MODE
		printf("\t2 <ranges> - writes nand from nand.bin/spare.bin to ique\n");
		printf("\tranges can optionally be specified, which might be quicker than writing the whole file\n");
		printf("\teg: \"2 0-0x100,4075\" writes blocks 0 - 0x100 (not including 0x100 itself), and block 4075\n");
		printf("\tmake sure hex block numbers are prefixed with '0x'!\n\n");
#endif
		printf("\t3 [file] - reads [file] from ique [to file]\n");
#ifdef DEVELOPER_MODE
		printf("\t4 [file] - write [file] to ique\n");
#endif
		printf("\t5 - list all files on ique\n");
#ifdef DEVELOPER_MODE
		printf("\t6 [file] - delete [file] from ique\n");
		printf("\t7 [old] [new] - rename [old] to [new]\n");
#endif
		break;
	case '1':
		return CmdDumpNandRaw();
		break;
#ifdef DEVELOPER_MODE
	case '2':
		return CmdWriteNandRaw(input + 1);
		break;
#endif
	case '3':
		return CmdDumpFile(input + 1);
		break;
#ifdef DEVELOPER_MODE
	case '4':
		return CmdWriteFile(input + 1);
		break;
#endif
	case '5':
		return CmdGetDirListing();
		break;
#ifdef DEVELOPER_MODE
	case '6':
		return CmdDeleteFile(input + 1);
		break;
	case '7':
		return CmdRenameFile(input + 1);
		break;
#endif
	default:
		return origCommandHandler(input);
	}

	return 0;
}

int mainHook(int argc, char * argv[]) {
	printf("iQueDiagExtend 0.5 by emoose, Normmatt and Jhynjhiruu\nHooks ");
	
	return originalMain(argc, argv);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	&hModule;
	&lpReserved;
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		MH_Initialize();
		
		MH_CreateHook((void*)origCommandHandlerLocation, (void*)CommandHandlerHook, (LPVOID*)&origCommandHandler);
		MH_EnableHook((void*)origCommandHandlerLocation);
		
		MH_CreateHook((void*)originalMainLocation, (void*)mainHook, (LPVOID*)&originalMain);
		MH_EnableHook((void*)originalMainLocation);
	}

	return TRUE;
}
