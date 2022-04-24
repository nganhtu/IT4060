Bài tập tuần này còn tồn đọng: nếu TCP_Client gửi thông điệp quá dài, TCP_Server sẽ chết. Hiện tại đang fix bằng lệnh `printf()` từng thông điệp được client gửi đi.

Tham khảo: https://stackoverflow.com/questions/29313067/why-does-inserting-a-printf-statement-make-my-function-work-correctly

Workflow:

1. Client dùng khối lệnh chứa hàm `send()` để gửi một hoặc nhiều request đến server.
2. Server dùng khối lệnh chứa hàm `recv()` để nhận một hoặc nhiều request từ client tới khi gặp delimiter ở cuối thông điệp.
3. Server xử lý từng request.
4. Sau khi xử lý xong mỗi request, server gửi trả response tương ứng tới client.
5. Client dựa trên số request mình gửi để gọi số khối lệnh chứa hàm `recv()` tương ứng.
6. 
