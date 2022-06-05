Lỗi LNK2019: kiểm tra `#pragma comment(lib, "ws2_32.lib")`.

Khi debug có thể in thông báo về thông tin của client, chẳng hạn như IP và port. Tuy nhiên bản final không nên có vì [thông tin in ra cần phải lưu trữ trong biến kiểu `char *`](https://stackoverflow.com/questions/21620752/display-a-variable-in-messagebox-c).
