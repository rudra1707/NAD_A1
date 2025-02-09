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


	//
	// FUNCTION : Reset
	// DESCRIPTION :
	// This function resets the flow control system.  
	// It sets the starting values for controlling the data speed.  
	// PARAMETERS :
	// None  
	// RETURNS :
	// void : Resets the internal values to default.
	//

	void Reset()
	{
		mode = Bad;
		penalty_time = 4.0f;
		good_conditions_time = 0.0f;
		penalty_reduction_accumulator = 0.0f;
	}

	//
// FUNCTION : Update
// DESCRIPTION :
// This function updates the flow control system based on network speed.  
// It changes the sending speed to avoid network problems.  
// PARAMETERS :
// float deltaTime : Time since the last update.  
// float rtt : Current round-trip time in milliseconds.  
// RETURNS :
// void : Updates the internal values for data sending speed.
//

	void Update(float deltaTime, float rtt)
	{
		const float RTT_Threshold = 250.0f;
		if (mode == Good)
		{
			if (rtt > RTT_Threshold)
			{
				printf("* dropping to bad mode *\n");
				mode = Bad;
				if (good_conditions_time < 10.0f && penalty_time < 60.0f)
				{
					penalty_time *= 2.0f;
					if (penalty_time > 60.0f)
						penalty_time = 60.0f;
					printf("penalty time increased to %.1f\n", penalty_time);
				}
				good_conditions_time = 0.0f;
				penalty_reduction_accumulator = 0.0f;
				return;
			}
			good_conditions_time += deltaTime;
			penalty_reduction_accumulator += deltaTime;
			if (penalty_reduction_accumulator > 10.0f && penalty_time > 1.0f)
			{
				penalty_time /= 2.0f;
				if (penalty_time < 1.0f)
					penalty_time = 1.0f;
				printf("penalty time reduced to %.1f\n", penalty_time);
				penalty_reduction_accumulator = 0.0f;
			}
		}
		if (mode == Bad)
		{
			if (rtt <= RTT_Threshold)
				good_conditions_time += deltaTime;
			else
				good_conditions_time = 0.0f;
			if (good_conditions_time > penalty_time)
				if (good_conditions_time > penalty_time)
				{
					printf("* upgrading to good mode *\n");
					good_conditions_time = 0.0f;
					penalty_reduction_accumulator = 0.0f;
					mode = Good;
					return;
				}
		}
	}

	//
// FUNCTION : GetSendRate
// DESCRIPTION :
// This function returns the current speed of sending data.  
// PARAMETERS :
// None  
// RETURNS :
// float : The speed of sending data in packets per second.
//

	float GetSendRate()
	{
		return mode == Good ? 30.0f : 10.0f;
	}
private:
	enum Mode
	{
		Good,
		Bad
	};
	Mode mode;
	float penalty_time;
	float good_conditions_time;
	float penalty_reduction_accumulator;
};


//
// FUNCTION : main
// DESCRIPTION :
// This is the main function. It starts the UDP connection,  
// controls client-server communication, and manages file transfer.  
// PARAMETERS :
// int argc : Number of command-line arguments.  
// char* argv[] : Command-line arguments, such as the server IP address.  
// RETURNS :
// int : Returns 0 if the program runs successfully, otherwise returns an error code.
//

int main(int argc, char* argv[])
{
	// parse command line
	enum Mode
	{
		Client,
		Server
	};
	Mode mode = Server;
	Address address;
	char filename[256] = { 0 };
	if (argc >= 2)
	{
		int a, b, c, d;
#pragma warning(suppress : 4996) 
		if (sscanf(argv[1], "%d.%d.%d.%d", &a, &b, &c, &d))
		{
			mode = Client;
			address = Address(a, b, c, d, ServerPort);
		}
	}

	if (!InitializeSockets())
	{
		printf("failed to initialize sockets\n");
		return 1;
	}
	ReliableConnection connection(ProtocolId, TimeOut);
	const int port = mode == Server ? ServerPort : ClientPort;
	if (!connection.Start(port))
	{
		printf("could not start connection on port %d\n", port);
		return 1;
	}
	if (mode == Client)
		connection.Connect(address);
	else
		connection.Listen();
	bool connected = false;
	float sendAccumulator = 0.0f;
	float statsAccumulator = 0.0f;
	FlowControl flowControl;
	while (true)
	{

		if (connection.IsConnected())
			flowControl.Update(DeltaTime, connection.GetReliabilitySystem().GetRoundTripTime() * 1000.0f);
		const float sendRate = flowControl.GetSendRate();

		if (mode == Server && connected && !connection.IsConnected())
		{
			flowControl.Reset();
			printf("reset flow control\n");
			connected = false;
		}
		if (!connected && connection.IsConnected())
		{
			printf("client connected to server\n");
			connected = true;
		}

		if (!connected && connection.ConnectFailed())
		{
			printf("connection failed\n");
			break;
		}

		if (mode == Client && connected) {
			printf("Enter filename to send: ");
			if (scanf("%255s", filename) != 1) {
				printf("Error: Invalid filename input!\n");
				return 1;
			}

			FILE* file = fopen(filename, "rb");
			if (!file) {
				printf("Error: File not found! (%s)\n", filename);
				return 1;
			}
			fclose(file);
			Send_File_With_Md5(filename, connection);
		}
		if (mode == Server && connected) {


			Receive_File_With_Md5(connection);
		}



		// send and receive packets
		sendAccumulator += DeltaTime;
		while (sendAccumulator > 1.0f / sendRate)
		{
			unsigned char packet[PacketSize];
			char pack[1000] = "";
			connection.SendPacket(packet, sizeof(packet));
			n++;
			sendAccumulator -= 1.0f / sendRate;
		}
		while (true)
		{
			unsigned char packet[256];
			int bytes_read = connection.ReceivePacket(packet, sizeof(packet));
			if (bytes_read == 0)
				break;
			printf("packets: %s ", packet);
		}



#ifdef SHOW_ACKS
		unsigned int* acks = NULL;
		int ack_count = 0;
		connection.GetReliabilitySystem().GetAcks(&acks, ack_count);
		if (ack_count > 0)
		{
			printf("acks: %d", acks[0]);
			for (int i = 1; i < ack_count; ++i)
				printf(",%d", acks[i]);
			printf("\n");
		}
#endif
		// update connection
		connection.Update(DeltaTime);
		// show connection stats
		statsAccumulator += DeltaTime;
		while (statsAccumulator >= 0.25f && connection.IsConnected())
		{
			float rtt = connection.GetReliabilitySystem().GetRoundTripTime();
			unsigned int sent_packets = connection.GetReliabilitySystem().GetSentPackets();
			unsigned int acked_packets = connection.GetReliabilitySystem().GetAckedPackets();
			unsigned int lost_packets = connection.GetReliabilitySystem().GetLostPackets();
			float sent_bandwidth = connection.GetReliabilitySystem().GetSentBandwidth();
			float acked_bandwidth = connection.GetReliabilitySystem().GetAckedBandwidth();
			printf("rtt %.1fms, sent %d, acked %d, lost %d (%.1f%%), sent bandwidth = %.1fkbps, acked bandwidth = %.1fkbps\n",
				rtt * 1000.0f, sent_packets, acked_packets, lost_packets,
				sent_packets > 0.0f ? (float)lost_packets / (float)sent_packets * 100.0f : 0.0f,
				sent_bandwidth, acked_bandwidth);
			statsAccumulator -= 0.25f;
		}
		net::wait(DeltaTime);
	}
	ShutdownSockets();
	return 0;
}