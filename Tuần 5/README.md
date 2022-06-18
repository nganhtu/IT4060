Tuần này không có gì để làm. Bài tập tuần xong từ tuần trước rồi :D

Câu 3 quiz 2 và câu 2 quiz 3 xung đột? Câu hỏi về những cách gán địa chỉ IP cho biến `addr` có kiểu `sockaddr_in`. Trong trường hợp sau đây:
```c++
addr.sin_addr.s_addr = htonl("192.168.1.1");
```
kết quả là sai (câu 3 quiz 2), trong khi đó:
```c++
addr.sin_addr.s_addr = htonl(INADDR_ANY);
```
kết quả lại là đúng (?!) (câu 2 quiz 3)
