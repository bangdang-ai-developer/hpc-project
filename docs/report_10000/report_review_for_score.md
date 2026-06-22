# Review báo cáo để tối đa điểm

## Kết luận nhanh

Báo cáo hiện tại đã có nền tảng tốt: có bài toán rõ, mô tả `mpi_2d`, cấu hình 3 node, bảng số liệu 10000^3, biểu đồ runtime/GFLOP/s/speedup/efficiency, checksum và phân tích vì sao p9 tốt hơn p44. Đây là phần mạnh nhất cho mục "Kết quả thực nghiệm và đánh giá hiệu năng".

Tuy nhiên, để đạt điểm cao theo đề bài, cần chỉnh 4 điểm chính:

1. Báo cáo phải bám sát rubric hơn, đặc biệt Speedup/Efficiency theo công thức T(1)/T(p).
2. Cần có mục riêng đánh giá theo số node 1/2/3, vì đề bài yêu cầu rõ.
3. Không nên gọi speedup p4-relative là speedup tuyệt đối; phải ghi là "scaling tương đối theo mốc p4" nếu chưa có T(1) cho 10000^3.
4. PDF sau chỉnh sửa còn 11 trang tổng cộng, trong đó phần chính kết thúc ở trang 10 và phụ lục bắt đầu từ trang 11. Cách này phù hợp yêu cầu "không quá 10 trang, không kể phụ lục".

## Đối chiếu rubric

| Tiêu chí đề bài | Trọng số | Mức hiện tại | Cách tăng điểm |
|---|---:|---|---|
| Cơ sở lý thuyết và phân tích thuật toán | 15% | Có công thức Cij và O(MKN), nhưng còn ngắn | Thêm phân tích FLOP, bộ nhớ A/B/C, row decomposition vs 2D decomposition |
| Thiết kế giải pháp MPI/Open MPI | 20% | Khá tốt, có `mpi_2d`, hostfile, rank 0 pack/send | Thêm sơ đồ data flow hoặc bảng vai trò rank 0/rank worker |
| Cài đặt chương trình | 25% | Có mô tả variant, checksum, CLI | Thêm bảng public CLI, input/output CSV, cách validate RAM, cách seed deterministic |
| Kết quả thực nghiệm và đánh giá hiệu năng | 25% | Mạnh ở process sweep 10000^3 | Bổ sung node sweep 1/2/3 hoặc ghi rõ thiếu; tách Result và Discussion |
| Báo cáo và trình bày | 15% | PDF đẹp, có biểu đồ, main report 10 trang | Giữ phần chính cô đọng, để lệnh/log dài trong phụ lục |

## Các vấn đề cần chú ý trước khi nộp

### 1. Speedup/Efficiency chưa hoàn toàn đúng công thức đề bài

Đề bài định nghĩa:

```text
S(p) = T(1) / T(p)
E(p) = S(p) / p
```

Báo cáo hiện tại dùng p4 làm baseline cho 10000^3 vì không chạy T(1). Cách này hợp lý về thực tế, nhưng khi trình bày phải gọi là "speedup tương đối theo mốc p4" hoặc "scaling tương đối", không nên để người chấm hiểu là speedup chuẩn T(1)/T(p).

Khuyến nghị:

- Giữ bảng 10000^3 làm kết quả chính vì dữ liệu lớn và đẹp.
- Thêm một bảng nhỏ "Speedup chuẩn trên dữ liệu kiểm chứng" nếu có thể chạy thêm p1/p2/p4/p8/p16 với shape vừa phải.
- Nếu không chạy thêm, viết rõ: "Do T(1) của 10000^3 không khả thi trong thời gian thí nghiệm, báo cáo dùng p4 làm mốc scaling tương đối; đây là hạn chế thực nghiệm."

### 2. Thiếu node sweep 1/2/3 node

Đề bài yêu cầu đánh giá theo số node:

```text
1 Node
2 Nodes
3 Nodes
```

Báo cáo hiện tại chủ yếu là process sweep trên 3 node. Để ăn điểm phần này, cần thêm một bảng/biểu đồ node sweep, ví dụ cùng shape và cùng PPN:

```text
1 node: p3 hoặc p4
2 node: p6 hoặc p8
3 node: p9 hoặc p12
```

Nếu không chạy thêm được, nên có một đoạn trung thực trong "Hạn chế thực nghiệm": cluster WSL qua Wi-Fi chạy được smoke 3 node, nhưng các workload cực lớn 15000^3/20000^3 gặp TCP timeout; kết quả node scaling đầy đủ vì vậy chưa đủ mạnh.

### 3. Cần tách rõ Result và Discussion

Theo chuẩn báo cáo khoa học/kỹ thuật, phần Results nên trình bày dữ kiện khách quan trước; phần Discussion mới giải thích ý nghĩa. Báo cáo hiện tại đã có cấu trúc gần đúng, nhưng nên làm rõ hơn:

