#include <iostream>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <signal.h>

using namespace std;

int ppid;
vector<int> pids;

void handle_sigint(int sig) {
    for (int i = 0; i < pids.size(); i++) {
        kill(pids[i], SIGINT);
        cout << "\n";
    }
    sleep(2);
    kill(ppid, SIGINT);
    cout << "\nTHE-END\n";
}

int main(int argc, char* argv[]) {
    ppid = fork();
    string port = "6655";
    if (ppid == 0) {
        execl(argv[1], argv[1], "127.0.0.1", port.c_str(), NULL);
    } else {
        usleep(100000);
        int no_senders = atoi(argv[2]);
        int no_receivers = atoi(argv[3]);

        string name1 = "Vaibhav";
        for (int i = 0; i < no_senders; i++) {
            int pid = fork();
            if (pid == 0) {
                execl("sender", "sender", "127.0.0.1", port.c_str(), (name1 + to_string(i)).c_str(), "2", NULL);
            } else {
                pids.push_back(pid);
            }
            usleep(20000);
        }

        string name2 = "Mahidhar";
        for (int i = 0; i < no_receivers; i++) {
            int pid = fork();
            if (pid == 0) {
                execl("receiver", "receiver", "127.0.0.1", port.c_str(), (name2 + to_string(i)).c_str(), "2", NULL);
            } else {
                pids.push_back(pid);                
            }
            usleep(20000);
        }
        sleep(1);
        handle_sigint(SIGINT);
    }
}
