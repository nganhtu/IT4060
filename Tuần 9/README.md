**Issue:** phần xử lý `if (sockEvent.lNetworkEvents & FD_CLOSE)` trong code mẫu chưa tốt khiến cho server ngừng hoạt động khi một client ngắt kết nối.

**Solution:** xóa bỏ mã lỗi trên `sockEvent`: `sockEvent.iErrorCode[FD_CLOSE_BIT] = 0;`
