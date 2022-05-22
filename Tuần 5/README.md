**Finding:** Dùng hàm `gets_s()` có thể nhận xâu có chứa khoảng trắng.

---

**Issue:** Error 10054: cannot receive data ở server khi client ngắt kết nối bằng ^C.

**Description:** Ở mã nguồn TCP_Echo_Client.cpp, khi dùng ^C ở client thì server rẽ vào nhánh `ret == 0`. Sau khi cải tiến mã nguồn bằng hàm `createRequest()`, khi client ^C, server lại rẽ vào nhánh `ret == SOCKET_ERROR`.

**Finding:** Hành vi của client khi nhận tín hiệu SIGINT (Signal for Interrupt) khó có thể xác định. Vấn đề lớn nhất là client sẽ gửi thông điệp rác (request chưa hoàn chỉnh) tới server. Khi nhận tín hiệu ^C (hay ^K,...), hàm `createRequest()` ở client hành xử tương đương khi nhận kí tự `\n`.

**Solution:** ~~Thông điệp rác của mã nguồn TCP_Echo_Client.cpp là một thông điệp trắng "". Do đó cần thiết kế hàm `createRequest()` để thông điệp rác trả về là một thông điệp trắng.~~ Xem finding tiếp theo. *Issue chưa được giải thích. Có thể là do sự khác biệt giữa 2 thông điệp rác "" và "USER ".*

---

**Finding:** Hành vi gửi thông điệp rác "" của mã nguồn TCP_Echo_Client.cpp là ngoài dự tính. Server cần phải phân biệt lỗi WSAECONNRESET với các lỗi khác.

**Solution:** Cách server nhận biết và loại bỏ thông điệp rác:
```c++
else if (ret == 0 || WSAGetLastError() == WSAECONNRESET)
{
    // handle crash client
    break;
}
```

---

**Issue:** Hành vi không lường trước xảy ra khi thao tác với `std::set<const char *>`.

**Finding:** Các iterator của hàm `std::set::count()` và `std::set::erase()` (nói riêng) không thể kiểm soát.

**Solution:** Đừng dùng `std::set<const char *>`, hãy dùng `std::set<std::string>`. Cách convert từ C string sang C++ string như sau:
```c++
char c_string[4] = "uwu";
std::string s_string = c_string;
```

---
