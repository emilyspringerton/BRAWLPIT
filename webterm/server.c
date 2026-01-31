// Minimal WebSocket shell server (C99)
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PORT 8080
#define BUF 4096

static const char *WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static void fatal(const char *msg) {
    perror(msg);
    exit(1);
}

static int write_all(int fd, const void *buf, size_t len) {
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t written = write(fd, ptr + total, len - total);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)written;
    }
    return 0;
}

static void sha1_transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)block[i * 4] << 24;
        w[i] |= (uint32_t)block[i * 4 + 1] << 16;
        w[i] |= (uint32_t)block[i * 4 + 2] << 8;
        w[i] |= (uint32_t)block[i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) {
        uint32_t v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
        w[i] = (v << 1) | (v >> 31);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d;
        d = c;
        c = (b << 30) | (b >> 2);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void sha1(const uint8_t *data, size_t len, uint8_t out[20]) {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint64_t bitlen = (uint64_t)len * 8;
    uint8_t block[64];

    size_t offset = 0;
    while (len - offset >= 64) {
        memcpy(block, data + offset, 64);
        sha1_transform(state, block);
        offset += 64;
    }

    size_t rem = len - offset;
    memset(block, 0, sizeof(block));
    if (rem > 0) {
        memcpy(block, data + offset, rem);
    }
    block[rem] = 0x80;
    if (rem >= 56) {
        sha1_transform(state, block);
        memset(block, 0, sizeof(block));
    }
    block[56] = (uint8_t)(bitlen >> 56);
    block[57] = (uint8_t)(bitlen >> 48);
    block[58] = (uint8_t)(bitlen >> 40);
    block[59] = (uint8_t)(bitlen >> 32);
    block[60] = (uint8_t)(bitlen >> 24);
    block[61] = (uint8_t)(bitlen >> 16);
    block[62] = (uint8_t)(bitlen >> 8);
    block[63] = (uint8_t)(bitlen);
    sha1_transform(state, block);

    for (int i = 0; i < 5; i++) {
        out[i * 4] = (uint8_t)(state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(state[i]);
    }
}

static void base64_encode(const uint8_t *in, size_t len, char *out, size_t out_len) {
    static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t pos = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t octet_a = i < len ? in[i] : 0;
        uint32_t octet_b = (i + 1) < len ? in[i + 1] : 0;
        uint32_t octet_c = (i + 2) < len ? in[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        if (pos + 4 >= out_len) {
            break;
        }
        out[pos++] = b64[(triple >> 18) & 0x3F];
        out[pos++] = b64[(triple >> 12) & 0x3F];
        out[pos++] = (i + 1) < len ? b64[(triple >> 6) & 0x3F] : '=';
        out[pos++] = (i + 2) < len ? b64[triple & 0x3F] : '=';
    }
    out[pos] = '\0';
}

static int ws_handshake(int client_fd) {
    char req[BUF];
    ssize_t n = read(client_fd, req, sizeof(req) - 1);
    if (n <= 0) {
        return -1;
    }
    req[n] = '\0';

    const char *key_hdr = "Sec-WebSocket-Key:";
    char *key_loc = strstr(req, key_hdr);
    if (!key_loc) {
        return -1;
    }

    key_loc += strlen(key_hdr);
    while (*key_loc == ' ') {
        key_loc++;
    }
    char *key_end = strstr(key_loc, "\r\n");
    if (!key_end) {
        return -1;
    }

    char client_key[128];
    size_t key_len = (size_t)(key_end - key_loc);
    if (key_len >= sizeof(client_key)) {
        return -1;
    }
    memcpy(client_key, key_loc, key_len);
    client_key[key_len] = '\0';

    char accept_src[256];
    snprintf(accept_src, sizeof(accept_src), "%s%s", client_key, WS_GUID);

    uint8_t digest[20];
    sha1((const uint8_t *)accept_src, strlen(accept_src), digest);

    char accept_key[64];
    base64_encode(digest, sizeof(digest), accept_key, sizeof(accept_key));

    char response[256];
    int response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_key);

    if (write_all(client_fd, response, (size_t)response_len) < 0) {
        return -1;
    }

    return 0;
}

static int ws_read(int fd, char *out, size_t out_size) {
    uint8_t hdr[2];
    ssize_t n = read(fd, hdr, 2);
    if (n <= 0) {
        return -1;
    }
    if (n < 2) {
        return -1;
    }

    uint8_t opcode = hdr[0] & 0x0F;
    uint8_t masked = hdr[1] & 0x80;
    uint64_t len = hdr[1] & 0x7F;

    if (opcode == 0x8) {
        return -1;
    }

    if (len == 126) {
        uint8_t ext[2];
        if (read(fd, ext, 2) != 2) {
            return -1;
        }
        len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (len == 127) {
        uint8_t ext[8];
        if (read(fd, ext, 8) != 8) {
            return -1;
        }
        len = 0;
        for (int i = 0; i < 8; i++) {
            len = (len << 8) | ext[i];
        }
    }

    uint8_t mask[4] = {0};
    if (masked) {
        if (read(fd, mask, 4) != 4) {
            return -1;
        }
    }

    if (len >= out_size) {
        len = out_size - 1;
    }

    size_t total = 0;
    while (total < len) {
        ssize_t got = read(fd, out + total, len - total);
        if (got <= 0) {
            return -1;
        }
        total += (size_t)got;
    }

    if (masked) {
        for (size_t i = 0; i < total; i++) {
            out[i] ^= mask[i % 4];
        }
    }
    out[total] = '\0';

    return (int)total;
}

static int ws_write(int fd, const char *msg, size_t len) {
    uint8_t header[10];
    size_t header_len = 0;

    header[0] = 0x81;
    if (len <= 125) {
        header[1] = (uint8_t)len;
        header_len = 2;
    } else if (len <= 65535) {
        header[1] = 126;
        header[2] = (uint8_t)(len >> 8);
        header[3] = (uint8_t)(len & 0xFF);
        header_len = 4;
    } else {
        header[1] = 127;
        for (int i = 0; i < 8; i++) {
            header[2 + i] = (uint8_t)((len >> (56 - 8 * i)) & 0xFF);
        }
        header_len = 10;
    }

    if (write_all(fd, header, header_len) < 0) {
        return -1;
    }
    if (write_all(fd, msg, len) < 0) {
        return -1;
    }
    return 0;
}

int main(void) {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        fatal("socket");
    }

    int opt = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fatal("setsockopt");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (void *)&addr, sizeof(addr)) < 0) {
        fatal("bind");
    }
    if (listen(server, 1) < 0) {
        fatal("listen");
    }

    printf("webterm listening on http://localhost:%d\n", PORT);
    fflush(stdout);

    int client = accept(server, NULL, NULL);
    if (client < 0) {
        fatal("accept");
    }

    if (ws_handshake(client) < 0) {
        fatal("handshake");
    }

    int in_pipe[2];
    int out_pipe[2];
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
        fatal("pipe");
    }

    pid_t pid = fork();
    if (pid < 0) {
        fatal("fork");
    }
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[1]);
        close(out_pipe[0]);
        execl("/bin/sh", "sh", NULL);
        _exit(1);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    char buf[BUF];
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client, &readfds);
        FD_SET(out_pipe[0], &readfds);

        int maxfd = client > out_pipe[0] ? client : out_pipe[0];
        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (FD_ISSET(client, &readfds)) {
            int n = ws_read(client, buf, sizeof(buf));
            if (n <= 0) {
                break;
            }
            if (write_all(in_pipe[1], buf, (size_t)n) < 0) {
                break;
            }
            if (write_all(in_pipe[1], "\n", 1) < 0) {
                break;
            }
        }

        if (FD_ISSET(out_pipe[0], &readfds)) {
            ssize_t r = read(out_pipe[0], buf, sizeof(buf) - 1);
            if (r > 0) {
                buf[r] = '\0';
                if (ws_write(client, buf, (size_t)r) < 0) {
                    break;
                }
            }
        }
    }

    close(client);
    close(server);
    close(in_pipe[1]);
    close(out_pipe[0]);
    waitpid(pid, NULL, 0);

    return 0;
}
