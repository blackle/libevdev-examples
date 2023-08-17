#include <libevdev/libevdev.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>

int main() {
	struct libevdev *dev = nullptr;

	for (int i = 0;; i++) {
		std::string path = "/dev/input/event" + std::to_string(i);
		int fd = open(path.c_str(), O_RDWR|O_CLOEXEC);
		if (fd == -1) {
			break; // no more character devices
		}
		if (libevdev_new_from_fd(fd, &dev) == 0) {
			std::string phys = libevdev_get_phys(dev);
			std::string name = libevdev_get_name(dev);

			std::cout << path << std::endl;
			std::cout << "- phys: " << phys << std::endl;
			std::cout << "- name: " << name << std::endl;
			std::cout << std::endl;

			libevdev_free(dev);
			dev = nullptr;
		}
		close(fd);
	}
	return 0;
}