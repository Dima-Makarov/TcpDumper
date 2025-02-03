#include "imtui.h"

#include "imtui-impl-ncurses.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <vector>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sstream>

std::string Execute(std::string command) {
    FILE* pipe = popen(command.c_str(), "r");  
    if (!pipe) {
        std::cerr << "Failed to run command\n";
        throw;
    }
    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += std::string(buffer);
    }

    pclose(pipe);
    return result;
}

class Subprocess {
public:
    Subprocess(const std::string& command, const std::vector<std::string>& args) {
        if (pipe(pipefd) == -1) {
            throw std::runtime_error("Failed to create pipe");
        }

        int flags = fcntl(pipefd[0], F_GETFL, 0);
        if (flags == -1 || fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) == -1) {
            close(pipefd[0]);
            close(pipefd[1]);
            throw std::runtime_error("Failed to set pipe non-blocking");
        }

        pid = fork();
        if (pid < 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            throw std::runtime_error("Failed to fork process");
        }

        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);

            close(pipefd[1]);

            std::vector<const char*> c_args;
            c_args.push_back(command.c_str());
            for (const auto& arg : args) {
                c_args.push_back(arg.c_str());
            }
            c_args.push_back(nullptr);

            execvp(command.c_str(), const_cast<char* const*>(c_args.data()));

            perror("execvp failed");
            _exit(EXIT_FAILURE);
        }
        close(pipefd[1]);
    }

    ~Subprocess() {
        if (pid > 0) {
            waitpid(pid, nullptr, 0);
        }
        close(pipefd[0]);
    }

    std::string readOutput() {
        char buffer[2048];
        std::string output;
        ssize_t count;
        while ((count = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            std::cout << count;
            buffer[count] = '\0';
            output += buffer;
        }

        if (count == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            throw std::runtime_error("Error reading from pipe: " + std::string(strerror(errno)));
        }

        return output;
    }

public:
    pid_t pid = -1;
    int pipefd[2];
};

void IpAddressPicker(std::vector<int>* ip_addr) {
    if (ip_addr->size() != 4) {
        throw ip_addr;
    }  
    if (ImGui::Button("^^^##1up", ImVec2(3,1))) {
        (*ip_addr)[0]++;
    }
    ImGui::SameLine();
    if (ImGui::Button("^^^##2up", ImVec2(3,1))) {
        (*ip_addr)[1]++;
    }
    ImGui::SameLine();
    if (ImGui::Button("^^^##3up", ImVec2(3,1))) {
        (*ip_addr)[2]++;
    }
    ImGui::SameLine();
    if (ImGui::Button("^^^##4up", ImVec2(3,1))) {
        (*ip_addr)[3]++;
    }
    ImGui::Text("%03d.%03d.%03d.%03d", (*ip_addr)[0], (*ip_addr)[1], (*ip_addr)[2], (*ip_addr)[3]);
    if (ImGui::Button("vvv##1down", ImVec2(3,1))) {
        (*ip_addr)[0]--;
    }
    ImGui::SameLine();
    if (ImGui::Button("vvv##2down", ImVec2(3,1))) {
        (*ip_addr)[1]--;
    }
    ImGui::SameLine();
    if (ImGui::Button("vvv##3down", ImVec2(3,1))) {
        (*ip_addr)[2]--;
    }
    ImGui::SameLine();
    if (ImGui::Button("vvv##4down", ImVec2(3,1))) {
        (*ip_addr)[3]--;
    }
}

void Ping() {
    static std::string ping_result = "";
    static Subprocess* ping = nullptr;
    static std::vector<int> ip_addr = {192, 168, 0, 1};
    IpAddressPicker(&ip_addr);
    if (ImGui::Button("PING")) {
        if (!ping) {
            std::string str_to_ping = std::to_string(ip_addr[0]) + "." +
                                      std::to_string(ip_addr[1]) + "." +
                                      std::to_string(ip_addr[2]) + "." +
                                      std::to_string(ip_addr[3]);
            ping = new Subprocess("ping", {"-c", "4", str_to_ping});
            ping_result.clear();
        }
    }
    if (ping) {
        std::string output = ping->readOutput();
        ping_result += output;
        int status;
        pid_t result = waitpid(ping->pid, &status, WNOHANG);
        if(result == ping->pid) {
            delete ping;
            ping = nullptr;
        }
    }
    std::stringstream ss(ping_result);
    std::string line;
    std::string result;
    while(std::getline(ss, line))
    {
        if (line.empty()) {
            result += '\n';
        } else if (line[3] == 'b') {
            result += "OK\n";
        } else {
            result += line + '\n';
        }
    }
    ImGui::TextWrapped(result.c_str());
}
#include <vector>
#include <string>
#include <signal.h>
#include <sys/wait.h>

