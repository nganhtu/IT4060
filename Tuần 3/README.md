Bài tập tuần này còn tồn đọng: nếu TCP_Client gửi thông điệp quá dài, TCP_Server sẽ chết. Hiện tại đang fix bằng cách gọi lệnh `printf()` với từng thông điệp được client gửi đi.

Tham khảo: https://stackoverflow.com/questions/29313067/why-does-inserting-a-printf-statement-make-my-function-work-correctly

Workflow ứng dụng socket TCP có xử lý truyền dòng:

1. Client dùng (các) khối lệnh chứa hàm `send()` để gửi một, nhiều, hoặc từng phần request đến server.
2. Server dùng 1 khối lệnh chứa hàm `recv()` để nhận một hoặc nhiều request từ client, dừng lại khi gặp delimiter ở cuối thông điệp.
3. Server xử lý từng request.
4. Sau khi xử lý xong mỗi request, server dùng 1 khối lệnh chứa hàm `send()` để gửi trả response tương ứng tới client.
5. Client dựa trên số request mình gửi để gọi số khối lệnh chứa hàm `recv()` tương ứng.

Xem file test1.cpp trong bài tập tuần để hiểu rõ hơn hành vi của client.

Rõ ràng có sơ hở: nếu nhiều request của client ngẫu nhiên bị tách khiến delimiter nằm cuối một thông điệp nào đó, server sẽ không nhận được phần thông điệp còn lại. Tuy nhiên, lỗi này chỉ xảy ra khi có NHIỀU request được chia ra gửi NHIỀU lần. Vì test của thầy không có trường hợp đó (chỉ có (1) nhiều request gửi 1 lần và (2) 1 request chia ra gửi nhiều lần) nên có thể tạm bỏ qua lỗi này.
