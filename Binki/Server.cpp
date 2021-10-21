#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#define PATH_MAX MAX_PATH
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <limits.h>
#include <arpa/inet.h>
#define _w_inet_ntop inet_ntop 
#define _w_inet_pton inet_pton
#endif
#include <openssl/md5.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include "Server.h"
#include "Config.h"
#include "../Common/INIReader.h"

static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
    ltrim(s);
    rtrim(s);
}

Server::Server() {
    cram5_init = false;
    cram5_opt = false;
    current_file = NULL;
#ifdef _MSC_VER
	winsock_init = false;
    socket = -1;
#endif    
}

void Server::cram5_init_challenge_data() {
    std::stringstream data;

    data << "BINKI " << BINKI_VERSION << " " << rand() << " " << time(NULL);
    MD5_CTX ctx;
    MD5_Init(&ctx);

    unsigned char hash[16];
    MD5_Update(&ctx, data.str().c_str(), data.str().size());
    MD5_Final(hash, &ctx);

    std::stringstream ss;
    for (int i = 0; i < 16; i++) {
        ss << std::setw(2) << std::setfill('0') << std::hex << (int)hash[i];
    }
    cram5_challenge_data = ss.str();
    cram5_init = true;
}

bool Server::cram5_validate_password(std::string challenge, std::string password, std::string hash) {
    std::string expected = cram5_create_hashed_pwd(challenge, password);

    return (expected == hash);
}

std::string Server::cram5_create_hashed_pwd(std::string challenge_hex, std::string password) {
    MD5_CTX ctx;
    std::stringstream ss;

    if (!cram5_init) {
        cram5_init_challenge_data();
    }
    trim(challenge_hex);
    char result[128];
    auto len = 0;

    for (int i = 0; i < challenge_hex.size(); i+=2) {
        std::string s;
        s.push_back(challenge_hex[i]);
        s.push_back(challenge_hex[i+1]);
        const auto chl = strtoul(s.c_str(), NULL, 16);
        const char ch = chl & 0xff;
        result[len++] = ch;
    }
    std::string challenge(result, len);
    std::string secret;

    if (password.size() > 64) {
        MD5_Init(&ctx);

        unsigned char hash[16];
        MD5_Update(&ctx, password.c_str(), password.size());
        MD5_Final(hash, &ctx);
        secret = std::string((char*)hash, 16);
    }
    else {
        secret = password;
    }
    uint8_t ipad = 0x36;
    uint8_t opad = 0x5c;

    unsigned char ip[65];
    unsigned char op[65];

    memset(ip, 0, 65);
    memset(op, 0, 65);

    memcpy(ip, secret.c_str(), secret.size());
    memcpy(op, secret.c_str(), secret.size());
    for (int i = 0; i < 65; i++) {
        ip[i] ^= ipad;
        op[i] ^= opad;
    }

    unsigned char digest[16];
    MD5_Init(&ctx);
    MD5_Update(&ctx, ip, 64);
    MD5_Update(&ctx, challenge.c_str(), challenge.size());
    MD5_Final(digest, &ctx);

    MD5_Init(&ctx);
    MD5_Update(&ctx, op, 64);
    MD5_Update(&ctx, digest, 16);
    MD5_Final(digest, &ctx);
    for (int i = 0; i < 16; i++) {
        ss << std::setw(2) << std::setfill('0') << std::hex << (int)digest[i];
    }

    return ss.str();
}

struct outfile_t {
    std::filesystem::path file;
    bool del;
    bool trunc;
    std::string flo;
    bool sent = false;
};

void remove_from_flo(struct outfile_t o) {
    FILE* fptr1;
    FILE* fptr2;

    char buffer[PATH_MAX];

    bool empty = true;

    snprintf(buffer, PATH_MAX, "%s.b", o.flo.c_str());

    std::string backupf(buffer);

    fptr1 = fopen(o.flo.c_str(), "r");
    fptr2 = fopen(backupf.c_str(), "w");

    while (fgets(buffer, PATH_MAX, fptr1)) {
        if (buffer[0] == '^' || buffer[0] == '~' || buffer[0] == '!' || buffer[0] == '@') {
            if (strncmp(&buffer[1], o.file.u8string().c_str(), o.file.u8string().size()) == 0) {
                continue;
            }
            else {
                fprintf(fptr2, "%s", buffer);
                empty = false;
            }
        }
        else {
            if (strncmp(buffer, o.file.u8string().c_str(), o.file.u8string().size()) == 0) {
                continue;
            }
            else {
                fprintf(fptr2, "%s", buffer);
                empty = false;
            }
        }
    }

    fclose(fptr1);
    fclose(fptr2);
    std::filesystem::remove(o.flo);
    if (!empty) {
        std::filesystem::rename(o.flo + ".b", o.flo);
    }
    else {
        std::filesystem::remove(o.flo + ".b");
    }
    
}