void TcpDump() {
    static Subprocess* tcpdump = nullptr;
    static std::vector<std::string> tcpdump_result;
    static int page = 0;
    static int hor_scroll = 0;
    const int lines_per_page = 17;
    tcpdump_result.reserve(1000);
    
    if (ImGui::Button("GO")) {
        if (!tcpdump) {
            tcpdump = new Subprocess("tcpdump", {"-ntl"});
            tcpdump_result.clear();
            page = 0;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("STOP")) {
        if (tcpdump) {
            kill(tcpdump->pid, SIGTERM);
            delete tcpdump;
            tcpdump = nullptr;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("UP") && page > 0) {
        page--;
    }
    ImGui::SameLine();
    ImGui::Text((std::to_string(page) + "/" + std::to_string(tcpdump_result.size() / lines_per_page)).c_str());
    ImGui::SameLine();
    if (ImGui::Button("DN") && ((page + 1) * lines_per_page < (int)tcpdump_result.size())) {
        page++;
    }
    ImGui::SameLine();
    if (ImGui::Button("<") && hor_scroll > 0) {
        hor_scroll--;
    }
    ImGui::SameLine();
    ImGui::Text(std::to_string(hor_scroll).c_str());
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        hor_scroll++;
    }
    if (tcpdump) {
        std::string output = tcpdump->readOutput();
        if (!output.empty()) {
            std::stringstream ss (output);
            std::string line;
            while(std::getline(ss, line)) {
                tcpdump_result.push_back(line);
            }
        }
        int status;
        pid_t result = waitpid(tcpdump->pid, &status, WNOHANG);
        if (result == tcpdump->pid) {
            delete tcpdump;
            tcpdump = nullptr;
        }
    }
    
    int start_idx = page * lines_per_page;
    int end_idx = std::min(start_idx + lines_per_page, (int)tcpdump_result.size());
    
    for (int i = start_idx; i < end_idx; i++) {
        if (hor_scroll >= (int)tcpdump_result[i].size()) {
            ImGui::Text(" ");
        } else {
            ImGui::Text(tcpdump_result[i].c_str() + hor_scroll);
        }
    }
} 

void PacketGenerator() {
    ImGui::Text("hi from pktgen");
}

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

std::string GetIPv4Address() {
    struct ifaddrs *ifaddr, *ifa;
    std::string ipAddress = "";

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {  // Check for IPv4
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char addrStr[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &addr->sin_addr, addrStr, INET_ADDRSTRLEN);
            
            // Ignore loopback address (127.x.x.x)
            if (strncmp(addrStr, "127.", 4) != 0) {
                ipAddress = addrStr;
                break;  // Return the first valid IPv4 address found
            }
        }
    }

    freeifaddrs(ifaddr);
    return ipAddress;
}

std::vector<int> IPv4ToVector(const std::string& ip) {
    std::vector<int> octets;
    std::stringstream ss(ip);
    std::string octet;

    while (std::getline(ss, octet, '.')) {
        octets.push_back(std::stoi(octet));
    }

    return octets;
}

std::string VectorToIpv4(const std::vector<int>& vec) {
    std::string result = "";
    for (auto i : vec) {
	result += std::to_string(i) + ".";
    }
    result.pop_back();
    return result;
}

void Interface() {
    ImGui::TextWrapped(Execute("ip a s dev eth0").c_str());
}

#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <mutex>

#define INTERFACE "eth0"  // Change to the correct network interface

std::vector<std::string> active_hosts;
std::mutex host_mutex;

// Structure for an ARP packet
struct arp_packet {
    struct ethhdr eth;
    struct ether_arp arp;
};

// Get MAC address of the local interface
bool get_mac_address(uint8_t *mac) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;

    struct ifreq ifr{};
    strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        close(fd);
        return false;
    }

    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
    return true;
}
bool arp_listening_thread_running = false;

// Listen for ARP replies and store active hosts with their MAC addresses
void receive_arp_replies(int sock) {
    uint8_t buffer[65536];
    while (arp_listening_thread_running) {
        ssize_t len = recv(sock, buffer, sizeof(buffer), 0);
        if (len <= 0) continue;

        struct ethhdr *eth = (struct ethhdr *)buffer;
        if (ntohs(eth->h_proto) != ETH_P_ARP) continue;

        struct ether_arp *arp = (struct ether_arp *)(buffer + sizeof(struct ethhdr));
        if (ntohs(arp->arp_op) == ARPOP_REPLY) {
            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, arp->arp_spa, sender_ip, sizeof(sender_ip));

            char sender_mac[18];
            snprintf(sender_mac, sizeof(sender_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                     arp->arp_sha[0], arp->arp_sha[1], arp->arp_sha[2],
                     arp->arp_sha[3], arp->arp_sha[4], arp->arp_sha[5]);

            std::lock_guard<std::mutex> lock(host_mutex);
            bool found = false;
            std::string cur_entry = std::string(sender_ip) + "\n" + std::string(sender_mac);
            for (const auto &entry : active_hosts) {
                if (entry == cur_entry) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                active_hosts.emplace_back(cur_entry);
            }
        }
    }
}


