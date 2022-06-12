#include "../src/iomanager.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <assert.h>
#include <string.h>

using namespace dserver;

int sockfd;
void watch_io_read();

// 写事件回调，只执行一次，用于判断非阻塞套接字connect成功
void do_io_write() {
    std::cout << "write callback";
    int so_err;
    socklen_t len = size_t(so_err);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_err, &len);
    if(so_err) {
        std::cout<< "connect fail";
        return;
    } 
    std::cout << "connect success";
}

// 读事件回调，每次读取之后如果套接字未关闭，需要重新添加
void do_io_read() {
    std::cout << "read callback";
    char buf[1024] = {0};
    int readlen = 0;
    readlen = read(sockfd, buf, sizeof(buf));
    if(readlen > 0) {
        buf[readlen] = '\0';
        std::cout << "read " << readlen << " bytes, read: " << buf;
    } else if(readlen == 0) {
        std::cout << "peer closed";
        close(sockfd);
        return;
    } else {
        std::cout << "err, errno=" << errno << ", errstr=" ;
        close(sockfd);
        return;
    }
    // read之后重新添加读事件回调，这里不能直接调用addEvent，因为在当前位置fd的读事件上下文还是有效的，直接调用addEvent相当于重复添加相同事件
    IOManager::GetThis()->schedule(watch_io_read);
}

void watch_io_read() {
    std::cout << "watch_io_read";
    IOManager::GetThis()->add_event(sockfd, IOManager::READ, do_io_read);
}

void test_io() {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd > 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(1234);
    inet_pton(AF_INET, "10.10.19.159", &servaddr.sin_addr.s_addr);

    int rt = connect(sockfd, (const sockaddr*)&servaddr, sizeof(servaddr));
    if(rt != 0) {
        if(errno == EINPROGRESS) {
            std::cout << "EINPROGRESS";
            // 注册写事件回调，只用于判断connect是否成功
            // 非阻塞的TCP套接字connect一般无法立即建立连接，要通过套接字可写来判断connect是否已经成功
            IOManager::GetThis()->add_event(sockfd, IOManager::WRITE, do_io_write);
            // 注册读事件回调，注意事件是一次性的
            IOManager::GetThis()->add_event(sockfd, IOManager::READ, do_io_read);
        } else {
            std::cout << "connect error, errno:" << errno << ", errstr:" << strerror(errno);
        }
    } else {
        std::cout << "else, errno:" << errno << ", errstr:" << strerror(errno);
    }
}

void test_iomanager() {
    IOManager iom;
    // sylar::IOManager iom(10); // 演示多线程下IO协程在不同线程之间切换
    iom.schedule(test_io);
}

int main(int argc, char *argv[]) {
    
    test_iomanager();

    return 0;
}