bool Server::send_data_packet(int len, char* data) {
    char* buffer;
    buffer = (char *)malloc(len + 2);

    if (!buffer) {
        std::cerr << "Out of Memory!" << std::endl;
        exit(-1);
    }

    len &= 0x7FFF;
    buffer[0] = (uint8_t)((len & 0xff00) >> 8);
    buffer[1] = (uint8_t)(len & 0x00ff);
    memcpy(&buffer[2], data, len);

    send(socket, buffer, len + 2, 0);

    free(buffer);
    return true;
}

bool Server::send_file_packet(std::filesystem::path file) {
    std::stringstream filecmd;
    time_t ts = time(NULL);

    FILE* fptr = fopen(file.u8string().c_str(), "rb");


    if (!fptr) {
        return false;
    }
    sending_filename = file.filename().u8string();
    sending_len = std::filesystem::file_size(file);
    sending_timestamp = ts;

    filecmd << file.filename().u8string() << " " << std::filesystem::file_size(file) << " " << ts << " 0";
    send_command_packet(M_FILE, filecmd.str());
    process_frames(1, 0xFF);
    char* buffer;
    buffer = (char*)malloc(16384);
    if (buffer == NULL) {
        std::cerr << "Out of Memory!" << std::endl;
        exit(-1);
    }

    for (size_t i = 0; i < std::filesystem::file_size(file); i += 16384) {
        int len = 0;

        if (i + 16384 > std::filesystem::file_size(file)) {
            len = fread(buffer, 1, std::filesystem::file_size(file) - i, fptr);
        }
        else {
            len = fread(buffer, 1, 16384, fptr);
        }
        send_data_packet(len, buffer);

        if (!process_frames(1, 0xFF)) {
            free(buffer);
            fclose(fptr);
            return false;
        }
    }

    fclose(fptr);

    free(buffer);

    return true;
}

bool Server::transfer_files(std::string domain, NETADDR *theirnode, std::filesystem::path dir, std::filesystem::path outbox) {
    std::vector<struct outfile_t> files;

    for (struct address_t a : c.addresses) {
        if (a.domain == domain) {
            std::stringstream flowfname;
            char buffer[13];
            struct outfile_t outf;


            flowfname.str("");

            outf.flo = "";

            if (theirnode->point != 0) {
                snprintf(buffer, sizeof buffer, "%08x", theirnode->point);
                flowfname << buffer;
            }
            else {
                snprintf(buffer, sizeof buffer, "%04x%04x", theirnode->net, theirnode->node);
                flowfname << buffer;
            }
            if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".cut")) {
                outf.file = std::filesystem::path(dir.u8string() + "/" + flowfname.str() + ".cut");
                outf.del = true;
                outf.trunc = false;
                files.push_back(outf);
            }
            if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".out")) {
                outf.file = std::filesystem::path(dir.u8string() + "/" + flowfname.str() + ".out");
                outf.del = true;
                outf.trunc = false;
                files.push_back(outf);
            }
            if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".hut")) {
                outf.file = std::filesystem::path(dir.u8string() + "/" + flowfname.str() + ".hut");
                outf.del = true;
                outf.trunc = false;
                files.push_back(outf);
            }
            if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".clo")) {
                std::ifstream is(dir.u8string() + "/" + flowfname.str() + ".clo");
                std::string str;
                while (getline(is, str))
                {
                    if (str[0] == '^' || str[0] == '-') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = true;
                        outf.trunc = false;
                    }
                    else if (str[0] == '~' || str[0] == '!') {
                        // skip
                        continue;
                    }
                    else if (str[0] == '#') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = false;
                        outf.trunc = true;
                    }
                    else if (str[0] == '@') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = false;
                        outf.trunc = false;
                    }
                    else {
                        outf.file = std::filesystem::path(str);
                        outf.del = false;
                        outf.trunc = false;
                    }
                    outf.flo = dir.u8string() + "/" + flowfname.str() + ".clo";
                    files.push_back(outf);
                }
                is.close();
                
            } else if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".hlo")) {
                std::ifstream is(dir.u8string() + "/" + flowfname.str() + ".hlo");
                std::string str;
                while (getline(is, str))
                {
                    if (str[0] == '^' || str[0] == '-') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = true;
                        outf.trunc = false;
                    }
                    else if (str[0] == '~' || str[0] == '!') {
                        // skip
                        continue;
                    }
                    else if (str[0] == '#') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = false;
                        outf.trunc = true;
                    }
                    else if (str[0] == '@') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = false;
                        outf.trunc = false;
                    }
                    else {
                        outf.file = std::filesystem::path(str);
                        outf.del = false;
                        outf.trunc = false;
                    }
                    outf.flo = dir.u8string() + "/" + flowfname.str() + ".hlo";
                    files.push_back(outf);
                }
                is.close();
               
            } else if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".flo")) {
                std::ifstream is(dir.u8string() + "/" + flowfname.str() + ".flo");
                std::string str;
                while (getline(is, str))
                {
                    if (str[0] == '^' || str[0] == '-') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = true;
                        outf.trunc = false;
                    }
                    else if (str[0] == '~' || str[0] == '!') {
                        // skip
                        continue;
                    }
                    else if (str[0] == '#') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = false;
                        outf.trunc = true;
                    }
                    else if (str[0] == '@') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = false;
                        outf.trunc = false;
                    }
                    else {
                        outf.file = std::filesystem::path(str);
                        outf.del = false;
                        outf.trunc = false;
                    }
                    outf.flo = dir.u8string() + "/" + flowfname.str() + ".flo";
                    files.push_back(outf);
                }
                is.close();
                
            }
