TARGET ?= DoukutsuEdit
APP     = $(TARGET)

CC   ?= cc
CXX  ?= c++
OPTS := -Wall -Wextra
INCS := $(shell sdl2-config --cflags) -Iimgui
LIBS := $(shell sdl2-config --libs)

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS += -lpthread -ldl -static-libgcc -static-libstdc++
endif
ifeq ($(UNAME), Darwin)
SDL2_DYLIB = /usr/local/opt/sdl2/lib/libSDL2-2.0.0.dylib
LDFLAGS += -framework CoreFoundation -framework OpenGL
APP = $(TARGET).app
endif

CCS  := $(wildcard *.c)
CPPS := $(wildcard *.cpp)
CPPS += $(wildcard imgui/*.cpp)
OBJS := $(CCS:.c=.o)
OBJS += $(CPPS:.cpp=.o)

all: release

release: OPTS += -Os
release: $(APP)

debug: OPTS += -Og -g -DDEBUG
debug: $(APP)

$(TARGET).app: $(TARGET)
	mkdir -p $(APP)/Contents/{MacOS,Resources,Frameworks}
	cp -f macos/Info.plist $(APP)/Contents/
	install_name_tool -change $(SDL2_DYLIB) @executable_path/../Resources/libSDL2-2.0.0.dylib $(TARGET)
	cp -f $(TARGET) $(APP)/Contents/MacOS/
	cp -f $(SDL2_DYLIB) $(APP)/Contents/Resources/
	cp -f imgui.ini $(APP)/Contents/Resources/
	rm -rf macos/$(TARGET).iconset
	mkdir -p macos/$(TARGET).iconset
	sips -z 16 16   macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_16x16.png
	sips -z 32 32   macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_16x16@2x.png
	sips -z 32 32   macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_32x32.png
	sips -z 64 64   macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_32x32@2x.png
	sips -z 128 128 macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_128x128.png
	sips -z 256 256 macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_128x128@2x.png
	sips -z 256 256 macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_256x256.png
	sips -z 512 512 macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_256x256@2x.png
	sips -z 512 512 macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_512x512.png
	sips -z 1024 1024 macos/$(TARGET).png --out macos/$(TARGET).iconset/icon_512x512@2x.png
	iconutil --convert icns macos/$(TARGET).iconset
	cp -f macos/$(TARGET).icns $(APP)/Contents/Resources/

$(TARGET): $(OBJS)
	$(CXX) $(OPTS) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(OPTS) -std=c99 $(INCS) $< -o $@ -c

%.o: %.cpp
	$(CXX) $(OPTS) -std=c++17 $(INCS) $< -o $@ -c

clean:
	rm -f $(OBJS) $(TARGET)
