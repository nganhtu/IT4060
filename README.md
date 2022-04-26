# Bài 1. Mở đầu

Chỉ thị tiền xử lý:
```c++
#include "winsock2.h"
#pragma comment(lib, "Ws2_32.lib")
```

# Bài 2. Cơ bản về lập trình mạng

## Một số hàm cơ bản:
- Khởi tạo và giải phóng WinSock:
```c++
WSADATA wsaData;
WORD wVersion = MAKEWORD(2, 2);
WSAStartup(wVersion, &wsaData);
// ...
WSACleanup();
```
- Địa chỉ socket được lưu bằng cấu trúc `sockaddr_in`.
- Xử lý địa chỉ socket:
	- Đổi dạng biểu diễn bằng các hàm `inet_pton()`, `inet_ntop()`, `htons()`, `htonl()`, `ntohs()`, `ntohl()`. Ví dụ:
	```c++
	in_addr address;
	inet_pton(AF_INET, "127.0.0.1", &address);
	```
	- Phân giải tên miền / địa chỉ: `getaddrinfo()`, `gethostbyname()`, `gethostbyaddr()`, `gethostname()`. Ví dụ:
	```c++
	addrinfo hints;
	hints.ai_family = AF_INET;
	memset(&hints, 0, sizeof(hints));
	addrinfo *result;
	getaddrinfo("google.com", NULL, &hints, &result);
	// ...
	freeaddrinfo(result);
	```
	```c++
	struct sockaddr_in addr;
	char hostname[NI_MAXHOST], serverInfo[NI_MAXSERV];
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
	getnameinfo((struct sockaddr *) &addr, sizeof(struct sockaddr), hostname, NI_MAXHOST, serverInfo, NI_MAXSERV, NI_NUMERICSERV);
	```
	> Ghi nhớ: hàm `getaddrinfo()` trả về kiểu dữ liệu `struct sockaddr **`, hàm `getnameinfo()` nhận kiểu dữ liệu `struct sockaddr *`. Muốn tương tác với địa chỉ IP ta lại phải dùng trường `sin_addr` của kiểu dữ liệu `struct sockaddr_in` (chuyển đổi qua lại bằng ép kiểu). Lưu ý các hàm nhận hoặc trả IP dưới dạng nhị phân; do đó cần dùng `inet_ntop()` hay `inet_pton()` để chuyển về dạng String.
- Khởi tạo socket: ví dụ:
```c++
SOCKET client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
...
closesocket(client);
```
- Gán địa chỉ cho socket bằng hàm `bind()`. Ví dụ:
```c++
SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
sockaddr_in addr;
addr.sin_family = AF_INET;
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
addr.sin_port = htons(5500);
bind(s, (sockaddr *)&addr, sizeof(addr));
```
- Tùy chọn cho socket (đọc thêm): sử dụng hàm `setsockopt()` và `getsockopt()`. Ví dụ:
```c++
SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
// (optional) Set time-out for receiving
int tv = 10000; // Time-out interval: 10000ms
setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)(&tv), sizeof(int));
```

## Xây dựng ứng dụng với UDP socket
- Sơ đồ chung:
	- Server: `socket()` → `bind()` → {`recvfrom()` và `sendto()`} → `closesocket()`
	- Client: `socket()` → {`sendto()` và `recvfrom()`} → `closesocket()`
- Hàm `sendto()` gửi dữ liệu đến một socket biết trước địa chỉ. Ví dụ:
```c++
SOCKET sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
sockaddr_in receiverAddr;
// cần phải biết địa chỉ của receiver trước khi thực thi hàm sendto()
sendto(sender, buff, strlen(buff), 0, (sockaddr *)&receiverAddr, sizeof(receiverAddr));
```
- Hàm `recvfrom()` nhận dữ liệu từ một nguồn nào đó (xác định sau khi hàm thực thi). Ví dụ:
```c++
SOCKET receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
sockaddr_in senderAddr;
int senderAddrLen = sizeof(senderAddr);
ret = recvfrom(receiver, buff, BUFF_SIZE, 0, (sockaddr *)&senderAddr, &senderAddrLen);
```
> Lưu ý rằng nếu kích thước thông điệp gửi tới lớn hơn bộ đệm UDP socket bên nhận thì chỉ nhận phần dữ liệu vừa đủ với kích thước bộ đệm, phần còn lại bị bỏ qua và hàm trả về `SOCKET_ERROR`.

## Xây dựng ứng dụng với TCP socket
