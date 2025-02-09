/*
 * FILE        : ReliableUDP.c
 * PROJECT     : PROG1345 - Assignment #1
 * PROGRAMMER 1 : Virajsinh Solanki
 * PROGRAMMER 2 : Rudra NiteshKumar Bhatt
 * FIRST VERSION : 2025-02-27
 * DESCRIPTION  :
 * This program allows reliable file transfer over UDP, which supports both ASCII and binary files.
 * It ensures data integrity using MD5 checksum and calculates transfer speed.
 */


 /*
	 Reliability and Flow Control Example
	 From "Networking for Game Programmers" - http://www.gaffer.org/networking-for-game-programmers
	 Author: Glenn Fiedler <gaffer@gaffer.org>
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#pragma warning(disable : 4996)
#define BUFFER_SIZE 1024
#include "Net.h"
#include"md5.h"
#define MD5_DIGEST_LENGTH 16  


using namespace std;
using namespace net;
int n = 0;
const int ServerPort = 30000;
const int ClientPort = 30001;
const int ProtocolId = 0x11223344;
const float DeltaTime = 1.0f / 30.0f;
const float SendRate = 1.0f / 30.0f;
const float TimeOut = 10.0f;
const int PacketSize = 256;


//
// FUNCTION : Compute_Md5_Metadata
// DESCRIPTION :
// This function finds the MD5 hash of a file and creates a metadata string.  
// The metadata includes the file name, file size, and MD5 hash.  
// PARAMETERS :
// const char* filename : Name of the file to process.  
// char* output_md5 : Buffer to store the metadata string.  
// RETURNS :
// void : The function fills the output_md5 buffer with metadata.
//

void Compute_Md5_Metadata(const char* filename, char* output_md5) {
	FILE* file = fopen(filename, "rb");
	if (!file) {
		printf("Error opening file for MD5 computation: %s\n", filename);
		strcpy(output_md5, "ERROR");
		return;
	}

	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	uint8_t md5_hash[16];  // Buffer for MD5 digest
	md5File(file, md5_hash);  // Compute MD5 hash of file contents
	fclose(file);

	// Convert MD5 hash to hex string
	char md5_hex[33];
	for (int i = 0; i < 16; i++) {
		sprintf(md5_hex + (i * 2), "%02x", md5_hash[i]);
	}
	md5_hex[32] = '\0';

	// Format: "filename,filesize,md5_hash"
	snprintf(output_md5, 512, "%s,%ld,%s", filename, file_size, md5_hex);
}

//
// FUNCTION : Send_File_With_Md5
// DESCRIPTION :
// This function reads a file, calculates its MD5 hash, and sends it  
// over a reliable UDP connection in parts. It makes sure the file is sent correctly.  
// PARAMETERS :
// const string& fileName : Name of the file to send.  
// ReliableConnection& connection : The connection used to send data.  
// RETURNS :
// void : Sends the file but does not return any value.
//

void Send_File_With_Md5(const string& fileName, ReliableConnection& connection) {
	ifstream file(fileName, ios::binary | ios::ate);
	if (!file) {
		cerr << "Error: Could not open file " << fileName << endl;
		return;
	}

	streamsize fileSize = file.tellg();
	file.seekg(0, ios::beg);
	vector<unsigned char> fileData(fileSize);
	file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
	file.close();

	// Compute MD5 hash
	FILE* md5FilePtr = fopen(fileName.c_str(), "rb");
	if (!md5FilePtr) {
		cerr << "Error: Could not open file for MD5 computation!" << endl;
		return;
	}

	uint8_t md5Hash[16];
	md5File(md5FilePtr, md5Hash);
	fclose(md5FilePtr);

	// Convert MD5 hash to hex string
	char md5Hex[33];
	for (int i = 0; i < 16; i++) {
		sprintf(md5Hex + (i * 2), "%02x", md5Hash[i]);
	}
	md5Hex[32] = '\0';

	// Send metadata: "filename filesize md5_hash"
	char metadata[512] = { 0 };
	snprintf(metadata, sizeof(metadata), "%s %lld %s", fileName.c_str(), static_cast<long long>(fileSize), md5Hex);

	if (!connection.SendPacket(reinterpret_cast<unsigned char*>(metadata), strlen(metadata) + 1)) {
		cerr << "Error: Failed to send metadata!" << endl;
		return;
	}

	// Send file in chunks with full data handling
	for (size_t i = 0; i < fileSize; i += PacketSize) {
		size_t chunkSize = min(PacketSize, static_cast<size_t>(fileSize - i));
		size_t sent = 0;

		while (sent < chunkSize) {
			int bytesSent = connection.SendPacket(fileData.data() + i + sent, chunkSize - sent);
			if (bytesSent <= 0) {
				cerr << "Error: Failed to send file chunk!" << endl;
				return;
			}
			sent += bytesSent;
		}
	}

	cout << "File transfer completed successfully." << endl;

	//
// FUNCTION : Receive_File_With_Md5
// DESCRIPTION :
// This function receives a file over a reliable UDP connection.  
// It saves the file on disk and checks its MD5 hash to make sure the file is correct.  
// PARAMETERS :
// ReliableConnection& connection : The connection used to receive data.  
// RETURNS :
// void : Receives and verifies the file, then prints success or failure messages.
//

}void Receive_File_With_Md5(ReliableConnection& connection) {
	char metadata[512] = { 0 };
	long long fileSize = 0;
	char fileName[256] = { 0 };
	char md5Hash[33] = { 0 };

	// Receive metadata
	int metadataBytes = connection.ReceivePacket(reinterpret_cast<unsigned char*>(metadata), sizeof(metadata));
	if (metadataBytes <= 0) {
		printf("Error: Failed to receive metadata!\n");
		return;
	}

	// Ensure metadata is null-terminated
	metadata[metadataBytes] = '\0';

	// Parse metadata (Extract filename, file size, and MD5 hash)
	if (sscanf(metadata, "%255s %lld %32s", fileName, &fileSize, md5Hash) != 3) {
		printf("Error: Invalid metadata format!\n");
		return;
	}

	// Open file for writing
	FILE* outFile = fopen(fileName, "wb");
	if (!outFile) {
		perror("Error opening file");
		return;
	}

	// Receive file data in chunks
	size_t receivedBytes = 0;
	unsigned char buffer[PacketSize];

	while (receivedBytes < fileSize) {
		size_t chunkSize = min(PacketSize, static_cast<size_t>(fileSize - receivedBytes));
		size_t totalReceived = 0;

		while (totalReceived < chunkSize) {
			int bytesRead = connection.ReceivePacket(buffer + totalReceived, chunkSize - totalReceived);
			if (bytesRead <= 0) {
				printf("Error: Failed to receive file data!\n");
				fclose(outFile);
				return;
			}
			totalReceived += bytesRead;
		}

		fwrite(buffer, 1, totalReceived, outFile);
		receivedBytes += totalReceived;
	}

	fclose(outFile);

	// Compute MD5 checksum of received file
	FILE* md5FilePtr = fopen(fileName, "rb");
	if (!md5FilePtr) {
		printf("Error: Could not open file for MD5 verification!\n");
		return;
	}

	uint8_t receivedMd5Hash[16];
	md5File(md5FilePtr, receivedMd5Hash);
	fclose(md5FilePtr);

	// Convert MD5 hash to hex string
	char receivedMd5Hex[33];
	for (int i = 0; i < 16; i++) {
		sprintf(receivedMd5Hex + (i * 2), "%02x", receivedMd5Hash[i]);
	}
	receivedMd5Hex[32] = '\0';

	// Compare with expected MD5 hash
	if (strncmp(md5Hash, receivedMd5Hex, 32) == 0) {
		printf("MD5 checksum verified. File transfer is successful! ✅\n");
	}
	else {
		printf("Error: MD5 checksum mismatch! ❌\nExpected: %s\nReceived: %s\n", md5Hash, receivedMd5Hex);
	}
}

class FlowControl
{
public:
	FlowControl()
	{
		printf("flow control initialized\n");
		Reset();
	}