#ifndef _MSC_VER
            flowfname.str("");
            outf.flo = "";
            if (theirnode->point != 0) {
                snprintf(buffer, sizeof buffer, "%08X", theirnode->point);
                flowfname << buffer;
            }
            else {
                snprintf(buffer, sizeof buffer, "%04X%04X", theirnode->net, theirnode->node);
                flowfname << buffer;
            }
            if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".CUT")) {
                outf.file = std::filesystem::path(dir.u8string() + "/" + flowfname.str() + ".CUT");
                outf.del = true;
                outf.trunc = false;
                files.push_back(outf);
            }
            if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".OUT")) {
                outf.file = std::filesystem::path(dir.u8string() + "/" + flowfname.str() + ".OUT");
                outf.del = true;
                outf.trunc = false;
                files.push_back(outf);
            }
            if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".HUT")) {
                outf.file = std::filesystem::path(dir.u8string() + "/" + flowfname.str() + ".HUT");
                outf.del = true;
                outf.trunc = false;
                files.push_back(outf);
            }
            if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".CLO")) {
                std::ifstream is(dir.u8string() + "/" + flowfname.str() + ".CLO");
                std::string str;
                while (getline(is, str))
                {
                    if (str[0] == '^' || str[0] == '-') {
                        outf.file = std::filesystem::path(str.substr(1));
                        outf.del = true;
                        outf.trunc = false;
                    }
                    else if (str[0] == '~' || str[0] == '!') {
                    // skip
                    continue;
                    }
                    else if (str[0] == '#') {
                    outf.file = std::filesystem::path(str.substr(1));
                    outf.del = false;
                    outf.trunc = true;
                    }
                    else if (str[0] == '@') {
                    outf.file = std::filesystem::path(str.substr(1));
                    outf.del = false;
                    outf.trunc = false;
                    }
                    else {
                    outf.file = std::filesystem::path(str);
                    outf.del = false;
                    outf.trunc = false;
                    }
                    outf.flo = dir.u8string() + "/" + flowfname.str() + ".CLO";
                    files.push_back(outf);
                }
                is.close();

            }
            else if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".HLO")) {
            std::ifstream is(dir.u8string() + "/" + flowfname.str() + ".HLO");
            std::string str;
            while (getline(is, str))
            {
                if (str[0] == '^' || str[0] == '-') {
                    outf.file = std::filesystem::path(str.substr(1));
                    outf.del = true;
                    outf.trunc = false;
                }
                else if (str[0] == '~' || str[0] == '!') {
                    // skip
                    continue;
                }
                else if (str[0] == '#') {
                    outf.file = std::filesystem::path(str.substr(1));
                    outf.del = false;
                    outf.trunc = true;
                }
                else if (str[0] == '@') {
                    outf.file = std::filesystem::path(str.substr(1));
                    outf.del = false;
                    outf.trunc = false;
                }
                else {
                    outf.file = std::filesystem::path(str);
                    outf.del = false;
                    outf.trunc = false;
                }
                outf.flo = dir.u8string() + "/" + flowfname.str() + ".HLO";
                files.push_back(outf);
            }
            is.close();

            }
            else if (std::filesystem::exists(dir.u8string() + "/" + flowfname.str() + ".FLO")) {
            std::ifstream is(dir.u8string() + "/" + flowfname.str() + ".FLO");
            std::string str;
            while (getline(is, str))
            {
                if (str[0] == '^' || str[0] == '-') {
                    outf.file = std::filesystem::path(str.substr(1));
                    outf.del = true;
                    outf.trunc = false;
                }
                else if (str[0] == '~' || str[0] == '!') {
                    // skip
                    continue;
                }
                else if (str[0] == '#') {
                    outf.file = std::filesystem::path(str.substr(1));
                    outf.del = false;
                    outf.trunc = true;
                }
                else if (str[0] == '@') {
                    outf.file = std::filesystem::path(str.substr(1));
                    outf.del = false;
                    outf.trunc = false;
                }
                else {
                    outf.file = std::filesystem::path(str);
                    outf.del = false;
                    outf.trunc = false;
                }
                outf.flo = dir.u8string() + "/" + flowfname.str() + ".FLO";
                files.push_back(outf);
            }
            is.close();

            }
