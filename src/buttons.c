/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 old_fatalis */
/* buttons - extra buttons for the raw AYN Odin 3 / AYN Thor gamepad while
 * InputPlumber is masked. It writes into the gamepad's own evdev node, so
 * games and Steam still see one controller.
 *
 *   Odin 3 left back paddle  (BTN_Z on gpio-keys-paddles) -> BTN_SOUTH (A)
 *   Odin 3 right back paddle (BTN_C on gpio-keys-paddles) -> BTN_EAST (B)
 *   bottom right button (BTN_BACK on the gamepad)         -> Guide + A
 *
 * SDL maps no gamepad button to BTN_BACK, so Steam ignores that button on the
 * raw node. Guide + A is Steam's Quick Access Menu chord. The order and the
 * 80 ms steps are the ones InputPlumber uses for its Xbox targets
 * (src/input/target/xpad.rs, write_event, MIN_CHORD_TIME),
 * https://github.com/ShadowBlip/InputPlumber
 *
 * The Thor has no back paddles, so it only gets the Quick Access chord.
 * Exits on any device error so systemd restarts it. */
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PADDLES_NAME "gpio-keys-paddles"
#define CHORD_STEP_MS 80
#define MAX_QUEUED 8

/* Gamepad names set by the device trees, and whether the board has paddles. */
static const struct {
    const char *gamepad;
    int has_paddles;
} boards[] = {
    { "AYN Odin3 Gamepad", 1 }, /* AYN Odin 3 */
    { "AYN Odin2 Gamepad", 0 }, /* AYN Thor */
};

struct queued_key {
    long long due_ms;
    unsigned short code;
    int value;
};

static struct queued_key queue[MAX_QUEUED];
static int queued;

static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

/* Opens the first event node with this name, or returns -1. */
static int find(const char *want, int flags)
{
    char path[64], name[128];
    for (int i = 0; i < 64; i++) {
        snprintf(path, sizeof path, "/sys/class/input/event%d/device/name", i);
        FILE *f = fopen(path, "r");
        if (!f)
            continue;
        if (!fgets(name, sizeof name, f)) {
            fclose(f);
            continue;
        }
        fclose(f);
        name[strcspn(name, "\n")] = 0;
        if (strcmp(name, want) == 0) {
            snprintf(path, sizeof path, "/dev/input/event%d", i);
            return open(path, flags | O_CLOEXEC);
        }
    }
    return -1;
}

static int send_key(int dst, unsigned short code, int value)
{
    struct input_event out[2];
    memset(out, 0, sizeof out);
    out[0].type = EV_KEY;
    out[0].code = code;
    out[0].value = value;
    out[1].type = EV_SYN;
    out[1].code = SYN_REPORT;
    return write(dst, out, sizeof out) == (ssize_t)sizeof out ? 0 : -1;
}

static void queue_key(long long due_ms, unsigned short code, int value)
{
    if (queued < MAX_QUEUED)
        queue[queued++] = (struct queued_key){ due_ms, code, value };
}

/* Press: Guide now, A one step later. Release: A after two steps, Guide after
 * three, so Steam always sees A inside a held Guide. */
static void queue_quick_access(int pressed)
{
    long long t = now_ms();
    if (pressed) {
        queue_key(t, BTN_MODE, 1);
        queue_key(t + CHORD_STEP_MS, BTN_SOUTH, 1);
    } else {
        queue_key(t + 2 * CHORD_STEP_MS, BTN_SOUTH, 0);
        queue_key(t + 3 * CHORD_STEP_MS, BTN_MODE, 0);
    }
}

static int send_due_keys(int dst)
{
    long long t = now_ms();
    int kept = 0;
    for (int i = 0; i < queued; i++) {
        if (queue[i].due_ms <= t) {
            if (send_key(dst, queue[i].code, queue[i].value) < 0)
                return -1;
        } else {
            queue[kept++] = queue[i];
        }
    }
    queued = kept;
    return 0;
}

static int ms_until_next_key(void)
{
    if (!queued)
        return -1;
    long long next = queue[0].due_ms;
    for (int i = 1; i < queued; i++)
        if (queue[i].due_ms < next)
            next = queue[i].due_ms;
    long long wait = next - now_ms();
    return wait > 0 ? (int)wait : 0;
}

static int handle_paddles(int paddles, int dst)
{
    struct input_event in[64];
    ssize_t n = read(paddles, in, sizeof in);
    if (n <= 0)
        return -1;
    for (size_t i = 0; i < (size_t)n / sizeof *in; i++) {
        if (in[i].type != EV_KEY || in[i].value > 1)
            continue;
        if (in[i].code == BTN_Z && send_key(dst, BTN_SOUTH, in[i].value) < 0)
            return -1;
        if (in[i].code == BTN_C && send_key(dst, BTN_EAST, in[i].value) < 0)
            return -1;
    }
    return 0;
}

static int handle_gamepad(int gamepad)
{
    struct input_event in[64];
    ssize_t n = read(gamepad, in, sizeof in);
    if (n <= 0)
        return -1;
    for (size_t i = 0; i < (size_t)n / sizeof *in; i++)
        if (in[i].type == EV_KEY && in[i].code == BTN_BACK && in[i].value <= 1)
            queue_quick_access(in[i].value);
    return 0;
}

int main(void)
{
    int gamepad = -1, paddles = -1;
    for (size_t b = 0; b < sizeof boards / sizeof *boards && gamepad < 0; b++) {
        gamepad = find(boards[b].gamepad, O_RDWR);
        if (gamepad < 0 || !boards[b].has_paddles)
            continue;
        paddles = find(PADDLES_NAME, O_RDONLY);
        if (paddles < 0) {
            fprintf(stderr, "%s not found yet\n", PADDLES_NAME);
            return 1;
        }
    }
    if (gamepad < 0) {
        fprintf(stderr, "no AYN Odin 3 or Thor gamepad found yet\n");
        return 1;
    }
    struct pollfd fds[2] = {
        { .fd = gamepad, .events = POLLIN },
        { .fd = paddles, .events = POLLIN }, /* poll ignores a negative fd */
    };
    for (;;) {
        if (poll(fds, 2, ms_until_next_key()) < 0)
            return 1;
        if ((fds[0].revents | fds[1].revents) & (POLLERR | POLLHUP | POLLNVAL))
            return 1;
        if ((fds[0].revents & POLLIN) && handle_gamepad(gamepad) < 0)
            return 1;
        if ((fds[1].revents & POLLIN) && handle_paddles(paddles, gamepad) < 0)
            return 1;
        if (send_due_keys(gamepad) < 0)
            return 1;
    }
}
