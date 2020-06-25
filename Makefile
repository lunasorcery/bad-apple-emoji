WIDTH ?= 20
HEIGHT ?= 12
CXX ?= clang

.PHONY: all run clean clean-all

all: player data.bin

run: player data.bin
	./player

source.mp4:
	youtube-dl -f 134 https://www.youtube.com/watch?v=FtutLA63Cp8 -o source.mp4

frames: source.mp4
	mkdir -p frames
	ffmpeg -i source.mp4 -vf scale=$$((2*$(WIDTH))):$(HEIGHT) frames/%04d.png

converter: converter.cpp data_header.h source.mp4
	$(CXX) converter.cpp -o converter -lstdc++ -std=c++11 -DWIDTH=$(WIDTH) -DHEIGHT=$(HEIGHT) -DFRAMERATE=`ffmpeg -i source.mp4 2>&1 | sed -n "s/.*, \(.*\) fp.*/\1/p"`

data.bin: converter frames
	./converter

player: player.cpp data_header.h
	$(CXX) player.cpp -o player -lstdc++ -std=c++11

clean:
	rm -rf frames/
	rm -f data.bin
	rm -f converter
	rm -f player

clean-all: clean
	rm -f source.mp4