#endif
        }
    }

    for (auto const& dir : std::filesystem::directory_iterator{ outbox }) {
        struct outfile_t outf;

        outf.file = std::filesystem::absolute(dir.path());
        outf.del = true;
        outf.flo = "";
        outf.sent = false;
        outf.trunc = false;

        files.push_back(outf);
    }

    // transfer files
    for (size_t i = 0; i < files.size(); i++) {
        while (sending_filename != "") {
            if (!process_frames(1, M_GOT)) {
                return false;
            }
        }
        if (!send_file_packet(files.at(i).file)) {
            break;
        }
        files.at(i).sent = true;
    }

    while (sending_filename != "") {
        if (!process_frames(1, M_GOT)) {
            return false;
        }
    }

    for (size_t i = 0; i < files.size(); i++) {
        if (files.at(i).sent) {
            if (files.at(i).del) {
                std::filesystem::remove(files.at(i).file);
            }
            if (files.at(i).flo != "") {
                remove_from_flo(files.at(i));
            }
        }
    }

    return true;
}

int Server::receive(int socket, char* buffer, int size, int timeout) {
    int len = 0;
    struct timeval tv;

    while (len < size) {
        fd_set rfd;
        FD_ZERO(&rfd);
        FD_SET(socket, &rfd);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        int rs = select(socket + 1, &rfd, NULL, NULL, &tv);
#if _MSC_VER
        int err = WSAGetLastError();
#else
        int err = errno;
#endif
        if (rs == 0) {
            return 0;
        }
        else if (rs == -1 && errno != EINTR) {
            return -1;
        }
        else if (FD_ISSET(socket, &rfd)) {
            int r = recv(socket, &buffer[len], size - len, 0);
            if (r > 0) {
                len += r;
            }
            else {
                return -1;
            }
        }
    }

    return len;
}

bool Server::process_data(uint16_t header, int timeout) {
    char* data = (char*)malloc(header);
    if (!data) {
        std::cerr << "Out of memory!" << std::endl;
        exit(-1);
    }

    memset(data, 0, header);
    if (receive(socket, data, header, timeout) != header) {
        free(data);
        return false;
    }

    if (current_file != NULL) {
        fwrite(data, 1, header, current_file);
    }
    else {
        std::cerr << "Current file = NULL! " << std::endl;
    }

    current_received += header;

    if (current_received >= current_len) {
        if (current_file != NULL) {
            fclose(current_file);
            current_file = NULL;

            send_command_packet(M_GOT, current_filename + " " + std::to_string(current_received) + " " + std::to_string(current_timestamp));
            if (c.semaphore != "") {
                FILE* sem = fopen(c.semaphore.c_str(), "w");
                if (sem) {
                    fprintf(sem, "Got File!\n");
                    fclose(sem);
                }
            }
        }
    }
    return true;
}

void Server::cleanup() {
    if (socket != -1) {
#ifdef _MSC_VER
        int ret = shutdown(socket, SD_BOTH);
        if (ret == 0) {
            closesocket(socket);
        }
        else {
            ret = WSAGetLastError();
            if (ret == WSAECONNABORTED || ret == WSAECONNRESET) {
                closesocket(socket);
            }
        }
#else
        if (shutdown(socket, SHUT_RDWR) == 0) {
            close(socket);
        }
#endif
    }
#ifdef _MSC_VER
    if (winsock_init) {
        WSACleanup();
    }
#endif
}

