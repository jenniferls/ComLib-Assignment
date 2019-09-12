#include <Windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>

#include "ComLib.h"

size_t convertToInt(std::string string) {
	std::istringstream number(string);
	size_t integer;
	number >> integer;

	return integer;
}

// random character generation of "len" bytes.
// the pointer s must point to a big enough buffer to hold "len" bytes.
void gen_random(char *s, const int len) {
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	for (auto i = 0; i < len; ++i) {
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}
	s[len - 1] = 0;
}

int main(int argc, char* argv[]) {
	if (argc != 6) {
		printf("Error! Incorrect amount of arguements. \n");
		system("pause");
		return -1;
	}

	size_t sizeInMB = convertToInt(argv[3]);
	size_t sleepTime = convertToInt(argv[2]); //Sleep(sleepTime);
	size_t msgNr = convertToInt(argv[4]);
	size_t msgLength;
	bool random = false;

	srand(0);

	if (strcmp(argv[5], "random") == 0) {
		random = true;
	}
	else {
		msgLength = convertToInt(argv[5]);
	}

	if (strcmp(argv[1], "producer") == 0) {
		//printf("This is a producer app. \n"); //Debug

		ComLib comlib("myFileMap", sizeInMB, ComLib::PRODUCER);

		for (size_t i = msgNr; i > 0; i--) {
			if (random == true) {
				unsigned int randomNum = rand() % (((sizeInMB / 2) - 1) << 20) + 1; //Place-holder
				msgLength = randomNum;
			}

			void* msg = new char[msgLength];
			gen_random((char*)msg, (const int)msgLength);

			while (msgNr == i) { //Will try to send message until it succeeds
				if (comlib.send(msg, msgLength) == true) {
					msgNr -= 1;
					std::cout << /*"Message" <<*//* i + 1 << ": " <<*/ (char*)msg << std::endl;
					delete msg;
					Sleep((DWORD)sleepTime);
				}
			}
		}

		//for (size_t i = 0; i < msgNr; i++) {
		//	if (random == true) {
		//		unsigned int randomNum = rand() % (((sizeInMB / 2) - 1) << 20); //Place-holder
		//		msgLength = randomNum;
		//	}

		//	void* msg = new char[msgLength];
		//	gen_random((char*)msg, msgLength);

		//	comlib.send(msg, msgLength);

		//	std::cout << /*"Message " << i << ": " << */(char*)msg << std::endl;
		//	delete[] msg;
		//}
		//system("pause");
	}
	else if (strcmp(argv[1], "consumer") == 0) {
		//printf("This is a consumer app. \n"); //Debug

		ComLib comlib("myFileMap", sizeInMB, ComLib::CONSUMER);
		char* msg;

		while (msgNr > 0) {
			msgLength = comlib.nextSize();
			if (msgLength > 0) {
				msg = new char[msgLength];
				if (comlib.recv(msg, msgLength) == true) {
					msgNr -= 1;
					std::cout << /*"Message" << i << ": " << */msg << std::endl;
					delete msg;
					Sleep((DWORD)sleepTime);
				}
			}
		}

		//for (size_t i = 0; i < msgNr; i++) {
		//	msgLength = comlib.nextSize();
		//	char* msg = new char[msgLength];
		//	comlib.recv(msg, msgLength);
		//	std::cout << /*"Message" << i << ": " << */msg << std::endl;
		//	delete[] msg;
		//}
		//system("pause");
	}
	else {
		printf("Error! No type. \n");
		system("pause");
		return -1;
	}

	return 0;
}