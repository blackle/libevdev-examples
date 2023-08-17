#include <libevdev/libevdev.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>

struct libevdev* find_device_by_name(const std::string& requested_name) {
	struct libevdev *dev = nullptr;

	for (int i = 0;; i++) {
		std::string path = "/dev/input/event" + std::to_string(i);
		int fd = open(path.c_str(), O_RDWR|O_CLOEXEC);
		if (fd == -1) {
			break; // no more character devices
		}
		if (libevdev_new_from_fd(fd, &dev) == 0) {
			std::string name = libevdev_get_name(dev);
			if (name == requested_name) {
				return dev;
			}
			libevdev_free(dev);
			dev = nullptr;
		}
		close(fd);
	}

	return nullptr;
}

int main() {
	struct libevdev *dev = find_device_by_name("Usb KeyBoard Usb KeyBoard");

	if (dev == nullptr) {
		std::cerr << "Couldn't find device!" << std::endl;
		return -1;
	}

	libevdev_free(dev);
	return 0;
}