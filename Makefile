main : main.cpp Makefile
	g++ -o main main.cpp `pkg-config --cflags --libs libevdev`