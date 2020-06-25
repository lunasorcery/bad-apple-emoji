#include <cstdint>
#include <vector>
#include "data_header.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

int const kWidth = WIDTH;
int const kHeight = HEIGHT;
int const kFramerate = FRAMERATE;

// 🌑🌒🌓🌔🌕🌖🌗🌘
uint8_t const lutRightGlow[] = { 0, 1, 2, 3, 4 }; // 🌑🌒🌓🌔🌕
uint8_t const lutLeftGlow [] = { 0, 7, 6, 5, 4 }; // 🌑🌘🌗🌖🌕

struct frame_t {
	uint8_t pixels[kWidth * kHeight];
};

struct packed_frame_t {
	uint8_t pixels[(kWidth*kHeight*3+7)/8];

	packed_frame_t() { }
	packed_frame_t(frame_t frame)
	{
		memset(pixels, 0, sizeof(pixels));
		int offset = 0;
		for (int i = 0; i < kWidth*kHeight; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				pixels[offset/8] |= ((frame.pixels[i]>>j)&1) << (offset%8);
				offset++;
			}
		}
	}
};

packed_frame_t calculateDeltaFrame(packed_frame_t before, packed_frame_t after)
{
	packed_frame_t diff;
	for (int i = 0; i < sizeof(diff.pixels); ++i)
	{
		diff.pixels[i] = before.pixels[i]^after.pixels[i];
	}
	if (((kWidth*kHeight)%8) != 0)
	{
		diff.pixels[sizeof(diff.pixels)-1] &= 0xff >> (8-((kWidth*kHeight)%8));
	}
	return diff;
}

void writeRLE(std::vector<uint8_t> const& data, FILE* fh)
{
	uint8_t const bitRepeat = 0x00;
	uint8_t const bitLiteral = 0x80;

	size_t offset = 0;
	while (offset < data.size())
	{
		if (data.size() - offset <= 2)
		{
			uint8_t const key = bitLiteral | (data.size() - offset);
			fwrite(&key, 1, 1, fh);
			fwrite(&data[offset], 1, data.size() - offset, fh);
			break;
		}

		bool findRepeats = data[offset+0] == data[offset+1] && data[offset+0] == data[offset+2];
		if (findRepeats)
		{
			int count = 3;
			for (size_t i = 3; offset+i < data.size(); ++i)
			{
				if (data[offset+i] == data[offset+0])
					count++;
				else
					break;
			}
			if (count > 127)
			{
				count = 127;
			}
			uint8_t const key = bitRepeat | count;
			fwrite(&key, 1, 1, fh);
			fwrite(&data[offset], 1, 1, fh);
			offset += count;
		}
		else
		{
			int count = 3;
			uint8_t last = data[offset+2];
			for (size_t i = 0; offset+i+3 < data.size(); ++i)
			{
				if (data[offset+i+3] == last && data[offset+i+4] == last)
				{
					count--;
					break;
				}
				else
				{
					count++;
					last = data[offset+i+3];
				}
			}
			if (count > 127)
			{
				count = 127;
			}
			uint8_t const key = bitLiteral | count;
			fwrite(&key, 1, 1, fh);
			fwrite(&data[offset], 1, count, fh);
			offset += count;
		}
	}
}

int main()
{
	std::vector<frame_t> frames;

	for (int frameIndex = 1; ; ++frameIndex)
	{
		char filename[32];
		sprintf(filename, "frames/%04d.png", frameIndex);

		int w,h,n;
		uint8_t const* data = stbi_load(filename, &w, &h, &n, 1);
		if (!data)
			break;

		if (w != kWidth*2 || h != kHeight)
			break;

		printf("Converting frame %d\n", frameIndex);

		frame_t currentFrame;
		for (int y = 0; y < kHeight; ++y)
		{
			for (int x = 0; x < kWidth; ++x)
			{
				uint8_t const a = data[(y*kWidth*2)+(x*2)];
				uint8_t const b = data[(y*kWidth*2)+(x*2)+1];
				int const brightness = ((a+b)*5)/512;

				// cheap binary version
				//frameData.pixels[y*kWidth+x] = (a+b>372) ? 4 : 0;

				// more expressive detail using all 8 moon emojis
				currentFrame.pixels[y*kWidth+x] = ((a<b) ? lutRightGlow : lutLeftGlow)[brightness];
			}
		}
		frames.push_back(currentFrame);

		stbi_image_free((void*)data);
	}

	printf("Squashing frames...\n");
	std::vector<packed_frame_t> crunchedFrames;
	for (const auto& frame : frames)
	{
		crunchedFrames.push_back(packed_frame_t(frame));
	}

	printf("Delta-encoding frames...\n");
	std::vector<packed_frame_t> deltaFrames;
	deltaFrames.push_back(crunchedFrames[0]);
	for (size_t i = 1; i < crunchedFrames.size(); ++i)
	{
		deltaFrames.push_back(calculateDeltaFrame(crunchedFrames[i-1], crunchedFrames[i]));
	}

	FILE* fh = fopen("data.bin", "wb");
	data_header_t header;
	header.framerate = kFramerate;
	header.width = kWidth;
	header.height = kHeight;
	header.frameCount = frames.size();
	fwrite(&header, sizeof(header), 1, fh);
	printf("Compressing and saving frames...\n");
	for (const auto& frame : deltaFrames)
	{
		writeRLE(std::vector<uint8_t>(frame.pixels, frame.pixels+sizeof(frame.pixels)), fh);
	}

	fclose(fh);
}
