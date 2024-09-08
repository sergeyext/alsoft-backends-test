#include "WaveReader.h"

#include <algorithm>
#include <cassert>
#include <iostream>


static const uint16_t COMPRESS_CODE_PCM = 1;
static const uint16_t COMPRESS_CODE_FLOAT_PCM = 3;

WaveReader::WaveReader(const std::vector<char>& data)
{
	// Read and check file header
	assert(data.size() >= 8);
	const char* ptr = data.data();
	const bool isRiff = std::equal(ptr, ptr + 4, "RIFF");
	assert(isRiff);
	ptr += 4;
	uint32_t sectionSize;
	{
		const auto res = memcpy_s(&sectionSize, sizeof(decltype(sectionSize)), ptr, 4);
		assert(res == 0);
	}
	assert(sectionSize == data.size() - 8);
	ptr += 4;
	const bool isWave = std::equal(ptr, ptr + 4, "WAVE");
	assert(isWave);
	ptr += 4;

	// Find fmt and data sections
	const char* fmtSection = nullptr;
	uint32_t dataSectionSize = 0;
	while (true) {
		const bool isData = std::equal(ptr, ptr + 4, "data");
		const bool isFmt = std::equal(ptr, ptr + 4, "fmt ");
		const bool isWavl = std::equal(ptr, ptr + 4, "wavl");
		const bool isSlnt = std::equal(ptr, ptr + 4, "slnt");
		assert(!(isData && isFmt));
		assert(!isWavl);
		assert(!isSlnt);
		{
			const auto res = memcpy_s(&sectionSize, sizeof(decltype(sectionSize)), ptr + 4, 4);
			assert(res == 0);
		}
		const auto currentOffset = static_cast<uint32_t>(ptr - static_cast<const char*>(data.data()));
		assert(data.size() > currentOffset);
		assert(sectionSize <= data.size() - currentOffset);

		if (isData) {
			dataSectionSize = sectionSize;
			assert(dataSection == nullptr);
			dataSection = ptr + 8;
		}
		else if (isFmt) {
			assert(fmtSection == nullptr);
			fmtSection = ptr + 8;
		}
		else std::cerr << "Warning: ignoring wav section <" << std::string(ptr, 4) << ">, size: " << sectionSize << std::endl;

		const uint32_t skipBytesCount = 8 + sectionSize + (sectionSize % 2);
		if (currentOffset + skipBytesCount == data.size()) break;
		ptr += skipBytesCount;
	}

	assert(dataSection != nullptr);
	assert(fmtSection != nullptr);

	// Parse format
	// Compress code
	{
		auto done = memcpy_s(&compressCode, sizeof(compressCode), fmtSection, 2);
		assert(done == 0);
	}
	fmtSection += 2;
	assert(compressCode == COMPRESS_CODE_PCM || compressCode == COMPRESS_CODE_FLOAT_PCM); // Only uncompressed wav is supported.

	// Channels count
	{
		auto done = memcpy_s(&channelsCount, sizeof(channelsCount), fmtSection, 2);
		assert(done == 0);
	}
	fmtSection += 2;
	assert(channelsCount == 1 || channelsCount == 2);

	// Sample rate
	{
		auto done = memcpy_s(&sampleRate, sizeof(sampleRate), fmtSection, 4);
		assert(done == 0);
	}
	fmtSection += 4;

	fmtSection += 4; // Avg bytes per second

	// Block align for checking
	uint16_t blockAlign;
	{
		auto done = memcpy_s(&blockAlign, sizeof(blockAlign), fmtSection, 2);
		assert(done == 0);
	}
	fmtSection += 2;

	uint16_t bitsPerSample = 0;
	auto done = memcpy_s(&bitsPerSample, sizeof(decltype(bitsPerSample)), fmtSection, 2);
	assert(done == 0);
	assert((bitsPerSample % 8) == 0);
	bytesPerSample = bitsPerSample / 8;
	assert(bytesPerSample <= 4);
	assert(bytesPerSample * channelsCount == blockAlign);

	assert((dataSectionSize % bytesPerSample) == 0);
	pcmCount = dataSectionSize / bytesPerSample;
}

void WaveReader::decodePcm(std::vector<uint16_t>& output, const uint32_t pcmStart, const uint32_t rawPcmCount)
{
	switch (bytesPerSample) {
		case 1:
			{
				const auto* ptr = static_cast<const uint8_t*>(static_cast<const void*>(dataSection + pcmStart));
				for (uint32_t i = 0; i < rawPcmCount; ++i) {
					auto val = static_cast<int16_t>(ptr[i]);
					val = (val - 127) * 256;
					output[i] = val;
				}
			}
			break;
		case 2:
			{
				const auto writeSize = rawPcmCount * 2;
				const auto done = memcpy_s(output.data(), writeSize, dataSection + pcmStart * 2, writeSize);
				assert(done == 0);
			}
			break;
		case 3:
			{
				const auto* ptr = static_cast<const uint8_t*>(static_cast<const void*>(dataSection + pcmStart * 3));
				for (uint32_t i = 0; i < rawPcmCount; ++i) {
					uint32_t buf = 0;
					{
						auto done = memcpy_s(static_cast<char*>(static_cast<void*>(&buf)), sizeof(buf), ptr + i * 3, 3);
						assert(done == 0);
					}
					static const uint32_t signBit = 0x800000;
					const uint32_t signum = (buf & signBit) >> 16u;
					assert(signum == 0 || signum == 0x80);
					buf = buf & (~signBit); // remove sign
					assert(buf <= 0xFFFFFF);
					auto val = static_cast<int32_t>(buf);
					*(static_cast<uint8_t*>(static_cast<void*>(&val)) + 3) |= static_cast<uint8_t>(signum); // apply sign at right place
					static const double factor = 1.0 / double(0xFF);
					output[i] = static_cast<uint16_t>(double(val) * factor);
				}
			}
			break;
		case 4:
			{
				const auto* ptr = static_cast<const int32_t*>(static_cast<const void*>(dataSection + pcmStart * 4));
				static const double factor = 1.0 / double(0xFFFF);
				for (uint32_t i = 0; i < rawPcmCount; ++i) output[i] = static_cast<uint16_t>(double(ptr[i]) * factor);
			}
			break;
		default:
			assert(false);
			break;
	}
}

void WaveReader::decodeFloatPcm(std::vector<uint16_t>& output, const uint32_t pcmStart, const uint32_t rawPcmCount)
{
	const auto* ptr = static_cast<const float*>(static_cast<const void*>(dataSection + pcmStart * 4));
	for (uint32_t i = 0; i < rawPcmCount; ++i) output[i] = static_cast<uint16_t>(ptr[i] * 0x7FFF);
}

uint32_t WaveReader::decode(std::vector<uint16_t>& output, const uint32_t pcmStart, const uint32_t pcms)
{
	const uint32_t pcmAvailable = pcmCount - pcmStart;
	const auto rawPcmCount = std::min(pcms, pcmAvailable);

	switch (compressCode) {
		case COMPRESS_CODE_PCM:
			decodePcm(output, pcmStart, rawPcmCount);
			break;
		case COMPRESS_CODE_FLOAT_PCM:
			decodeFloatPcm(output, pcmStart, rawPcmCount);
			break;
		default:
			assert(false);
			break;
	}

	return rawPcmCount;
}
