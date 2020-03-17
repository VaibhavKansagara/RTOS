
#include <bits/stdc++.h>

using namespace std;

int main() {
    string line;
    long double delay = 0;
    int count = 0;
    ifstream myfile("test_results.txt");
    if (myfile.is_open()) {
        while(getline(myfile, line)) {
            delay += stold(line);
            count++;
        }
    }
    printf("%.12LG", delay / count);
    myfile.close(); 
}
