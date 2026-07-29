Pstro Laucher - J2ME FAT compatibility tester for Nintendo DS
ROM code: J2DS
Output: pstro_launcher.nds
Audio: disabled

MUC DICH
- Test nhanh J2ME JAR tren may DS that truoc khi tao ROM standalone.
- Quet file .jar tren FAT/SD, ke ca cac thu muc con toi da 5 cap.
- Doc META-INF/MANIFEST.MF trong JAR, lay MIDlet-Name va MIDlet-1.
- Sau khi chon game, man hinh duoi doi sang 3 tab single/combo/log.

CACH BUILD
1. Chay build.bat.
2. ROM tao ra: pstro_launcher.nds.
3. Chep ROM va cac file J2ME .jar vao the nho.

CACH DUNG
- UP/DOWN: chon game.
- LEFT/RIGHT: chuyen nhanh mot trang.
- A hoac START: chay game dang chon.
- B: quet lai FAT.
- Cam ung: cham dong de chon, cham lai dong do de chay.

CAI PHIM SAU KHI CHON GAME
- Tab single: cham dong, sau do bam 1 phim vat ly.
- Tab combo: cham dong, sau do bam dong thoi 2 phim vat ly.
- Tab log: xem log gan nhat.
- D-pad co dinh: 2/4/6/8.
- Phim co the gan: LS, RS, 1, 3, 5, 7, 9, *, 0, #.
- Cau hinh luu rieng theo ten JAR: fat:/<ten_game>.keys.

SAVE
- Launcher thu luu RMS rieng theo ten JAR: fat:/<ten_game>.sav.
- Khong tao file save rong khi chi kiem tra quyen ghi.
- Bo loc RMS rieng cua Diamond Rush da tat; launcher giu tat ca RecordStore.
- Save con phu thuoc cach game su dung RMS va muc do tuong thich cua game.

GIOI HAN
- Khong ho tro am thanh. MIDlet luon duoc khoi dong voi -mute.
- Chi chay JAR co manifest hop le va co MIDlet-1.
- JAR nen la ban J2ME/CLDC da preverify dung chuan.
- Sau khi MIDlet thoat, can khoi dong lai launcher de chon game khac.

FAT scan compatibility fix:
- JAR files are accepted directly from readdir(); stat() is no longer required.
- Recursion uses opendir() so it works on DLDI implementations where stat("fat:/...") fails.
- Scan order: fat:/, ROM directory, current directory, /, fat:, sd:/.
- If no JAR is found, the screen shows scanned directory/entry counts and cwd for diagnosis.

Update - list display fix:
- Launcher list is now printed sequentially after FAT scan.
- Avoids cursor-positioned blank rows seen on some real DS console setups.
- Selected JAR is prefixed by '>'.
