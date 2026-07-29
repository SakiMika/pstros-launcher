# PSTROS NDS LAUCHER BUILD

**Chơi thử nhanh game J2ME trực tiếp từ thẻ nhớ trên Nintendo DS/DSi.**  
**Quickly test and run J2ME games directly from the SD card on Nintendo DS/DSi.**

> Tên dự án được giữ nguyên là **PSTROS NDS LAUCHER BUILD**.  
> The project name is intentionally kept as **PSTROS NDS LAUCHER BUILD**.

---

## Tiếng Việt

### 1. Giới thiệu

**PSTROS NDS Laucher Build** là bản Pstros/KVM dùng để chạy nhanh các game J2ME từ file `.jar` nằm trên thẻ nhớ FAT.

Launcher phù hợp để:

- kiểm tra nhanh game có khởi động được hay không;
- thử khả năng tương thích của game với Pstros NDS;
- kiểm tra hiển thị, điều khiển và save;
- xác định game có cần chỉnh sửa riêng trước khi build standalone.

Launcher không nhúng cố định một game vào ROM. Khi khởi động, chương trình sẽ quét thẻ nhớ và hiển thị danh sách các file JAR tìm thấy.

### 2. Mục đích sử dụng

```text
Chép JAR vào thẻ nhớ
        ↓
Mở Pstro Laucher
        ↓
Chọn game
        ↓
Chạy thử nhanh
```

Bản launcher chủ yếu dùng để **kiểm tra tương thích**, không phải bản phát hành tối ưu cho từng game.

### 3. Tính năng

- Quét các file `.jar` trên FAT.
- Hiển thị danh sách game để lựa chọn.
- Đọc `META-INF/MANIFEST.MF` trong JAR.
- Tự xác định:
  - tên MIDlet;
  - class khởi động;
  - thông tin game cơ bản.
- Chạy JAR trực tiếp từ thẻ nhớ.
- Hỗ trợ cấu hình phím đơn.
- Hỗ trợ combo hai phím.
- Có tab log để kiểm tra lỗi runtime.
- Hỗ trợ RMS save khi game tương thích.
- Mỗi game sử dụng file save và file cấu hình phím riêng.

### 4. Cách sử dụng

Chép file launcher và các game JAR vào thẻ nhớ:

```text
SD:/
├── pstro_launcher.nds
├── Game1.jar
├── Game2.jar
└── j2me/
    └── Game3.jar
```

Sau đó mở:

```text
pstro_launcher.nds
```

Launcher sẽ quét FAT và hiển thị danh sách game.

Điều khiển danh sách:

```text
UP / DOWN       Chọn game
LEFT / RIGHT    Chuyển nhanh trong danh sách
A / START       Chạy game đang chọn
B               Quét lại thẻ nhớ
Touch           Chạm để chọn game
```

Sau khi game chạy, màn hình dưới sẽ chuyển sang giao diện cài phím và log.

### 5. Cấu hình phím

Các phím J2ME có thể gán:

```text
LS
RS
1
3
5
7
9
*
0
#
```

D-pad được cố định:

```text
UP    = 2
LEFT  = 4
RIGHT = 6
DOWN  = 8
```

Tab `single` dùng để gán một phím NDS cho một phím J2ME.

Tab `combo` dùng để gán tổ hợp hai phím NDS cho một phím J2ME.

Ví dụ:

```text
L + A = 9
R + A = RS
```

Cấu hình được lưu riêng cho từng game.

### 6. Save

Launcher hỗ trợ lưu dữ liệu RMS vào thẻ nhớ khi game tương thích với backend RMS của Pstros.

File save thường có dạng:

```text
fat:/<ten_game>.sav
```

Ví dụ:

```text
fat:/diamond_rush.sav
fat:/zenonia.sav
```

Lưu ý:

- không phải mọi game đều sử dụng RMS giống nhau;
- một số game có DRM hoặc RecordStore đặc biệt;
- một số game cần bộ lọc save riêng;
- save có thể hoạt động không đầy đủ trong launcher;
- không nên dùng chung save giữa các game khác nhau.

Launcher được thiết kế để kiểm tra nhanh. Với game cần save ổn định lâu dài, nên build riêng bản standalone.

### 7. Không hỗ trợ âm thanh

**PSTROS NDS Laucher Build không hỗ trợ âm thanh.**

Launcher chạy game ở chế độ tắt tiếng để:

- giảm mức sử dụng RAM;
- tránh lỗi `OutOfMemoryError`;
- tránh phải chuyển đổi âm thanh của mọi JAR trên thẻ nhớ;
- giữ launcher gọn và phù hợp cho việc kiểm tra tương thích.

Âm thanh J2ME thường sử dụng các định dạng như:

```text
MIDI
AMR
WAV
IMA-ADPCM
```

Nintendo DS không thể phát trực tiếp toàn bộ các định dạng này thông qua backend hiện tại. Nhạc và hiệu ứng cần được quét, chuyển đổi và nhúng riêng cho từng game.

### 8. Khi nào nên build standalone?

Sau khi xác nhận game chạy được trong launcher, nên build riêng bản standalone nếu muốn:

- có âm thanh;
- save ổn định hơn;
- khởi động trực tiếp vào game;
- không cần chọn JAR;
- có icon và tên ROM riêng;
- giảm lỗi do đọc JAR từ FAT;
- tối ưu cấu hình riêng cho từng game.

Bản standalone sử dụng cơ chế:

```text
1 game J2ME → 1 ROM .nds
```

Nhạc và hiệu ứng có thể được chuyển sang PCM native rồi nhúng ngoài Java heap.

### 9. Yêu cầu build

- Windows;
- devkitPro;
- devkitARM;
- libnds;
- Calico;
- Python 3.

