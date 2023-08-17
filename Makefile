all : step-1 step-2 step-3 step-4 step-5 step-6 step-7

step-1 : step-1-enumerating-devices.cpp
	g++ -o $@ $^ `pkg-config --cflags --libs libevdev` -pthread
step-2 : step-2-find-by-name.cpp
	g++ -o $@ $^ `pkg-config --cflags --libs libevdev` -pthread
step-3 : step-3-grab-and-dump.cpp
	g++ -o $@ $^ `pkg-config --cflags --libs libevdev` -pthread
step-4 : step-4-process-key.cpp
	g++ -o $@ $^ `pkg-config --cflags --libs libevdev` -pthread
step-5 : step-5-group-capabilities.cpp
	g++ -o $@ $^ `pkg-config --cflags --libs libevdev` -pthread
step-6 : step-6-virtual-mouse-clicks.cpp
	g++ -o $@ $^ `pkg-config --cflags --libs libevdev` -pthread
step-7 : step-7-virtual-mouse-movement.cpp
	g++ -o $@ $^ `pkg-config --cflags --libs libevdev` -pthread