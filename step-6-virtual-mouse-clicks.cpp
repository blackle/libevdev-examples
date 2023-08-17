#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <iostream>
#include <string>
#include <mutex>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>

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

VirtualMouse g_mouse;

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

void process_key(int code, bool is_down) {
	if (code == 82) g_mouse.Click(BTN_LEFT, is_down);
	if (code == 96) g_mouse.Click(BTN_RIGHT, is_down);
	if (code == 83) g_mouse.Click(BTN_MIDDLE, is_down);
}

void process_events(struct libevdev *dev) {
	struct input_event ev = {};
	int status = 0;
	auto is_error = [](int v) { return v < 0 && v != -EAGAIN; };
	auto has_next_event = [](int v) { return v >= 0; };
	const auto flags = LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING;

	while (status = libevdev_next_event(dev, flags, &ev), !is_error(status)) {
		if (!has_next_event(status)) continue;

		if (ev.type == 1) {
			bool is_up = ev.value == 0;
			bool is_down = ev.value == 1;
			if (is_down || is_up) { // excludes autorepeat
				process_key(ev.code, is_down);
			}
		}
	}
}

int main() {
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

	struct libevdev *dev = find_device_by_name("Usb KeyBoard Usb KeyBoard");

	if (dev == nullptr) {
		std::cerr << "Couldn't find device!" << std::endl;
		return -1;
	}

	// must init mouse before we drop permissions
	if (g_mouse.Init() != 0) {
		std::cerr << "couldn't init mouse!" << std::endl;
		return -1;
	}

	//drop back into old permissions
	if (setgid(oldgid) < 0) {
		std::cerr << "couldn't switch back to old group!" << std::endl;
		return -1;
	}

	libevdev_grab(dev, LIBEVDEV_GRAB);

	process_events(dev);

	libevdev_free(dev);
	return 0;
}