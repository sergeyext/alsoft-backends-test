#pragma once

#include <cstdint>
#include <vector>

class WaveReader
{
public:
	explicit WaveReader(const std::vector<char>&);

	[[nodiscard]] auto getPcmCount() const { return pcmCount; }
	uint32_t decode(std::vector<uint16_t>&, uint32_t pcmStart, uint32_t pcmCount);
	[[nodiscard]] auto getChannelsCount() const { return channelsCount; }
	[[nodiscard]] auto getSampleRate() const { return sampleRate; }

private:
	void decodePcm(std::vector<uint16_t>&, uint32_t pcmStart, uint32_t rawPcmCount);
	void decodeFloatPcm(std::vector<uint16_t>&, uint32_t pcmStart, uint32_t rawPcmCount);

private:
	uint16_t channelsCount = 0;
	uint32_t sampleRate = 0;
	uint8_t bytesPerSample = 0;
	const char* dataSection = nullptr;
	uint32_t pcmCount = 0;
	uint16_t compressCode = 0;
};
