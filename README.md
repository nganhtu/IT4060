# Bài 1. Mở đầu

Chỉ thị tiền xử lý:
```c++
#include "winsock2.h"
#pragma comment(lib, "Ws2_32.lib")
```

# Bài 2. Cơ bản về lập trình mạng

1. Một số hàm cơ bản:
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
- Khởi tạo socket:
