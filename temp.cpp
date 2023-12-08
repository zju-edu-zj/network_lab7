#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
using namespace std;
int main(){
    const short port = 4234;
    cout << htons(port) << endl;
}