- "Kết quả thực nghiệm": chỉ bảng, biểu đồ, checksum, runtime, GFLOP/s.
- "Phân tích": vì sao p9 tốt, vì sao p44 chậm, bottleneck rank 0.
- "Threats to validity / Hạn chế": WSL, Wi-Fi, không có filesystem chung, không có T(1) cho 10000^3.

### 4. Page limit

PDF hiện có 11 trang tổng cộng. Phần phụ lục bắt đầu ở trang 11, nên phần chính của báo cáo là 10 trang.

Các chỉnh sửa đã áp dụng:

- Bỏ `\tableofcontents`.
- Chuyển lệnh kiểm tra mapping và thử nghiệm 15000/20000 sang phụ lục.
- Giữ bảng đối chiếu yêu cầu đề bài ở phần chính để người chấm nhìn thấy trực tiếp.

### 5. Bảng "Đối chiếu yêu cầu đề bài"

Báo cáo đã thêm một bảng ngắn gần cuối phần chính để người chấm thấy nhóm đã đáp ứng yêu cầu:

| Yêu cầu | Minh chứng trong báo cáo |
|---|---|
| Chương trình tuần tự | `seq`, `seq_tiled` |
| Chương trình song song MPI | `mpi`, `mpi_tiled`, `mpi_2d` |
| Dữ liệu lớn | 10000^3, 2e12 FLOP |
| Đổi số processes | p4, p9, p12, p16, p25, p36, p44 |
| Đổi số nodes | Cần bổ sung hoặc ghi hạn chế |
| Execution time | Bảng runtime + biểu đồ |
| Speedup/Efficiency | Có scaling tương đối; cần ghi rõ baseline |
| Tính đúng | Checksum hash giống nhau |

## Cách trình bày để "ăn điểm"

- Mỗi biểu đồ phải có caption nói thẳng insight, không chỉ mô tả trục.
- Mỗi bảng nên có 1-2 câu kết luận ngay bên dưới.
- Không giấu kết quả xấu: p44 chậm hơn p9 là điểm phân tích tốt, vì chứng minh nhóm hiểu overhead MPI.
- Không overclaim: WSL + Wi-Fi không phải HPC cluster thật, nên báo cáo nên nói đây là "cụm thực nghiệm giáo dục".
- Nhấn mạnh reproducibility: seed, shape, variant, host mapping, command, checksum file.
- Phần kết luận nên trả lời 3 câu: thuật toán nào chạy đúng, cấu hình nào tốt nhất, vì sao kết quả đó xảy ra.

## Trạng thái chỉnh báo cáo

Các mục đã xử lý trong bản PDF hiện tại:

1. Đã thêm bảng đối chiếu đề bài.
2. Đã đổi tên biểu đồ speedup thành "Speedup tương đối so với p4".
3. Đã thêm đoạn hạn chế về việc chưa có T(1) cho 10000^3.
4. Đã thêm phụ lục về thử nghiệm 15000^3/20000^3: 3 node smoke OK, nhưng workload lớn fail do Open MPI TCP timeout trên WSL/LAN; không dùng làm kết quả chính.
5. Đã rút phần chính còn 10 trang bằng cách bỏ mục lục và chuyển lệnh dài sang phụ lục.
6. Đã sửa biểu đồ PNG để không còn lỗi font tiếng Việt trong ảnh; nhãn chart hiện dùng tiếng Việt và caption LaTeX diễn giải insight chính.
7. Đã đồng bộ báo cáo Word `docs/BaoCao_DeTai1_NhanMaTran_MPI.docx` với kết quả cuối, thay cho bản cũ còn ghi chờ cập nhật 3 node.
8. Đã thêm nhãn rõ hơn cho mục "Giới thiệu và cơ sở lý thuyết" và dòng đối chiếu "Cơ sở lý thuyết và độ phức tạp" trong bảng rubric.
9. Đã sửa trang bìa, caption "Bảng/Hình", grid p12/p44 theo log/code thật, kết luận p9/p12 thận trọng hơn và thêm mục tài liệu tham khảo cuối báo cáo.

Mục còn thiếu nếu muốn nâng điểm thêm là node sweep 1/2/3 node hoặc một benchmark kiểm chứng nhỏ hơn có đủ T(1), T(2), T(4), T(8), T(16) để tính speedup chuẩn theo đúng công thức đề bài.

## Nguồn tham khảo kỹ năng viết báo cáo

- University of Toronto, "The Lab Report": https://advice.writing.utoronto.ca/types-of-writing/lab-report/
- San Jose State University Writing Center, "Results Section": https://www.sjsu.edu/writingcenter/docs/handouts/Results%20Section%20for%20Research%20Papers.pdf
- Hoefler và Belli, "Scientific Benchmarking of Parallel Computing Systems": https://htor.inf.ethz.ch/publications/img/hoefler-scientific-benchmarking.pdf
- Colorado School of Mines Research Computing, "Parallel Scaling Guide": https://rc-docs.mines.edu/pages/user_guides/Parallel_Scaling_Guide.html
