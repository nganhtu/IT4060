**Issue:** Server sập khi client ^C ngay đầu.

**Finding:** Lỗi ở việc không khởi tạo FILE pointer dẫn đến đoạn mã `if (s->pSer != NULL) fclose(s->pSer);` gặp lỗi (do pSer mang giá trị rác khác NULL).

**Solution:** xem hàm `clearSession()` trong HW06.
