#include "ComLib.h"

ComLib::ComLib(const std::string& fileMapName, const size_t& buffSize, TYPE type) {
	mType = type;

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
	mCircBuffer = (char*)mData;	//Buffer is cast to char* and initialized to beginning of data

	mHead = (size_t*)mCircBuffer;		//Initialize head, first in the buffer
	mTail = ((size_t*)mCircBuffer + 1);	//Initialize tail, 8 bytes forward in the buffer

	if (type == PRODUCER) {
		//The head and the tail are stored first in memory and therefore this offset has to be considered when initialized
		*mHead = sizeof(size_t) * 2;
		*mTail = sizeof(size_t) * 2;
	}
}

ComLib::~ComLib() {
	ReleaseMutex(hMutex);
	UnmapViewOfFile((LPCVOID)mData);
	CloseHandle(hFileMap);
}

bool ComLib::send(const void* msg, const size_t length) {
	size_t msgSize = length + sizeof(Header);

	size_t padding = 0;
	if (msgSize % 64 != 0) {
		size_t multiplier = (size_t)ceil((msgSize / 64.0f)); //See how many times it can be divided by 64, rounds upwards so it'll always be at least 1
		padding = (64 * multiplier) - msgSize;
		msgSize += padding;
	}

	if (getFreeMemory() > msgSize) {
		WaitForSingleObject(hMutex, INFINITE); //Lock mutex

		if (mSize - *mHead >= msgSize || *mTail > *mHead) {
			Header header = { length }; //Save neccessary information for the consumer into a header
			memcpy(mCircBuffer + *mHead, &header, sizeof(Header));

			memcpy(mCircBuffer + *mHead + sizeof(Header), msg, length);

			*mHead += msgSize;

			ReleaseMutex(hMutex);
			return true;
		}
		else if(msgSize < *mTail){
			Header header = { length }; //We still need to leave a header
			memcpy(mCircBuffer + *mHead, &header, sizeof(Header));

			*mHead = sizeof(size_t) * 2; //Then reset the pointer to the beginning of memory
			ReleaseMutex(hMutex);
			return false;
		}
		else {
			return false;
		}
	}

	return false;
}

bool ComLib::recv(char* msg, size_t& length) {
	if (*mTail != *mHead && length > 0) {
		WaitForSingleObject(hMutex, INFINITE); //Lock mutex

		size_t msgSize = length + sizeof(Header);

		size_t padding = 0;
		if (msgSize % 64 != 0) {
			size_t multiplier = (size_t)ceil((msgSize / 64.0f)); //See how many times it can be divided by 64, rounds upwards so it'll always be at least 1
			padding = (64 * multiplier) - msgSize;
			msgSize += padding;
		}

		if (mSize - *mTail < msgSize) {
			*mTail = sizeof(size_t) * 2; //Reset the pointer to the beginning of memory
		}

		memcpy(msg, mCircBuffer + *mTail + sizeof(Header), length);

		*mTail += msgSize;

		ReleaseMutex(hMutex);
		return true;
	}

	return false;
}

size_t ComLib::nextSize() {
	if (*mTail != *mHead) {
		Header* header = (Header*)(mCircBuffer + *mTail);
		return header->msgSize;
	}
	else {
		return 0;
	}
}

size_t ComLib::getSizeBytes() const {
	return this->mSize;
}

size_t ComLib::getFreeMemory() {
	size_t freeMemory;
	if (*mHead > *mTail) {
		freeMemory = (mSize - *mHead /*+ *mTail*/) - (sizeof(size_t) * 2);
	}
	else if (*mHead < *mTail) {

		freeMemory = (*mTail - *mHead) - (sizeof(size_t) * 2);
	}
	else {

		freeMemory = mSize;
	}

	return freeMemory;
}
