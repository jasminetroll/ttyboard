#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <libevdev/libevdev-uinput.h>
#include "keymap.h"

struct libevdev *dev = 0;
struct libevdev_uinput *uidev = 0;
struct termios tios_saved;
int tios_fd = -1;

int writekey(unsigned int code, int value) {
    int err;
    if ((err = libevdev_uinput_write_event(uidev, EV_KEY, code, value))) {
        fprintf(stderr, "libevdev_uinput_write_event(uidev, EV_KEY, %d, %d: %s\n", code, value, strerror(-err));
        return 1;
    }
    if ((err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0))) {
        fprintf(stderr, "libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0): %s\n", strerror(-err));
        return 1;
    }
    return 0;
}

int keydown(unsigned int code) {
    return writekey(code, 1);
}

int keyup(unsigned int code) {
    return writekey(code, 0);
}

int keypress(unsigned int code) {
    return keydown(code) || keyup(code);
}

int writekeychar(char c) {
    keydef k;
    if (c < 0 || c > 127) {
        return 0;
    }
    k = keymap[(int)c];
    if (k.control) {
        if (keydown(KEY_LEFTCTRL)) {
            return 1;
        }
    }
    if (k.shift) {
        if (keydown(KEY_LEFTSHIFT)) {
            return 1;
        }
    }
    if (keypress(k.code)) {
        return 1;
    }
    if (k.shift) {
        if (keyup(KEY_LEFTSHIFT)) {
            return 1;
        }
    }
    if (k.control) {
        if (keyup(KEY_LEFTCTRL)) {
            return 1;
        }
    }
    return 0;
}

void cleanup(void) {
    if (tios_fd != -1) {
        tcsetattr(tios_fd, TCSAFLUSH, &tios_saved);
    }
    if (uidev) {
        libevdev_uinput_destroy(uidev);
    }
    if (dev) {
        libevdev_free(dev);
    }
}

int main(int argc, char **argv) {
    int err;
    char c;
    if (atexit(cleanup)) {
        fputs("atexit: failed", stderr);
        return 1;
    }
    if (!(dev = libevdev_new())) {
        perror("libevdev_new");
        return 1;
    }
    libevdev_set_name(dev, "ttyboard");
    if (libevdev_enable_event_type(dev, EV_KEY)) {
        fputs("libevdev_enable_event_type: failed", stderr);
        return 1;
    }
    for (unsigned int i = 0; i < 255; ++i) {
        if (libevdev_enable_event_code(dev, EV_KEY, i, NULL)) {
            fprintf(stderr, "libevdev_enable_event_code: failed (code %d)\n", i);
            return 1;
        }
    }
    if ((err = libevdev_uinput_create_from_device(
                                                  dev,
                                                  LIBEVDEV_UINPUT_OPEN_MANAGED,
                                                  &uidev))) {
        fprintf(stderr, "libevdev_uinput_create_from_device: %s\n", strerror(-err));
        return 1;
    }
    if (isatty(STDIN_FILENO)) {
        struct termios tios;
        if (tcgetattr(STDIN_FILENO, &tios)) {
            perror("tcgetattr");
            return 1;
        }
        tios_saved = tios;
        tios_fd = STDIN_FILENO;
        tios.c_lflag &= ~(ECHO | ICANON);
        if (tcsetattr(tios_fd, TCSAFLUSH, &tios)) {
            perror("tcsetattr");
            return 1;
        }
    }
    while (1) {
        switch (read(STDIN_FILENO, &c, 1)) {
        case 0:
            return 0;
        case 1:
            if (writekeychar(c)) {
                return 1;
            }
            break;
        default:
            perror("read");
            return 1;
        }
    }
    return 0;
}
