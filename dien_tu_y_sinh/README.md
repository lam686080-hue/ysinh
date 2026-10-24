# PATIENT MONITOR - THEO DÕI NHỊP TIM & SPO2 THỜI GIAN THỰC

##Tên dự án & Tổng quan
* **Tên dự án:** Thiết bị theo dõi bệnh nhân đo Nhịp tim (BPM) & Nồng độ Oxy trong máu (SpO2) thời gian thực (Patient Monitor STM32F103).
* **Mục đích:** 
  - Thu thập tín hiệu quang thể tích (PPG - Photoplethysmography) từ mao mạch đầu ngón tay thông qua cảm biến quang học MAX30102.
  - Xử lý lọc tín hiệu số (DSP) thời gian thực: khử trôi nền DC, lọc dải thông sinh lý học (0.5Hz - 4.5Hz), bắt đỉnh tâm thu thích nghi và loại bỏ sóng dội van tim (Dicrotic Notch).
  - Tính toán chính xác các chỉ số sinh tồn: Nhịp tim (BPM) và Độ bão hòa Oxy trong máu (SpO2 %).
  - Vẽ trực quan dạng sóng mạch nảy thời gian thực (PPG Waveform) và hiển thị số đo sinh động lên màn hình màu LCD TFT ILI9341 theo tiêu chuẩn máy theo dõi bệnh nhân trong bệnh viện.