uint8_t Server::process_command(uint16_t header, int timeout) {
    uint8_t cmd;
    if (receive(socket, (char*)&cmd, 1, timeout) != 1) {
        return 0xff;
    }
    char* data = NULL;
    if (header > 1) {
        data = (char*)malloc(header);

        if (!data) {
            std::cerr << "Out of memory!" << std::endl;
            exit(-1);
        }
        memset(data, 0, header);

        int len = receive(socket, data, header - 1, timeout);
        if (len != header - 1) {
            free(data);
            return 0xff;
        }
    }

    if (data == NULL) {
        log.log(LOG_INFO, ">>> %s: No Data", commands[cmd]);
    }
    else {
        if (cmd == M_PWD) {
            log.log(LOG_INFO, ">>> %s: ************", commands[cmd]);
        }
        else {
            log.log(LOG_INFO, ">>> %s: %s", commands[cmd], data);
        }
    }

    switch (cmd) {
    case M_ERR:
    {
        if (pwdack) {
            secure = false;
            pwdack = false;
        }
        std::cerr << "Got Error : " << data << std::endl;
    }
    break;
    case M_OK: 
    {
        if (pwdack) {
            secure = true;
            pwdack = false;
        }
        std::cerr << "Got OK : " << data << std::endl;
    }
    break;
    case M_GOT:
    {
        if (data != NULL) {
            std::stringstream ss(data);
            std::string fragment;
            std::vector<std::string> frags;
            while (std::getline(ss, fragment, ' ')) {
                frags.push_back(fragment);
            }

            sending_filename = "";
            sending_len = stoul(frags.at(1));
            sending_timestamp = stoul(frags.at(2));
        }
    }
        break;
    case M_NUL:
    {
        if (data != NULL) {
            std::string s(data);
            if (s.size() >= 3 && s.substr(0, 3) == "OPT") {
                std::stringstream ss(s.substr(3));
                std::string fragment;
                while (std::getline(ss, fragment, ' ')) {
                    if (fragment.size() > 9 && fragment.substr(0, 9) == "CRAM-MD5-") {
                        cram5_challenge_data = fragment.substr(9);
                        cram5_init = true;
                        cram5_opt = true;
                    }
                }
            }
        }
    }
        break;
    case M_EOB:
        goteob = true;
        break;
    case M_ADR:
    {
        if (data != NULL) {
            std::stringstream ss(data);
            std::string fragment;
            while (std::getline(ss, fragment, ' ')) {
                NETADDR* newaddr = parse_fido_addr(fragment.substr(0, fragment.find_first_of('@')).c_str());
                if (!newaddr) {
                    continue;
                }
                struct address_t addr;

                addr.addr = newaddr;
                addr.domain = fragment.substr(fragment.find_first_of('@') + 1).c_str();

                remote_addresses.push_back(addr);
            }
        }
    }
        break;
    case M_PWD:
        if (data != NULL) {
            remote_password = data;
        }
        break;
    case M_FILE:
        // receive a file
    {
        if (data != NULL) {
            std::stringstream ss(data);
            std::string fragment;
            std::vector<std::string> frags;
            while (std::getline(ss, fragment, ' ')) {
                frags.push_back(fragment);
            }

            if (frags.size() >= 3) {
                current_filename = std::filesystem::path(frags.at(0)).filename().u8string();
                current_len = stoul(frags.at(1));
                current_timestamp = stoul(frags.at(2));
                current_received = 0;
                if (current_file != NULL) {
                    fclose(current_file);
                }

                std::string fname;

                if (secure) {
                    fname = c.inbound_secure + "/" + current_filename;
                }
                else {
                    fname = c.inbound + "/" + current_filename;
                }
                current_file = fopen(fname.c_str(), "wb");
            }
        }
    }
        break;
    }
    if (data != NULL) free(data);

    return cmd;
}

bool Server::process_frames(int timeout, uint8_t upto) {
    uint8_t cmd = 0xff;
    int ret;

    do {
        uint16_t header;

        ret = receive(socket, (char*)&header, 2, timeout);

        

        if (ret == 2) {
            header = ntohs(header);

            if (header & 0x8000) {
                // process command
                cmd = process_command(header & 0x7fff, timeout);
                if (cmd == 0xff) {
                    return false;
                }
            }
            else {
                // process data
                bool ret2 = process_data(header & 0x7fff, timeout);
                if (ret2 == false) {
                    return false;
                }
            }
        }
        else if (ret == 0) {
            return true; // timeout
        } else {
            return false;
        }
    } while (upto != cmd && upto != 0xff);



    return true;
}

int Server::send_command_packet(uint8_t type, std::string data) {
    uint16_t size = (uint16_t)data.size() + 1;
    uint8_t *out = (uint8_t *)malloc(size + 2);

    log.log(LOG_INFO, "<<< %s: %s", commands[type], data.c_str());

    if (!out) {
        std::cerr << "Out of Memory!" << std::endl;
        log.log(LOG_ERROR, "Out of Memory!");
        exit(-1);
    }

    out[0] = (uint8_t)((size & 0xFF00) >> 8 | 0x80);
    out[1] = (uint8_t)(size & 0x00FF);

    out[2] = type;

    memcpy(&out[3], data.c_str(), data.size());

    send(socket, (const char *)out, size + 2, 0);


    free(out);
    return size;
}

#if defined(_MSC_VER) || defined(WIN32)
int _w_inet_pton(int af, const char* src, void* dst)
{
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN + 1];

    ZeroMemory(&ss, sizeof(ss));
    /* stupid non-const API */
    strncpy(src_copy, src, INET6_ADDRSTRLEN + 1);
    src_copy[INET6_ADDRSTRLEN] = 0;

    if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr*)&ss, &size) == 0) {
        switch (af) {
        case AF_INET:
            *(struct in_addr*)dst = ((struct sockaddr_in*)&ss)->sin_addr;
            return 1;
        case AF_INET6:
            *(struct in6_addr*)dst = ((struct sockaddr_in6*)&ss)->sin6_addr;
            return 1;
        }
    }
    return 0;
}