Biến môi trường:

```text
DEVKITPRO
DEVKITARM
```

Build bằng:

```bat
clean.bat
build.bat
```

### 10. Thông tin ROM mặc định

```text
Tên ROM: Pstro Laucher
Mã ROM: J2DS
File ROM: pstro_launcher.nds
Âm thanh: Không hỗ trợ
Save: Có hỗ trợ khi tương thích
```

### 11. Báo lỗi

Khi báo lỗi, nên gửi:

- ảnh màn hình trên;
- ảnh màn hình dưới;
- file `last_build.log`;
- tên file JAR;
- file save nếu lỗi xảy ra sau khi mở lại game;
- mô tả chính xác bước gây treo hoặc thoát.

---

## English

### 1. Introduction

**PSTROS NDS Laucher Build** is a Pstros/KVM launcher designed to quickly run J2ME `.jar` files directly from FAT storage on Nintendo DS and Nintendo DSi.

The launcher is useful for:

- checking whether a game can boot;
- testing Pstros NDS compatibility;
- testing graphics, controls and save support;
- identifying game-specific patches required before creating a standalone ROM.

The launcher does not embed one fixed game inside the ROM. At startup, it scans the SD card and displays the JAR files it finds.

### 2. Intended Use

```text
Copy JAR to SD card
        ↓
Open Pstro Laucher
        ↓
Select a game
        ↓
Run a quick compatibility test
```

The launcher is primarily a **compatibility testing tool**, not an optimized final release for each game.

### 3. Features

- Scans FAT storage for `.jar` files.
- Displays a selectable game list.
- Reads `META-INF/MANIFEST.MF` from each JAR.
- Automatically detects:
  - MIDlet name;
  - main class;
  - basic game metadata.
- Runs JAR files directly from the SD card.
- Supports single-button mappings.
- Supports two-button combo mappings.
- Includes a runtime log tab.
- Supports RMS save data when compatible.
- Uses separate save and key configuration files for each game.

### 4. Usage

Copy the launcher and J2ME games to the SD card:

```text
SD:/
├── pstro_launcher.nds
├── Game1.jar
├── Game2.jar
└── j2me/
    └── Game3.jar
```

Launch:

```text
pstro_launcher.nds
```

The launcher will scan FAT storage and display the available games.

Game list controls:

```text
UP / DOWN       Select a game
LEFT / RIGHT    Move quickly through the list
A / START       Run the selected game
B               Rescan the SD card
Touch           Touch a game to select it
```

After the game starts, the lower screen changes to the key configuration and log interface.

### 5. Key Configuration

Configurable J2ME keys:

```text
LS
RS
1
3
5
7
9
*
0
#
```

The D-pad is fixed:

```text
UP    = 2
LEFT  = 4
RIGHT = 6
DOWN  = 8
```

The `single` tab assigns one Nintendo DS button to one J2ME key.

The `combo` tab assigns a two-button combination to one J2ME key.

Examples:

```text
L + A = 9
R + A = RS
```

Key settings are stored separately for each game.

### 6. Save Support

The launcher supports RMS persistence on FAT storage when the game is compatible with the Pstros RMS backend.

Save files normally use this format:

```text
fat:/<game_name>.sav
```

Examples:

```text
fat:/diamond_rush.sav
fat:/zenonia.sav
```

Important notes:

- J2ME games do not all use RMS in the same way;
- some games include DRM or custom RecordStore logic;
- some games require a game-specific save filter;
- save support may be incomplete in launcher mode;
- save files must not be shared between unrelated games.

The launcher is intended for quick testing. For reliable long-term save support, create a dedicated standalone build.

### 7. No Audio Support

**PSTROS NDS Laucher Build does not support audio.**

Games are launched in muted mode to:

- reduce RAM usage;
- avoid `OutOfMemoryError`;
- avoid converting audio for every JAR on the SD card;
- keep the launcher small and suitable for compatibility testing.

J2ME games commonly use formats such as:

```text
MIDI
AMR
WAV
IMA-ADPCM
```

Nintendo DS cannot directly play all of these formats through the current backend. Music and sound effects must be scanned, converted and embedded separately for each game.

### 8. When Should You Build a Standalone ROM?

After confirming that a game works in the launcher, create a standalone build for a better experience:

- audio support;
- more reliable save handling;
- direct game boot;
- no JAR selection screen;
- custom ROM name and icon;
- fewer FAT JAR loading issues;
- game-specific optimizations.

The standalone build uses this model:

```text
1 J2ME game → 1 .nds ROM
```

Music and sound effects can be converted to native PCM and linked outside the Java heap.

### 9. Build Requirements

- Windows;
- devkitPro;
- devkitARM;
- libnds;
- Calico;
- Python 3.

Required environment variables:

```text
DEVKITPRO
DEVKITARM
```

Build with:

```bat
clean.bat
build.bat
```

### 10. Default ROM Information

```text
ROM name: Pstro Laucher
ROM code: J2DS
ROM file: pstro_launcher.nds
Audio: Not supported
Save: Supported when compatible
```

### 11. Reporting Problems

When reporting a problem, include:

- a photo of the upper screen;
- a photo of the lower screen;
- `last_build.log`;
- the JAR filename;
- the save file when the issue occurs after reopening the game;
- the exact step that causes a freeze or exit.

---

## Project Status / Trạng thái dự án

This project is experimental. J2ME compatibility varies between games.

Dự án đang trong giai đoạn thử nghiệm. Mức độ tương thích phụ thuộc vào từng game J2ME.

The launcher is designed for quick testing. For the best gameplay experience, create a dedicated standalone build.

Launcher được thiết kế để kiểm tra nhanh. Để có trải nghiệm tốt nhất, hãy build riêng bản standalone cho từng game.
