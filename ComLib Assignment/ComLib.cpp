#include "ComLib.h"

ComLib::ComLib(const std::string& fileMapName, const size_t& buffSize, TYPE type) {
	m_type = type;

	mSize = buffSize << 20; //Converts from megabytes to bytes

	hFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,			//Memory not associated with an existing file
		NULL,							//Default
		PAGE_READWRITE,					//Read/write access to PageFile
		(DWORD)0,
		mSize + sizeof(size_t) * 2,		//Max size of object (added space for the head and tail pointers)
		(LPCWSTR)fileMapName.c_str());	//Name of shared memory

	if (hFileMap == NULL) {
		printf("Error! \n");
		exit(EXIT_FAILURE);
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		//printf("The file mapping already exists! \n"); //Debug
	}

	mData = MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, mSize); //mData always points to beginning of data
	cBuffer = (char*)mData;	//Buffer is cast to char* and initialized to beginning of data

	head = (size_t*)cBuffer;		//Initialize head, first in the buffer
	tail = ((size_t*)cBuffer + 1);	//Initialize tail, 8 bytes forward in the buffer

	if (type == PRODUCER) {
		//The head and the tail are stored first in memory and therefore this offset has to be considered when initialized
		*head = sizeof(size_t) * 2;
		*tail = sizeof(size_t) * 2;
	}
}

ComLib::~ComLib() {
	ReleaseMutex(hMutex);
	UnmapViewOfFile((LPCVOID)mData);
	CloseHandle(hFileMap);
}

bool ComLib::send(const void* msg, const size_t length) {
	if (*tail >= *head && length > 0) {
		WaitForSingleObject(hMutex, INFINITE); //Lock mutex

		size_t msgSize = length + sizeof(Header);

		size_t padding = 0;
		if (msgSize % 64 != 0) {
			size_t multiplier = (size_t)ceil((msgSize / 64.0f)); //See how many times it can be divided by 64, rounds upwards so it'll always be at least 1
			padding = (64 * multiplier) - msgSize;
			msgSize += padding;
		}

		if (mSize - *head < msgSize) { //If there is no space left for the message in the buffer
			Header header = { length }; //We still need to leave a header
			memcpy(cBuffer + *head, &header, sizeof(Header));

			*head = sizeof(size_t) * 2; //Then reset the pointer to the beginning of memory and move on to send the message
		}

		Header header = { length }; //Save neccessary information for the consumer into a header
		memcpy(cBuffer + *head, &header, sizeof(Header));

		memcpy(cBuffer + *head + sizeof(Header), msg, length);

		*head += msgSize;

		//if (*head >= mSize) {
		//	*head -= mSize;
		//}

		//std::cout << *head << std::endl; //Debug
		ReleaseMutex(hMutex);
		return true;
	}

	return false;
}

bool ComLib::recv(char* msg, size_t& length) {
	if (*tail != *head && length > 0) {
		WaitForSingleObject(hMutex, INFINITE); //Lock mutex

		size_t msgSize = length + sizeof(Header);

		size_t padding = 0;
		if (msgSize % 64 != 0) {
			size_t multiplier = (size_t)ceil((msgSize / 64.0f)); //See how many times it can be divided by 64, rounds upwards so it'll always be at least 1
			padding = (64 * multiplier) - msgSize;
			msgSize += padding;
		}

		if (mSize - *tail < msgSize) {
			*tail = sizeof(size_t) * 2; //Reset the pointer to the beginning of memory
		}

		memcpy(msg, cBuffer + *tail + sizeof(Header), length);

		*tail += msgSize;

		//if (*tail >= mSize) {
		//	*tail -= mSize;
		//}

		ReleaseMutex(hMutex);
		return true;
	}

	return false;
}

size_t ComLib::nextSize() {
	if (*tail != *head) {

		Header* header = (Header*)(cBuffer + *tail);

		return header->msgSize;
	}
	else {
		return 0;
	}
}

size_t ComLib::getSizeBytes() const {
	return this->mSize;
}

void ComLib::updateFreeMemory() {
	//if (*head > *tail) {
	//	freeMemory = mSize - 
	//}
	//else if(*head < *tail){

	//}
	//else if (*head == *tail) { //The buffer is empty (or full?)
	//	freeMemory = mSize;
	//}
}
