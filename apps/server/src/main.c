#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define usleep(x) Sleep((x)/1000)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

#include "../../../packages/common/protocol.h"
#include "../../../packages/common/physics.h"
#include "../../../packages/simulation/local_game.h"

int sock = -1;
struct sockaddr_in bind_addr;

unsigned int get_server_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void server_net_init() {
    #ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    #endif
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    #ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
    #else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    #endif
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(6969); 
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
    printf("BRAWLPIT SERVER PORT 6969\n");
}

void server_handle_packet(struct sockaddr_in *sender, char *buffer, int size) {
    if (size < sizeof(NetHeader)) return;
    NetHeader *head = (NetHeader*)buffer;
    
    int client_id = -1;
    for(int i=1; i<MAX_CLIENTS; i++) {
        if (local_state.client_active[i] && 
            memcmp(&local_state.clients[i].sin_addr, &sender->sin_addr, sizeof(struct in_addr)) == 0 &&
            local_state.clients[i].sin_port == sender->sin_port) {
            client_id = i;
            break;
        }
    }
    
    if (client_id == -1 && head->type == PACKET_CONNECT) {
        for(int i=1; i<MAX_CLIENTS; i++) {
            if (!local_state.client_active[i]) {
                local_state.client_active[i] = 1;
                local_state.clients[i] = *sender;
                local_state.players[i].active = 1;
                local_state.players[i].stocks = STOCK_COUNT;
                local_state.players[i].id = i;
                local_state.players[i].shield_health = SHIELD_MAX;
                local_state.players[i].damage_percent = 0;
                local_state.players[i].respawn_timer = 0;
                local_state.players[i].ground_platform_type = -1;
                local_state.players[i].drop_through_timer = 0;
                local_state.players[i].wavedash_frames = 0;
                local_state.players[i].dodge_cooldown = 0;
                local_state.players[i].btn_special = 0;
                phys_respawn(&local_state.players[i], get_server_time());
                printf("FIGHTER %d JOINED\n", i);
                
                NetHeader h;
                h.type = PACKET_WELCOME; h.client_id = i;
                // TODO(net): include server-authoritative stage_id in PACKET_WELCOME payload. 
                sendto(sock, (char*)&h, sizeof(NetHeader), 0, (struct sockaddr*)sender, sizeof(struct sockaddr_in));
                break;
            }
        }
    }
    
    if (client_id != -1 && head->type == PACKET_USERCMD) {
        int cursor = sizeof(NetHeader) + 1; 
        if(size >= cursor + sizeof(UserCmd)) {
             UserCmd *cmd = (UserCmd*)(buffer + cursor);
             PlayerState *p = &local_state.players[client_id];
             p->in_x = cmd->stick_x;
             p->in_y = cmd->stick_y;
             p->btn_jump = (cmd->buttons & BTN_JUMP);
             p->btn_attack = (cmd->buttons & BTN_ATTACK);
             p->btn_shield = (cmd->buttons & BTN_SHIELD);
             p->btn_special = (cmd->buttons & BTN_SPECIAL);
        }
    }
}

void server_broadcast() {
    char buffer[4096];
    int cursor = 0;
    NetHeader head;
    head.type = PACKET_SNAPSHOT; head.client_id = 0;
    // TODO(net): include stage_id in snapshots if stage swaps are supported mid-match. 
    head.timestamp = get_server_time();
    
    unsigned char count = 0;
    for(int i=0; i<MAX_CLIENTS; i++) if (local_state.players[i].active) count++;
    head.entity_count = count;
    
    memcpy(buffer + cursor, &head, sizeof(NetHeader)); cursor += sizeof(NetHeader);
    memcpy(buffer + cursor, &count, 1); cursor += 1;
    
    for(int i=0; i<MAX_CLIENTS; i++) {
        PlayerState *p = &local_state.players[i];
        if (p->active) {
            NetPlayer np;
            np.id = (unsigned char)i;
            np.x = p->x; np.y = p->y;
            np.vx = p->vx; np.vy = p->vy;
            np.state = (unsigned char)p->state;
            np.damage = (unsigned short)p->damage_percent;
            np.stocks = (unsigned char)p->stocks;
            np.shield = (unsigned char)p->shield_health;
            np.facing = (p->facing > 0);
            memcpy(buffer + cursor, &np, sizeof(NetPlayer)); cursor += sizeof(NetPlayer);
        }
    }
    
    for(int i=1; i<MAX_CLIENTS; i++) {
        if (local_state.client_active[i]) {
            sendto(sock, buffer, cursor, 0, (struct sockaddr*)&local_state.clients[i], sizeof(struct sockaddr_in));
        }
    }
}

int main() {
    server_net_init();
    local_init_match(1, 0, STAGE_FD, CHARACTER_PETALIA, CHARACTER_VEXAR); 
    
    while(1) {
        char buffer[1024];
        struct sockaddr_in sender;
        socklen_t slen = sizeof(sender);
        int len = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&sender, &slen);
        while (len > 0) {
            server_handle_packet(&sender, buffer, len);
            len = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&sender, &slen);
        }
        
        // Tick
        local_update(0,0,0,0,0,0, NULL, get_server_time());
        server_broadcast();
        
        #ifdef _WIN32
        Sleep(16);
        #else
        usleep(16000);
        #endif
    }
    return 0;
}