const char* _w_inet_ntop(int af, const void* src, char* dst, socklen_t size)
{
    struct sockaddr_storage ss;
    unsigned long s = size;

    ZeroMemory(&ss, sizeof(ss));
    ss.ss_family = af;

    switch (af) {
    case AF_INET:
        ((struct sockaddr_in*)&ss)->sin_addr = *(struct in_addr*)src;
        break;
    case AF_INET6:
        ((struct sockaddr_in6*)&ss)->sin6_addr = *(struct in6_addr*)src;
        break;
    default:
        return NULL;
    }
    /* cannot direclty use &size because of strict aliasing rules */
    return (WSAAddressToStringA((struct sockaddr*)&ss, sizeof(ss), NULL, dst, &s) == 0) ?
        dst : NULL;
}

#endif


int hostname_to_ip(const char* hostname, char* ip) {
    struct addrinfo hints, * res, * p;
    int status;
    struct sockaddr_in* ipv4;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        return 1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            ipv4 = (struct sockaddr_in*)p->ai_addr;
            _w_inet_ntop(p->ai_family, &(ipv4->sin_addr), ip, INET_ADDRSTRLEN);
            freeaddrinfo(res);
            return 0;
        }
    }
    freeaddrinfo(res);
    return 1;
}

int binkp_connect_ipv4(const char* server, uint16_t port, int* socketp) {
    struct sockaddr_in servaddr;
    int bink_socket;
    char buffer[513];
    u_long iMode = 1;
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    if (_w_inet_pton(AF_INET, server, &servaddr.sin_addr) != 1) {
        if (hostname_to_ip(server, buffer)) {
            return 0;
        }
        if (!_w_inet_pton(AF_INET, buffer, &servaddr.sin_addr)) {
            return 0;
        }
    }
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if ((bink_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return 0;
    }


    if (connect(bink_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        return 0;
    }
    * socketp = bink_socket;
    return 1;
}


int Server::run(NETADDR* addr, std::string domain) {
#ifdef _MSC_VER
    WSADATA wsaData;

    if (!winsock_init) {
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Error initializing winsock!" << std::endl;
            return -1;
        }
        winsock_init = true;
    }
#endif

    struct link_t* match = NULL;

    for (size_t l = 0; l < c.links.size(); l++) {
        if (c.links.at(l).addr->zone == addr->zone && c.links.at(l).addr->net == addr->net && c.links.at(l).addr->node == addr->node && c.links.at(l).addr->point == addr->point) {
            if (domain != "") {
                if (domain == c.links.at(l).network) {
                    match = &c.links.at(l);
                    break;
                }
            }
            else {
                match = &c.links.at(l);
                break;
            }
        }
    }

    if (match == NULL) {
        return -1;
    }
    if (!binkp_connect_ipv4(match->host.c_str(), match->port, &socket)) {
        log.log(LOG_ERROR, "Error connecting to %s", match->host.c_str());
        return -1;
    }

    // connected!
    goteob = false;
    send_command_packet(M_NUL, "SYS " + _system_name);
    send_command_packet(M_NUL, "ZYZ " + _sysop_name);
    send_command_packet(M_NUL, "LOC " + _location);
    send_command_packet(M_NUL, "VER binki/" + std::string(BINKI_VERSION) + " binkp/1.0");
    std::stringstream ss;

    for (size_t i = 0; i < c.addresses.size(); i++) {
        ss << c.addresses.at(i).addr->zone << ":" << c.addresses.at(i).addr->net << "/" << c.addresses.at(i).addr->node << "." << c.addresses.at(i).addr->point << "@" << c.addresses.at(i).domain;
        if (i < c.addresses.size() - 1) {
            ss << " ";
        }
    }
    send_command_packet(M_ADR, ss.str());

    process_frames(60, M_ADR);
    //process_frames(1, 0xff);

    if (cram5_opt == true) {
         std::string hash("CRAM-MD5-" + cram5_create_hashed_pwd(cram5_challenge_data, match->password));
         send_command_packet(M_PWD, hash);
    }
    else {
        send_command_packet(M_PWD, match->password);
    }

    pwdack = true;
    secure = false;

    while (pwdack) {
        if (!process_frames(60, 0xFF)) {
            return -1;
        }
    }

    bool gotmatch = false;
    senteob = false;
    std::vector<struct link_t> common_links;

    for (struct address_t a : remote_addresses) {
        for (struct link_t l : c.links) {
            if (a.addr->zone == l.addr->zone && a.addr->net == l.addr->net && a.addr->node == l.addr->node && a.addr->point == l.addr->point) {
                bool found = false;
                for (size_t lc = 0; lc < common_links.size(); lc++) {
                    if (common_links.at(lc).network == l.network) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    common_links.push_back(l);
                }
            }
        }
    }
    if (!process_frames(1, 0xff)) {
        return 0;
    }

    sending_filename = "";

    char buffer[5];

    for (struct link_t l : common_links) {
        memset(buffer, 0, 5);

        std::filesystem::path fspath;

        if (l.addr->zone != c.defaultzone) {
            snprintf(buffer, 4, "%03x", l.addr->zone);

            fspath.assign(c.outbound + "." + buffer);
            if (!std::filesystem::exists(fspath)) {
                snprintf(buffer, 4, "%03X", l.addr->zone);
                fspath.assign(c.outbound + "." + buffer);
                if (!std::filesystem::exists(fspath)) {
                    continue;
                }
            }
        }
        else {
            fspath.assign(c.outbound);
        }

        transfer_files(l.network, l.addr, fspath, std::filesystem::path(l.outbox));
    }

    if (!process_frames(2, 0xff)) {
        return 0;
    }

    if (!senteob) {
        send_command_packet(M_EOB, "All done!");
    }

    while (!goteob) {
        if (!process_frames(2, 0xff)) {
            return 0;
        }
    }

    return 0;

}

int Server::load_config() {
    // load config
    INIReader inir("talisman.ini");

    if (inir.ParseError()) {
        std::cerr << "Failed to parse talisman.ini" << std::endl;
        return -1;
    }

    _datapath = inir.Get("Paths", "Data Path", "data");
    _logpath = inir.Get("Paths", "Log Path", "logs");
    _tmppath = inir.Get("Paths", "Temp Path", "temp");
    _sysop_name = inir.Get("Main", "Sysop Name", "Sysop");
    _system_name = inir.Get("Main", "System Name", "A Talisman BBS");
    _location = inir.Get("Main", "Location", "Somewhere, The World");

    if (!c.load(_datapath)) {
        std::cerr << "Error loading config!" << std::endl;
        return -1;
    }

    log.load(_logpath + "/binki.log");

    return 0;
}

int Server::run(int socket) {

    this->socket = socket;

#ifdef _MSC_VER
    WSADATA wsaData;

    if (!winsock_init) {
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Error initializing winsock!" << std::endl;
            return -1;
        }
        winsock_init = true;
    }
#endif
    // Send
    goteob = false;

    cram5_init_challenge_data();

    send_command_packet(M_NUL, "OPT CRAM-MD5-" + cram5_challenge_data);
    send_command_packet(M_NUL, "SYS " + _system_name);
    send_command_packet(M_NUL, "ZYZ " + _sysop_name);
    send_command_packet(M_NUL, "LOC " + _location);
    send_command_packet(M_NUL, "VER binki/0.1 binkp/1.0");
    std::stringstream ss;

    for (size_t i = 0; i < c.addresses.size(); i++) {
        ss << c.addresses.at(i).addr->zone << ":" << c.addresses.at(i).addr->net << "/" << c.addresses.at(i).addr->node << "." << c.addresses.at(i).addr->point << "@" << c.addresses.at(i).domain;
        if (i < c.addresses.size() - 1) {
            ss << " ";
        }
    }
    send_command_packet(M_ADR, ss.str());

    if (!process_frames(60, M_PWD)) {
        log.log(LOG_ERROR, "Error waiting for password");
        return 0;
    }

    if (remote_addresses.empty()) {
        send_command_packet(M_ERR, "Unable to find common address!");
        log.log(LOG_ERROR, "Unable to find common address");
        return 0;
    }

    bool gotmatch = false;
    secure = false;
    senteob = false;
    std::vector<struct link_t> common_links;

    for (struct address_t a : remote_addresses) {
        for (struct link_t l : c.links) {
            if (a.addr->zone == l.addr->zone && a.addr->net == l.addr->net && a.addr->node == l.addr->node && a.addr->point == l.addr->point) {
                bool found = false;
                for (size_t lc = 0; lc < common_links.size(); lc++) {
                    if (common_links.at(lc).network == l.network) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    common_links.push_back(l);
                }
            }
        }
    }

    if (remote_password != "-") {
        for (struct address_t a : remote_addresses) {
            for (struct link_t l : common_links) {
                if (a.addr->zone == l.addr->zone && a.addr->net == l.addr->net && a.addr->node == l.addr->node && a.addr->point == l.addr->point) {
                    gotmatch = true;
                    secure = true;
                    if (!(remote_password.substr(0, 5) == "CRAM-")) {
                        if (l.crammd5) {
                            send_command_packet(M_ERR, "Sorry, unencrypted passwords are not acceptable.");
                        }
                        else {
                            if (remote_password != l.password) {
                                send_command_packet(M_ERR, "Password mismatch!");
                                log.log(LOG_ERROR, "Password mismatch!");
                                return 0;
                            }
                        }
                    }
                    else {
                        if (remote_password.substr(0, 9) == "CRAM-MD5-") {
                            if (!cram5_validate_password(cram5_challenge_data, l.password, remote_password.substr(9))) {
                                send_command_packet(M_ERR, "Password mismatch!");
                                log.log(LOG_ERROR, "Password mismatch!");
                                return 0;
                            }
                        }
                        else {
                            send_command_packet(M_ERR, "Unavailable Digest!");
                            log.log(LOG_ERROR, "Unavailable Digest");
                            return 0;
                        }
                    }
                }
            }
        }
    }

    if (secure == true) {
        log.log(LOG_INFO, "Passwords match, Secure Session.");
        send_command_packet(M_OK, "Passwords match, Secure Session.");
    }
    else if (gotmatch == true) {
        log.log(LOG_INFO, "No Password, Insecure Session.");
        send_command_packet(M_OK, "No Password, Insecure Session.");
    }
    else {
        log.log(LOG_ERROR, "Unable to find common address");
        send_command_packet(M_ERR, "Unable to find common address!");
        return 0;
    }

    if (!process_frames(1, 0xff)) {
        return 0;
    }

    sending_filename = "";

    char buffer[5];

    for (struct link_t l : common_links) {
        memset(buffer, 0, 5);

        std::filesystem::path fspath;

        if (l.addr->zone != c.defaultzone) {
            snprintf(buffer, 4, "%03x", l.addr->zone);

            fspath.assign(c.outbound + "." + buffer);
            if (!std::filesystem::exists(fspath)) {
                snprintf(buffer, 4, "%03X", l.addr->zone);
                fspath.assign(c.outbound + "." + buffer);
                if (!std::filesystem::exists(fspath)) {
                    continue;
                }
            }
        }
        else {
            fspath.assign(c.outbound);
        }

        transfer_files(l.network, l.addr, fspath, std::filesystem::path(l.outbox));
    }

    if (!senteob) {
        send_command_packet(M_EOB, "All done!");
    }
    
    while (!goteob) {
        if (!process_frames(2, M_EOB)) {
            return 0;
        }
    }

	return 0;
}

int Server::runall() {
    std::vector<std::string> addrs_to_poll;

    for (size_t i = 0; i < c.links.size(); i++) {
        if (!std::filesystem::is_empty(c.links.at(i).outbox)) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
        std::stringstream ss;
        if (c.links.at(i).addr->zone != c.defaultzone) {
            ss << c.outbound << "." << std::setfill('0') << std::setw(3) << std::hex << c.links.at(i).addr->zone;
        }
        else {
            ss << c.outbound;
        }


        char flowfname[9];

        memset(flowfname, 0, 9);

        if (c.links.at(i).addr->point == 0) {
            snprintf(flowfname, 9, "%04x%04x", c.links.at(i).addr->net, c.links.at(i).addr->node);
        }
        else {
            snprintf(flowfname, 9, "%08x", c.links.at(i).addr->point);
        }

        if (std::filesystem::exists(ss.str() + "/" + flowfname + ".clo")) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
        if (std::filesystem::exists(ss.str() + "/" + flowfname + ".flo")) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
        if (std::filesystem::exists(ss.str() + "/" + flowfname + ".cut")) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
        if (std::filesystem::exists(ss.str() + "/" + flowfname + ".out")) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
#ifndef _MSC_VER
        ss.str("");
        if (c.links.at(i).addr->zone != c.defaultzone) {
            ss << c.outbound << "." << std::setfill('0') << std::setw(3) << std::uppercase << std::hex << c.links.at(i).addr->zone;
        }
        else {
            ss << c.outbound;
        }

        memset(flowfname, 0, 9);

        if (c.links.at(i).addr->point == 0) {
            snprintf(flowfname, 9, "%04X%04X", c.links.at(i).addr->net, c.links.at(i).addr->node);
        }
        else {
            snprintf(flowfname, 9, "%08X", c.links.at(i).addr->point);
        }

        if (std::filesystem::exists(ss.str() + "/" + flowfname + ".CLO")) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
        if (std::filesystem::exists(ss.str() + "/" + flowfname + ".FLO")) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
        if (std::filesystem::exists(ss.str() + "/" + flowfname + ".CUT")) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
        if (std::filesystem::exists(ss.str() + "/" + flowfname + ".OUT")) {
            addrs_to_poll.push_back(std::to_string(c.links.at(i).addr->zone) + ":" + std::to_string(c.links.at(i).addr->net) + "/" + std::to_string(c.links.at(i).addr->node) + "." + std::to_string(c.links.at(i).addr->point) + "@" + c.links.at(i).network);
            continue;
        }
#endif
    }
    for (size_t i = 0; i < addrs_to_poll.size(); i++) {
        NETADDR* addr = parse_fido_addr(addrs_to_poll.at(i).substr(0, addrs_to_poll.at(i).find('@')).c_str());
        if (addr) {
            run(addr, addrs_to_poll.at(i).substr(addrs_to_poll.at(i).find('@') + 1));
            free(addr);
        }
    }

    return 0;
}