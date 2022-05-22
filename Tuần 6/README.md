Đề bài tập tuần copy từ tuần trước hơi ảo ma.

---

Tập `readfds` chứa các socket được thăm dò ở trạng thái có thể đọc. Trong code mẫu và bài tập tuần, đó là `listenSock` và các mảng các `connSock`. Khi hàm `select()` thực thi, các socket không
mang trạng thái đọc sẽ bị xóa khỏi tập `readfds`. Sau đó sử dụng macro `FD_ISSET()` để kiểm tra sự có mặt của socket trong tập `readfds` và xử lý.

Lưu ý: biến `nEvents` lưu kết quả của hàm `select()`, nói cách khác lưu tổng số socket (cả `listenSock` và `connSock`) ở trong trạng thái có thể đọc.

---

**Issue:** `strcpy_s(sendBuff, BUFF_SIZE, recvBuff);` gây lỗi buffer size is too small trong khi `memcpy_s(sendBuff, BUFF_SIZE, recvBuff, BUFF_SIZE);` hoạt động bình thường.
