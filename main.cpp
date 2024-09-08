#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <string>
#include <thread>

#include "WaveReader.h"

#define AL_LIBTYPE_STATIC
#define AL_ALEXT_PROTOTYPES
#include <AL/alext.h>

static void usage(const char* pname)
{
	std::cout << "Usage: " << pname << " some_file.wav\n";
}

[[maybe_unused]] static bool isAlError()
{
	const auto err = alGetError();
	if (err != AL_NO_ERROR) {
		std::cout << "AL error: " << err << std::endl;
		return true;
	}
	return false;
}

int main(const int argc, const char** argv)
{
	if (argc != 2) {
		usage(argv[0]);
		return 1;
	}

	std::ifstream ifs(argv[1], std::ios_base::binary);
	if (!ifs) {
		std::cerr << "Error: could not open " << argv[1] << "\n";
		return 2;
	}

	std::vector<char> data;
	ifs.seekg(0, std::ios_base::end);
	const auto fileSize = ifs.tellg();
	data.resize(fileSize);
	ifs.seekg(0, std::ios_base::beg);
	ifs.read(data.data(), fileSize);
	ifs.close();

	std::cout << "File size: " << data.size() << "\n";

	WaveReader waveReader(data);
	const auto pcmCount = waveReader.getPcmCount();
	std::cout << "Channels: " << waveReader.getChannelsCount() << "\n";
	std::cout << "Sample rate: " << waveReader.getSampleRate() << "\n";
	std::cout << "Pcm count: " << pcmCount << "\n";

	std::vector<uint16_t> pcmData;
	pcmData.resize(pcmCount);
	waveReader.decode(pcmData, 0, pcmCount);


	const auto device = alcOpenDevice("");
	if (device == nullptr) {
		std::cerr << "Error: could not open device\n";
		return 3;
	}
	const auto ctx = alcCreateContext(device, nullptr);
	if (ctx == nullptr) {
		std::cerr << "Error: could not create context\n";
		return 4;
	}
	if (!alcMakeContextCurrent(ctx)) {
		std::cerr << "Error: could not make context current\n";
	}

	std::cout << "ALC_SOFT_HRTF: " << std::to_string(bool(alcIsExtensionPresent(&(*device), "ALC_SOFT_HRTF"))) << "\n";
	ALCint hrtf_count;
	alcGetIntegerv(device, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &hrtf_count);
	std::cout << "HRTFs count: " << hrtf_count << "\n";
	for (int i = 0; i < hrtf_count; ++i) {
		std::string s = alcGetStringiSOFT(device, ALC_HRTF_SPECIFIER_SOFT, i);
		std::cout << "HRTF Name: " << s << "\n";
	}

	ALint hrtfParam = 0;
	alcGetIntegerv(device, ALC_HRTF_SOFT, 1, &hrtfParam);
	std::cout << "Use HRTF: " << hrtfParam << "\n";

	alcGetIntegerv(device, ALC_HRTF_STATUS_SOFT, 1, &hrtfParam);
	std::cout << "HRTF status: " << hrtfParam << std::endl;

	ALuint buf1;
	alGenBuffers(1, &buf1);
	assert(buf1 != 0);

	assert(waveReader.getChannelsCount() < 3);
	const auto format = waveReader.getChannelsCount() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	alBufferData(buf1, format, pcmData.data(), pcmData.size() * sizeof(uint16_t), waveReader.getSampleRate());
	assert(!isAlError());

	ALuint ss1;
	alGenSources(1, &ss1);
	assert(!isAlError());

	alSourcei(ss1, AL_BUFFER, buf1);
	assert(!isAlError());

	alSourcePlay(ss1);
	assert(!isAlError());

	std::cout << "Playing..." << std::endl;
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		ALint sourceState;
		alGetSourcei(ss1, AL_SOURCE_STATE, &sourceState);
		if (sourceState != AL_PLAYING) break;
	}

	std::cout << "Finished." << std::endl;

	alDeleteSources(1, &ss1);
	alDeleteBuffers(1, &buf1);

	alcMakeContextCurrent(nullptr);
	alcDestroyContext(ctx);
	alcCloseDevice(device);

	return 0;
}
