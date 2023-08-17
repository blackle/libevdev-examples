#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
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
#include <memory>
#include <set>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <math.h>

class VirtualMouse {
public:
	struct libevdev_uinput* m_uinput = nullptr;
	std::mutex m_mouseMutex;
public:
	VirtualMouse() {}
	~VirtualMouse() {
		libevdev_uinput_destroy(m_uinput);
	}

	int Init() {
		struct libevdev* dev = libevdev_new();
		libevdev_set_name(dev, "Virtual Mouse");

		libevdev_enable_property(dev, INPUT_PROP_POINTER);

		libevdev_enable_event_type(dev, EV_REL);
		libevdev_enable_event_code(dev, EV_REL, REL_X, nullptr);
		libevdev_enable_event_code(dev, EV_REL, REL_Y, nullptr);
		libevdev_enable_event_code(dev, EV_REL, REL_WHEEL, nullptr);

		libevdev_enable_event_type(dev, EV_KEY);
		libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, nullptr);
		libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, nullptr);
		libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, nullptr);

		int r = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &m_uinput);
		libevdev_free(dev);
		return r;
	}

	void Move(int rx, int ry) {
		std::lock_guard<std::mutex> guard(m_mouseMutex);
		libevdev_uinput_write_event(m_uinput, EV_REL, REL_X, rx);
		libevdev_uinput_write_event(m_uinput, EV_REL, REL_Y, ry);
		libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0);
	}

	void Scroll(int rs) {
		std::lock_guard<std::mutex> guard(m_mouseMutex);
		libevdev_uinput_write_event(m_uinput, EV_REL, REL_WHEEL, rs);
		libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0);
	}

	void Click(int btn, bool isDown) {
		std::lock_guard<std::mutex> guard(m_mouseMutex);
		libevdev_uinput_write_event(m_uinput, EV_KEY, btn, isDown);
		libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0);
	}
};

std::set<int> g_pressedKeys;
std::mutex g_pressedKeysMutex;

std::atomic_bool g_runMouseThread;
void mouse_thread(VirtualMouse* mouse) {
	float rx = 0;
	float ry = 0;
	const float friction = 0.85;
	const float accel = 1.2/friction;
	while (g_runMouseThread) {
		float dx = 0;
		float dy = 0;

		float rs = 0;

		{
			std::lock_guard<std::mutex> guard(g_pressedKeysMutex);
			if (g_pressedKeys.count(77) > 0) rx += accel;
			if (g_pressedKeys.count(75) > 0) rx -= accel;
			if (g_pressedKeys.count(76) > 0) ry += accel;
			if (g_pressedKeys.count(72) > 0) ry -= accel;

			if (g_pressedKeys.count(78) > 0) rs += 1;
			if (g_pressedKeys.count(14) > 0) rs -= 1;
		}

		if (fabs(dx)+fabs(dy) > accel) {
			dx *= .7;
			dy *= .7;
		}

		rx += dx;
		ry += dy;
		rx *= friction;
		ry *= friction;


		if (fabs(rx)+fabs(ry) > 0) {
			mouse->Move(rx, ry);
		}
		if (fabs(rs) > 0) {
			mouse->Scroll(rs);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void handle_event(int code, bool isDown, VirtualMouse* mouse) {
	std::lock_guard<std::mutex> guard(g_pressedKeysMutex);
	if (isDown) {
		g_pressedKeys.insert(code);
	} else {
		g_pressedKeys.erase(code);
	}
	if (code == 82) mouse->Click(BTN_LEFT, isDown);
	if (code == 96) mouse->Click(BTN_RIGHT, isDown);
	if (code == 83) mouse->Click(BTN_MIDDLE, isDown);

	// if (code == 78) mouse->Scroll(1);
	// if (code == 14) mouse->Scroll(-1);
	// if (isDown) {
	// 	if (code == 69) {
	// 		// system("xterm &");
	// 	}
	// 	if (code == 77) {
	// 		system("xdotool mousemove_relative -- 10 0 &");
	// 	}
	// 	if (code == 75) {
	// 		system("xdotool mousemove_relative -- -10 0 &");
	// 	}
	// 	if (code == 80) {
	// 		system("xdotool mousemove_relative -- 0 10 &");
	// 	}
	// 	if (code == 72) {
	// 		system("xdotool mousemove_relative -- 0 -10 &");
	// 	}
	// }
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
			dev = nullptr;
		}
		close(fd);
	}

	VirtualMouse mouse;
	if (mouse.Init() != 0) {
		std::cerr << "couldn't init mouse!" << std::endl;
		return -1;
	}

	//drop back into old permissions
	if (setgid(oldgid) < 0) {
		std::cerr << "couldn't switch back to old group!" << std::endl;
		return -1;
	}

	g_runMouseThread = true;
	std::thread mouseThread(mouse_thread, &mouse);

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
	const auto flags = LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING;
	while (r = libevdev_next_event(dev, flags, &ev), !is_error(r)) {
		if (!has_next_event(r)) continue;
		if (ev.type == 1) {
			bool isUp = ev.value == 0;
			bool isDown = ev.value == 1;
			if (isDown || isUp) {
				std::cout << ev.code << " " << (isDown ? "down" : "up") << std::endl;
				handle_event(ev.code, isDown, &mouse);
			}
		}
	}

	libevdev_free(dev);

	g_runMouseThread = false;
	mouseThread.join();
}