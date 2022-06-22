# Bài 1. Mở đầu

Chỉ thị tiền xử lý:
```c++
#include "winsock2.h"
#pragma comment(lib, "Ws2_32.lib")
```

# Bài 2. Cơ bản về lập trình mạng

## Một số hàm cơ bản

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
// ...
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

- Sơ đồ chung:
	- Server: `socket()` → `bind()` → `listen()` → {`accept()` → {`recv()` và `send()`} → `shutdown()` → `closesocket()`  → quay lại `accept()`}
	- Client: `socket()` → `connect()` → {`send()` và `recv()`} → `shutdown()` → `closesocket()`
- Hàm `listen()` đặt socket sang trạng thái lắng nghe kết nối. Ví dụ:
```c++
SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
sockaddr_in serverAddr;
serverAddr.sin_family = AF_INET;
serverAddr.sin_port = htons(5500);
inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr));
listen(listenSock, 10);
```
- Hàm `accept()` khởi tạo một socket gắn với kết nối TCP nằm trong hàng đợi. Ví dụ:
```c++
sockaddr_in clientAddr;
int clientAddrLen = sizeof(clientAddr);
SOCKET connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
```
- Hàm `connect()` gửi yêu cầu thiết lập kết nối tới server. Ví dụ:
```c++
SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
sockaddr_in serverAddr;
serverAddr.sin_family = AF_INET;
serverAddr.sin_port = htons(5500);
inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
connect(client, (sockaddr *)&serverAddr, sizeof(serverAddr));
```
- Hàm `send()` gửi dữ liệu bằng socket. Ví dụ:
```c++
send(client, buff, strlen(buff), 0);
```
- Hàm `recv()` nhận dữ liệu bằng socket. Ví dụ:
```c++
recv(client, buff, BUFF_SIZE, 0);`
```
> Lưu ý: trên UDP socket có thể sử dụng hàm `connect()` để thiết lập địa chỉ của phía bên kia khi truyền tin. Nếu UDP socket đã dùng hàm `connect()` để kiểm tra, có thể sử dụng `send()` thay cho `sendto()` hay hàm `recv()` thay cho `recvfrom()`.
- Hàm `shutdown()` đóng kết nối trên socket (theo 1 hoặc 2 chiều gửi và nhận). Ví dụ:
```c++
shutdown(client, SD_RECEIVE);
```
- Kích thước bộ đệm TCP socket trên Windows 8.1 là 64kB.
	- Hàm `send()` dừng vòng lặp nếu dữ liệu gửi đi lớn hơn kích thước bộ đệm của ứng dụng.
	- Hàm `recv()`: khi kích thước bộ đệm nhận nhỏ hơn kích thước thông điệp gửi tới, cần sử dụng vòng lặp để đọc được hết dữ liệu.
- Giải pháp truyền theo dòng byte trong TCP:
	- Giải pháp 1: sử dụng thông điệp có kích thước cố định.
	- Giải pháp 2: sử dụng mẫu ký tự phân tách (delimiter).
	- Giải pháp 3: gửi kèm kích thước thông điệp.

## Xây dựng giao thức cho ứng dụng

Xem [trang ]30](https://users.soict.hust.edu.vn/tungbt/it4060/Lec02.BasicSocket.pdf).

# Bài 3. Các chế độ vào ra trên socket

## Các chế độ vào ra

- Nhận xét: TCP server chỉ phục vụ được cùng lúc 1 client.
- Các chế độ hoạt động trên WinSock:
	- Chế độ chặn dừng, hoặc đồng bộ: là chế độ mặc định (`connect()`, `accept()`, `send()`,...)
	- Chế độ không chặn dừng, hoặc bất đồng bộ:
		- Các thao tác vào ra trên SOCKET sẽ trở về nơi gọi ngay lập tức và tiếp tục thực thi luồng. Kết quả của thao tác vào ra sẽ được thông báo cho chương trình dưới một cơ chế đồng bộ nào đó.
		- Các hàm vào ra bất đồng bộ sẽ trả về mã lỗi WSAEWOULDBLOCK nếu thao tác đó không thể hoàn tất ngay và mất thời gian đáng kể (chấp nhận kết nối, nhận dữ liệu, gửi dữ liệu...)
		- Socket chuyển sang chế độ này bằng hàm `ioctlsocket()`:
		```c++
		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		unsigned long ul = 1;
		ioctlsocket(s, FIONBIO, (unsigned long *) &ul);
		```
		- Một số trường hợp trả về WSAEWOULDLOCK: xem [trang 5](https://users.soict.hust.edu.vn/tungbt/it4060/Lec03.IOMode.pdf).

## Kỹ thuật đa luồng

- Giải quyết vấn đề chặn dừng bằng cách tạo ra các luồng riêng biệt với hàm `_beginthreadex()`.
- Sơ đồ luồng chính: `socket()` → `bind()` → `listen()` → {`accept()` → `_beginthreadex() → other task → quay lại `accept()`}
- Một số hàm xử lý luồng: [trang 8](https://users.soict.hust.edu.vn/tungbt/it4060/Lec03.IOMode.pdf)
- Điều độ luồng sử dụng đoạn găng:
	- Khai báo đoạn găng: `CRITICAL_SECTION`
	- Khởi tạo đoạn găng: `void InitializeCriticalSection(CRITICAL_SECTION *);`
	- Giải phóng đoạn găng: `void DeleteCriticalSection(CRITICAL_SECTION *);`
	- Yêu cầu vào đoạn găng: `EnterCriticalSection(CRITICAL_SECTION *);`
	- Rời khỏi đoạn găng: `LeaveCriticalSection(CRITICAL_SECTION *);`
- Ví dụ về điều độ luồng sử dụng đoạn găng:
	- Luồng chính:
	```c++
	CRITICAL_SECTION criticalSection;
	int main() {
		InitializeCriticalSection(&criticalSection);
		// ...
		DeleteCriticalSection(&criticalSection);
	}
	```
	- Hàm thực hiện trong luồng con:
	```c++
	unsigned int __stdcall mythread(void *) {
		EnterCriticalSection(&criticalSection);
		// ...
		LeaveCriticalSection(&criticalSection);
		return 0;
	}
	```

## Kỹ thuật thăm dò
- Sơ đồ: `socket()` → `bind()` → `listen()` → {khởi tạo tập select → `select()` → xử lý sự kiện vào ra}
- Sử dụng hàm `select()`:
	- Thăm dò các trạng thái trên socket (yêu cầu kết nối, kết nối thành công, gửi dữ liệu, nhận dữ liệu,...)
	- Có thể xử lý tập trung tất cả các socket trong cùng một thread (tối đa 1024)
	- Các socket cần thăm dò được đặt trong cấu trúc `fd_set`:
	```c++
	struct fd_set {
		u_int fd_count;
		SOCKET fd_array[FD_SETSIZE];
	};
	```
- Hàm `select()`:
```c++
/**
 * 3 tham số kiểu fd_set * không thể cùng NULL
 * @return SOCKET_ERROR (lỗi), 0 (time-out), hoặc tổng số socket có trạng thái sẵn sàng / có lỗi
 */
int select(
	int nfds, 			// 0
	fd_set *readfds, 	// [i/o] tập các socket được thăm dò trạng thái có thể đọc
	fd_set *writefds, 	// [i/o] tập các socket được thăm dò trạng thái có thể ghi
	fd_set *exceptfds, 	// [i/o] tập các socket được thăm dò trạng thái có lỗi
	const struct timeval *timeout // thời gian chờ trả về
);
```
- Kỹ thuật thăm dò:
	- Thao tác với `fd_set` qua các macro: `FD_CLR()`, `FD_SET()`, `FD_ISSET()`, `FD_ZERO()`.
	- Kỹ thuật:
		- Bước 1: thêm các socket cần thăm dò vào tập `fd_set`.
		- Bước 2: gọi hàm `select()`. Khi đó các socket không mang trạng thái thăm dò sẽ bị xóa khỏi tập `fd_set` tương ứng.
		- Bước 3: sử dụng macro FD_ISSET() để sự có mặt của socket trong tập `fd_set` và xử lý.
	- Các trạng thái trên socket được ghi nhận:
		- Tập `readfds`:
			- Có yêu cầu kết nối tới socket đang ở trạng thái lắng nghe (LISTENING).
			- Dữ liệu sẵn sàng trên socket để đọc.
			- Kết nối bị đóng / reset / hủy.
		- Tập `writefds`:
			- Kết nối thành công khi gọi hàm `connect()` ở chế độ non-blocking.
			- Sẵn sàng gửi dữ liệu.
		- Tập `exceptfds`:
			- Kết nối thất bại khi gọi hàm `connect()` ở chế độ non-blocking.
			- Có dữ liệu OOB (out-of-band) để đọc.
	- Sơ đồ ở [trang 14](https://users.soict.hust.edu.vn/tungbt/it4060/Lec03.IOMode.pdf).

## Kỹ thuật vào ra theo thông báo

Kinh dị và không thi đến (bài duy nhất lập trình với Win32 API). Bỏ qua.

## Kỹ thuật vào ra theo sự kiện

Kinh dị và không thi đến (đùa chứ đã làm bài tập với `WSAAsyncSelect()` rồi, cái này là `WSAEventSelect()`). Bỏ qua.

## Kỹ thuật vào ra overlapped

- Sử dụng cấu trúc WSAOVERLAPPED chứa thông tin về các thao tác vào ra.
- Socket phải được khởi tạo với cờ điều kiển tương ứng.
- Sử dụng các hàm đặc trưng với kỹ thuật overlapped.
- Hiệu năng cao hơn do có thể gửi đồng thời nhiều yêu cầu tới hệ thống.
- Các phương pháp xử lý kết quả:
	- Đợi thông báo từ một sự kiện.
	- Thực hiện một thủ tục CALLBACK (completion routine).
- Hàm `WSASocket()` để khởi tạo socket. Để socket ở chế độ overlapped, gán cờ WSA_FLAG_OVERLAPPED cho tham số `dwFlags`.
- Hàm `WSASend()` gửi dữ liệu với cơ chế overlapped trên trên nhiều bộ đệm. Trả về 0 hoặc WSA_IO_PENDING.
- Hàm `WSARecv()` nhận dữ liệu với cơ chế overlapped trên trên nhiều bộ đệm. Trả về 0 hoặc WSA_IO_PENDING.
- Cấu trúc `WSABUF`.
- Sơ đồ à các bước sử dụng kỹ thuật overlapped - xử lý qua sự kiện: [trang 10](https://users.soict.hust.edu.vn/tungbt/it4060/Lec03.IOMode(cont).pdf).
- Hàm `WSAGetOverlappedResult()` lấy kết quả thực hiện thao tác vào ra trên socket.

## Overlapped I/O – Completion routine
- Hệ thống sẽ thông báo cho ứng dụng biết thao tác vào ra kết thúc thông qua hàm `CompletionROUTINE()`.
- WinSock sẽ bỏ qua trường `event` trong cấu trúc OVERLAPPED, việc tạo đối tượng event và thăm dò là
không cần thiết nữa.
- Lưu ý: completion routine không thực hiện được các tác vụ nặng.
- Ứng dụng cần chuyển luồng sang trạng thái alertable ngay sau khi gửi yêu cầu vào ra. Sử dụng hàm `WSAWaitForMultipleEvents()` (hoặc `SleepEx()` nếu ứng dụng không có đối tượng event nào).
- Sơ đồ: [trang 14](https://users.soict.hust.edu.vn/tungbt/it4060/Lec03.IOMode(cont).pdf)