// Send ARP request to a specific IP
void send_arp_request(const std::string &target_ip, int sock, struct sockaddr_ll &socket_address, uint8_t *src_mac) {
    struct arp_packet packet{};

    // Set Ethernet header
    memset(packet.eth.h_dest, 0xFF, 6);  // Broadcast MAC
    memcpy(packet.eth.h_source, src_mac, 6);
    packet.eth.h_proto = htons(ETH_P_ARP);

    // Set ARP header
    packet.arp.arp_hrd = htons(ARPHRD_ETHER);
    packet.arp.arp_pro = htons(ETH_P_IP);
    packet.arp.arp_hln = 6;
    packet.arp.arp_pln = 4;
    packet.arp.arp_op = htons(ARPOP_REQUEST);

    memcpy(packet.arp.arp_sha, src_mac, 6);
    inet_pton(AF_INET, GetIPv4Address().c_str(), packet.arp.arp_spa);  // Spoofed sender IP
    memset(packet.arp.arp_tha, 0, 6);
    inet_pton(AF_INET, target_ip.c_str(), packet.arp.arp_tpa);

    sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr*)&socket_address, sizeof(socket_address));
}

std::thread* receiver_thread;

int arp_scan() {
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (sock < 0) {
        perror("Socket error");
        return -1;
    }

    struct sockaddr_ll socket_address{};
    socket_address.sll_ifindex = if_nametoindex(INTERFACE);
    socket_address.sll_halen = ETH_ALEN;
    memset(socket_address.sll_addr, 0xFF, 6);  // Broadcast

    uint8_t src_mac[6];
    if (!get_mac_address(src_mac)) {
        std::cerr << "Failed to get MAC address.\n";
        close(sock);
        return -1;
    }
    arp_listening_thread_running = true;
    // Start ARP response listener in a separate thread
    receiver_thread = new std::thread(receive_arp_replies, sock);
    std::vector ip = IPv4ToVector(GetIPv4Address());
    ip.pop_back();
    std::string ip_str = VectorToIpv4(ip);
    // Send ARP requests to the 192.168.0.0/24 subnet
    for (int i = 1; i <= 254; ++i) {
        std::string target_ip = ip_str + "." + std::to_string(i);
        send_arp_request(target_ip, sock, socket_address, src_mac);
    }
    return sock;
}

void Scanner() {
    int sock;
    static int scroll = 0;
    if (ImGui::Button("START")) {
        if (!arp_listening_thread_running) {
            sock = arp_scan();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("STOP")) {
        if (arp_listening_thread_running) {
            arp_listening_thread_running = false;
            close(sock);
            receiver_thread->detach();
            delete receiver_thread;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("<") && scroll > 0) {
        scroll--;
    }
    ImGui::SameLine();
    ImGui::Text(std::to_string(scroll).c_str());
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        scroll++;
    }
    if (arp_listening_thread_running) {
        ImGui::SameLine();
        ImGui::Text("Scanning...");
    }
    std::string res = "";
    std::lock_guard<std::mutex> lock(host_mutex);
    for(int i = scroll; i < active_hosts.size() && i < scroll + 4; i++) {
        res += active_hosts[i] + "\n";
    }
    ImGui::TextWrapped(res.c_str());
    
}

std::vector<std::string> modes = {
    "pinger",
    "tcpdumper",
    "scanner",
    "interface"
};

int main()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();
    static bool mode_screen = false;
    std::string current_mode = "pinger";
    while (true)
    {
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(30, 20));
        ImGui::Begin("Hello, world!", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);
        if(!mode_screen) {
            if (ImGui::Button("MODE", ImVec2(5,2))) {
                mode_screen = !mode_screen;
            }
        } else {
            ImGui::Text("Select mode:");
            ImGui::Text(" ");
        }
        
        if(mode_screen) {
            for(auto& mode : modes) {
                if(ImGui::Button(mode.c_str(), ImVec2(0, 2))) {
                    current_mode = mode;
                    mode_screen = false;
                }
            }
        } else {
            ImGui::SameLine();
            ImGui::SetCursorPosX(10);
            ImGui::Text(current_mode.c_str());
            if (current_mode == "pinger") {
                Ping();
            } else if(current_mode == "tcpdumper") {
                TcpDump();
            } else if(current_mode == "scanner") {
                Scanner();
            } else if(current_mode == "interface") {
                Interface();
            } else {
                ImGui::Text("Internal error");
            }
        }
        ImGui::End();

        ImGui::Render();

        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }

    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();

    return 0;
}
