#include <libevdev/libevdev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <grp.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>

void handle_event(int code, bool isDown) {
	if (isDown) {
		if (code == 69) {
			system("xterm &");
		}
		if (code == 77) {
			system("xdotool mousemove_relative -- 10 0 &");
		}
		if (code == 75) {
			system("xdotool mousemove_relative -- -10 0 &");
		}
		if (code == 80) {
			system("xdotool mousemove_relative -- 0 10 &");
		}
		if (code == 72) {
			system("xdotool mousemove_relative -- 0 -10 &");
		}
	}
}

int main(int argc, char** argv) {
	auto grp = getgrnam("input");
	if (grp == nullptr) {
		std::cerr << "getgrnam(\"input\") failed" << std::endl;
		return -1;
	}
	int oldgid = getgid();
	if (setgid(grp->gr_gid) < 0) {
		std::cerr << "couldn't change group to input!" << std::endl;
		return -1;
	}

	struct libevdev *dev = nullptr;
	for (int i = 0; i < 256; i++) {
		std::string path = "/dev/input/event" + std::to_string(i);
		int fd = open(path.c_str(), O_RDWR|O_CLOEXEC);
		if (fd == -1) {
			break;
		}
		if (libevdev_new_from_fd(fd, &dev) == 0) {
			std::string phys = libevdev_get_phys(dev);
			std::string name = libevdev_get_name(dev);
			auto uniq_c = libevdev_get_uniq(dev);
			std::string uniq = uniq_c ? uniq_c : "(none)";
			std::cout << phys << " " << name << " " << uniq << std::endl;
			if (name == "Usb KeyBoard Usb KeyBoard") {
				break;
			}
			libevdev_free(dev);
		}
		close(fd);
	}
	//drop back into old permissions
	if (setgid(oldgid) < 0) {
		std::cerr << "couldn't switch back to old group!" << std::endl;
		return -1;
	}

	if (dev == nullptr) {
		std::cerr << "couldn't find the device" << std::endl;
		return -1;
	}
	std::cout << "Found device! grabbing..." << std::endl;

	libevdev_grab(dev, LIBEVDEV_GRAB);

	struct input_event ev = {};
	int r = 0;
	auto is_error = [](int v) { return v < 0 && v != -EAGAIN; };
	auto has_next_event = [](int v) { return v >= 0; };
	auto flags = LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING;
	while (r = libevdev_next_event(dev, flags, &ev), !is_error(r)) {
		if (!has_next_event(r)) continue;
		if (ev.type == 1) {
			bool isUp = ev.value == 0;
			bool isDown = ev.value == 1;
			if (isDown || isUp) {
				std::cout << ev.code << " " << (isDown ? "down" : "up") << std::endl;
				handle_event(ev.code, isDown);
			}
		}
	}

	libevdev_free(dev);
}