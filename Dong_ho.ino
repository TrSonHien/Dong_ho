#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include "pitches.h"

// Khởi tạo đối tượng RTC và LCD
RTC_DS1307 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Đảm bảo địa chỉ I2C của LCD là 0x27, nếu không hãy thay đổi cho phù hợp.

// Định nghĩa các chân cho nút bấm
#define BTN_MENU 5
#define BTN_OK   17
#define BTN_UP   16
#define BTN_DOWN 4
#define BUZZER_PIN 2

// --- Các Enum cho Quản lý Trạng thái Hệ thống ---

// Các trạng thái chính của menu
enum MenuState {
  NORMAL,                 // Trạng thái hiển thị giờ/ngày mặc định
  MAIN_MENU,              // Menu chính với các lựa chọn: Chỉnh giờ, Hẹn giờ, Bấm giờ, Đặt lại RTC

  SET_TIME_DATE_MODE,     // Menu con: Chọn giữa chỉnh Ngày/Tháng/Năm hoặc Giờ/Phút/Giây
  EDIT_TIME,              // Chế độ chỉnh sửa chi tiết từng phần (ngày, tháng, năm, giờ, phút, giây)

  TIMER_ALARM_SELECT,
  ALARM_MENU,             // Menu cài đặt/quản lý hẹn giờ
  ALARM_SET_TIME,         // Cài đặt giờ/phút cho báo thức
  ALARM_TOGGLE,           // Bật/tắt báo thức
  TIMER_MENU,             // Menu chính cho Đếm ngược
  TIMER_SET_DURATION,     // Cài đặt thời gian đếm ngược
  TIMER_RUNNING,          // Đếm ngược đang chạy
  ALARM_SOUNDING,         // Trạng thái báo thức đang kêu

  STOPWATCH_MENU,         // Menu chức năng bấm giờ 
  STOPWATCH_DISPLAY,      // Hiển thị thời gian bấm giờ 

  RESET_RTC_CONFIRM       // Màn hình xác nhận đặt lại thời gian RTC về mặc định
};

// Chế độ chỉnh sửa thời gian (đang chỉnh ngày hay giờ)
enum SetTimeMode {
  MODE_NONE, // Không chỉnh gì
  SET_DATE,  // Đang chỉnh ngày
  SET_TIME   // Đang chỉnh giờ
};

// Các bước con khi chỉnh sửa chi tiết
enum SubStep {
  STEP_DAY,
  STEP_MONTH,
  STEP_YEAR,
  STEP_HOUR,
  STEP_MINUTE,
  STEP_SECOND,
  STEP_DONE  // Đã hoàn thành tất cả các bước chỉnh sửa trong chế độ hiện tại
};

// --- Các Biến Toàn Cục ---

// Biến lưu trạng thái hiện tại của hệ thống và các chế độ con
MenuState menuState = NORMAL;
SetTimeMode setMode = MODE_NONE;
SubStep subStep = STEP_DONE;

// Biến lưu trữ giá trị ngày/giờ đang được chỉnh sửa
int setDay = 1, setMonth = 1, setYear = 2025;
int setHour = 0, setMinute = 0, setSecond = 0;

// Biến lưu chỉ mục của tùy chọn đang được chọn trong menu (ví dụ: trong MAIN_MENU)
int selectOptionIndex = 0;
//------------------------------------------------------------------------------------------------------------------
const char* daysOfTheWeek[] = {
  "CN", "T2", "T3", "T4", "T5", "T6", "T7"
};
//------------------------------------------------------------------------------------------------------------------
// Giai điệu và độ dài nốt
int alarmMelody[] = {
  NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5, NOTE_G4, NOTE_E4, NOTE_C4
};
int alarmNoteDurations[] = {
  8, 8, 8, 4, 8, 8, 4
};
const int NUM_ALARM_NOTES = sizeof(alarmMelody) / sizeof(alarmMelody[0]);
//------------------------------------------------------------------------------------------------------------------
// Mảng chứa các tùy chọn cho menu chọn Báo thức/Đếm ngược
const char* timerAlarmSelectOptions[] = {
  "1. Bao Thuc",
  "2. Dem Nguoc"
};
const int numTimerAlarmSelectOptions = sizeof(timerAlarmSelectOptions) / sizeof(timerAlarmSelectOptions[0]);

// Mảng chứa các chuỗi tùy chọn cho ALARM_MENU
const char* alarmMenuOptions[] = {
  "1. Cai Bao Thuc",
  "2. Bat/Tat Bao Thuc"
};
const int numAlarmMenuOptions = sizeof(alarmMenuOptions) / sizeof(alarmMenuOptions[0]);

// Mảng chứa các chuỗi tùy chọn cho TIMER_MENU
const char* timerMenuOptions[] = {
  "1. Cai Dem Nguoc",
  "2. Bat Dau/Dung",
  "3. Dat Lai"
};
const int numTimerMenuOptions = sizeof(timerMenuOptions) / sizeof(timerMenuOptions[0]);

// --- Biến cho chức năng Hẹn giờ báo thức (Alarm) ---
int alarmHour = 7;        // Giờ báo thức mặc định
int alarmMinute = 0;      // Phút báo thức mặc định
bool alarmEnabled = false; // Trạng thái báo thức (bật/tắt)
bool alarmTriggered = false; // Cờ báo hiệu báo thức đã kích hoạt

// --- Biến cho chức năng Hẹn giờ đếm ngược (Timer) ---
unsigned long timerDuration = 0; // Thời gian đếm ngược (tính bằng mili giây)
unsigned long timerStartTime = 0; // Thời điểm bắt đầu đếm ngược (millis())
unsigned long timerRemainingTime = 0; // Thời gian còn lại khi tạm dừng
bool timerRunning = false;     // Trạng thái đếm ngược (đang chạy/dừng)
bool confirmMessageDisplayed = false;
bool editTimeScreenDisplayed = false;

bool timerAlarmSounding = false; 
unsigned long timerAlarmStartTime = 0; // Thời điểm bắt đầu kêu báo thức/timer
const unsigned long ALARM_TONE_DURATION_MS = 30000; // Thời gian kêu báo thức: 30 giây (30000 ms)

unsigned long alarmSoundingStartTime = 0; // Thời điểm bắt đầu kêu báo thức
const unsigned long ALARM_SOUND_DURATION_MS = 30000; // 30 giây
bool alarmCurrentlySounding = false; // Cờ báo hiệu báo thức đang kêu (để kiểm soát việc phát tone lặp lại)

static long prevDisplayedHours = -1;
static long prevDisplayedMinutes = -1;
static long prevDisplayedSeconds = -1;
static bool hetGioMessageDisplayed = false;
static bool alarmMessageDisplayed = false;
static long prevDisplayedDay = -1;
static long prevDisplayedMonth = -1;
static long prevDisplayedYear = -1;
static long prevDisplayedDayOfWeek = -1;
static int currentAlarmNoteIndex = 0; // Chỉ số nốt hiện tại đang chơi
static unsigned long nextAlarmNoteTime = 0; // Thời điểm nốt tiếp theo sẽ bắt đầu
static bool inAlarmRestPeriod = false;
static int currentBeepCount = 0; // Đếm số tiếng 'tút' đã phát
static unsigned long nextBeepTime = 0; // Thời điểm tiếng 'tút' tiếp theo sẽ phát
static bool isTonePlaying = false; // Cờ cho biết tiếng 'tút' đang phát hay không
static bool inTimerRestPeriod = false; // Cờ mới: đang trong thời gian nghỉ sau khi hoàn thành 3 tiếng 'tút'
const unsigned long TIMER_SEQUENCE_REST_INTERVAL_MS = 1000; // Khoảng nghỉ 1 giây giữa các chuỗi "tút tút tút"

//------------------------------------------------------------------------------------------------------------------
const char* stopwatchMenuOptions[] {
  "Start",
  "Stop",
  "Reset",
  "Lap"
};

const int numStopwatchMenuOptions = sizeof(stopwatchMenuOptions) / sizeof(stopwatchMenuOptions[0]);

bool stopwatchRunning = false;  // true nếu bấm giờ đang chạy, false nếu đang dừng/reset
unsigned long startTime = 0;    // Thời điểm millis() khi bấm giờ bắt đầu hoặc tiếp tục
unsigned long pausedTime = 0;   // Tổng thời gian đã trôi qua khi bấm giờ đang tạm dừng
unsigned long lapTime = 0;      // Thời gian vòng cuối cùng (nếu có)
//------------------------------------------------------------------------------------------------------------------
// Mảng chứa các chuỗi tùy chọn cho MAIN_MENU
const char* mainMenuOptions[] = {
  "1. Hen Gio",         
  "2. Bam Gio",         
  "3. Chinh Thoi Gian", 
  "4. Dat Lai RTC"      
};
// Tính số lượng tùy chọn trong mảng mainMenuOptions
const int numMainMenuOptions = sizeof(mainMenuOptions) / sizeof(mainMenuOptions[0]);

// Biến phục vụ chức năng chống dội nút (debounce)
unsigned long lastButtonPressTime = 0;
const long debounceDelay = 200; // Thời gian tối thiểu giữa các lần nhận diện nút nhấn (miliseconds)

// Biến phục vụ hiệu ứng nhấp nháy trên LCD
unsigned long lastBlinkTime = 0;
const long blinkInterval = 250; // Khoảng thời gian tắt/bật để nhấp nháy (miliseconds)
bool blinkOn = false; // Trạng thái hiện tại của nhấp nháy (true: hiển thị, false: ẩn)

// --- Khai báo Nguyên mẫu Hàm (Function Prototypes) ---
// Việc này giúp các hàm có thể gọi nhau mà không cần quan tâm thứ tự định nghĩa
void showClock();
void handleMainMenu();
void handleSetTimeDateMode();
void handleEditTime();
void handleTimerAlarmSelect();
void handleAlarmMenu();
void handleAlarmSetTime();
void handleAlarmSounding();
void handleAlarmToggle();
void handleTimerMenu();
void handleTimerSetDuration();
void handleTimerRunning();
void checkAlarm();
void handleStopwatchMenu();
void displayStopwatchTime();
void handleResetRTCConfirm();
void playAlarmToneNonBlocking();
void playTimerToneNonBlocking();
void adjustTime(int dir);
void nextStep();
bool isButtonPressed(int buttonPin);
int daysInMonth(int month, int year);

// --- Định nghĩa ký tự khối đen ---
// Ký tự khối đen (hoặc đầy đủ)
byte fullBlock[8] = {
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};

// =======================================================
// --- Hàm Setup: Chạy một lần khi khởi động Arduino ---
// =======================================================
void setup() {
  Wire.begin();       // Khởi tạo giao tiếp I2C
  lcd.init();         // Khởi tạo màn hình LCD
  lcd.backlight();    // Bật đèn nền LCD
  Serial.begin(115200); // Khởi tạo giao tiếp Serial cho mục đích gỡ lỗi (debug)

   // Thiết lập các chân nút bấm là INPUT_PULLUP (sử dụng điện trở kéo lên bên trong)
  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // Kiểm tra và khởi tạo module RTC DS1307
  if (!rtc.begin()) {
    lcd.print("Loi DS1307"); // Hiển thị lỗi trên LCD
    Serial.println("ERROR: Khong tim thay DS1307!");
    while (1); // Dừng chương trình nếu không tìm thấy RTC
  }
  if (!rtc.isrunning()) {
    lcd.print("Dat lai thoi gian!"); // Thông báo trên LCD
    Serial.println("RTC chua chay, dat lai thoi gian!");
    // Đặt thời gian RTC về thời gian khi biên dịch code
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    delay(2000); // Giữ thông báo hiển thị 2 giây
  }

  Serial.println("--- Khoi dong he thong ---");
  lcd.clear();
  lcd.createChar(0, fullBlock);

  lcd.setCursor(0, 0);
  lcd.print("LOADING SYSTEM..."); // Dòng 1: Thông báo chính

  // Hiệu ứng Loading bằng cách "tô đen" dần dòng 2
  const int totalColumns = 16; // Tổng số cột của màn hình LCD (16 cho màn 16x2)
  const int fillDelay = 300;   // Độ trễ giữa mỗi bước lấp đầy (miligiây)

  lcd.setCursor(0, 1); // Đặt con trỏ ở đầu dòng 2
  for (int i = 0; i < totalColumns; i++) { 
    lcd.write(byte(0)); 
    delay(fillDelay);    
  }

  // --- Sau khi Loading hoàn tất ---
  lcd.clear(); // Xóa màn hình loading
  lcd.setCursor(0, 0);
  lcd.print("  HE THONG DA   "); // Thông báo sẵn sàng
  lcd.setCursor(0, 1);
  lcd.print("   SAN SANG!    ");
  delay(1500); // Giữ thông báo này trong 1.5 giây

  Serial.println("Khoi dong RTC thanh cong.");
  Serial.println("--- San sang hoat dong ---");
}

// ==================================================
// --- Hàm Loop: Chạy lặp đi lặp lại vô thời hạn ---
// ==================================================
void loop() {
  // --- Điều khiển chuyển trạng thái chính bằng nút MENU ---
  // Nút MENU có hai chức năng:
  // 1. Từ trạng thái NORMAL -> vào MAIN_MENU
  // 2. Từ bất kỳ menu con nào -> thoát về NORMAL (như nút BACK)
  if (isButtonPressed(BTN_MENU)) {
    if (menuState == NORMAL) {
      Serial.println("Chuyen trang thai: NORMAL -> MAIN_MENU");
      menuState = MAIN_MENU;
      selectOptionIndex = 0; // Đặt lại lựa chọn về đầu menu chính
      lcd.clear(); // Xóa màn hình để hiển thị menu mới
    } else {
      Serial.println("Thoat menu, chuyen trang thai ve NORMAL");
      menuState = NORMAL;
      resetClockDisplayFlags();
      lcd.clear(); // Xóa màn hình và hiển thị giờ bình thường
      blinkOn = false; // Tắt hiệu ứng nhấp nháy
      stopwatchRunning = false; // Bấm giờ dừng khi thoát về menu
      timerRunning = false; // Dừng đếm ngược khi thoát về Normal
      alarmTriggered = false; // Đảm bảo cờ báo thức tắt khi thoát
    }
  }

  // --- Xử lý logic dựa trên trạng thái hiện tại của hệ thống ---
  switch (menuState) {
    case NORMAL:
      showClock(); // Hiển thị đồng hồ
      checkAlarm(); // Luôn kiểm tra báo thức
      break;
    case MAIN_MENU:
      handleMainMenu(); // Quản lý menu chính
      break;
    case SET_TIME_DATE_MODE:
      handleSetTimeDateMode(); // Quản lý menu chọn chỉnh ngày/giờ
      break;
    case EDIT_TIME:
      handleEditTime(); // Quản lý quá trình chỉnh sửa chi tiết
      break;
    case TIMER_ALARM_SELECT:  
      handleTimerAlarmSelect(); // Hàm để chọn giữa Báo thức và Đếm ngược
      break;
    case ALARM_MENU:
      handleAlarmMenu(); // Quản lý menu hẹn giờ (báo thức)
      break;
    case ALARM_SET_TIME:
      handleAlarmSetTime(); // Cài đặt giờ/phút báo thức
      break;
    case ALARM_TOGGLE:
      handleAlarmToggle(); // Bật/tắt báo thức
      break;
    case TIMER_MENU:
      handleTimerMenu(); // Menu chính Hẹn giờ đếm ngược
      break;
    case ALARM_SOUNDING: // <-- THÊM CASE NÀY VÀ GỌI HÀM MỚI
      handleAlarmSounding();
      break;  
    case TIMER_SET_DURATION:
      handleTimerSetDuration(); // Cài đặt thời gian đếm ngược
      break;
    case TIMER_RUNNING:
      handleTimerRunning(); // Đếm ngược đang chạy
      break;
    case STOPWATCH_MENU:
      handleStopwatchMenu(); // Quản lý menu bấm giờ
      break;
    case STOPWATCH_DISPLAY:
      displayStopwatchTime(); // Hiển thị bấm giờ
      break;
    case RESET_RTC_CONFIRM:
      handleResetRTCConfirm(); // Quản lý xác nhận đặt lại RTC
      break;
    default:
      Serial.println("WARNING: Trang thai menu khong xac dinh!");
      break;
  }
  delay(10); // Độ trễ nhỏ để CPU không chạy quá nhanh, giúp hệ thống ổn định và Serial dễ đọc
}

// =======================================================
// --- Các Hàm Hỗ Trợ và Quản lý Chức năng ---
// =======================================================

/**
 * @brief Kiểm tra xem một nút bấm có được nhấn hay không, có xử lý chống dội nút.
 * @param buttonPin Chân digital của nút bấm.
 * @return true nếu nút được nhấn, false nếu không.
 */
bool isButtonPressed(int buttonPin) {
  if (digitalRead(buttonPin) == LOW) { // Nút được nhấn khi chân ở mức LOW (do INPUT_PULLUP)
    if (millis() - lastButtonPressTime > debounceDelay) { // Đảm bảo đã qua thời gian debounce
      lastButtonPressTime = millis(); // Cập nhật thời gian nhấn cuối cùng
      Serial.print("Button pressed: "); // In ra Serial để debug
      Serial.println(buttonPin);
      return true;
    }
  }
  return false;
}

/**
 * @brief Tính số ngày trong một tháng cụ thể của một năm cụ thể (có xét năm nhuận).
 * @param month Tháng (1-12).
 * @param year Năm.
 * @return Số ngày trong tháng đó.
 */
int daysInMonth(int month, int year) {
  if (month == 2) { // Tháng 2
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) { // Năm nhuận
      return 29;
    } else {
      return 28;
    }
  } else if (month == 4 || month == 6 || month == 9 || month == 11) { // Tháng có 30 ngày
    return 30;
  } else { // Các tháng còn lại có 31 ngày
    return 31;
  }
}

/**
 * @brief Hiển thị thời gian và ngày hiện tại lên màn hình LCD.
 */
void showClock() {
  DateTime now = rtc.now(); // Lấy thời gian hiện tại từ RTC
  // Lấy thứ dưới dạng số và đảm bảo nó trong phạm vi hợp lệ
  int dayIndex = now.dayOfTheWeek();
  if (dayIndex < 0 || dayIndex > 6) { 
    dayIndex = 0; // Mặc định về Chủ Nhật nếu có lỗi
  }

  // --- Dòng 0: Hiển thị Thứ Ngày/Tháng/Năm ---
  // Chỉ cập nhật dòng 0 nếu có bất kỳ phần nào của ngày/tháng/năm/thứ thay đổi
  if (now.day() != prevDisplayedDay || now.month() != prevDisplayedMonth || 
      now.year() != prevDisplayedYear || dayIndex != prevDisplayedDayOfWeek) {
    
    lcd.setCursor(0, 0); // Đặt con trỏ ở đầu dòng 0

    // In THỨ (ví dụ: T2)
    lcd.print(daysOfTheWeek[dayIndex]);
    lcd.print(" "); // Khoảng cách giữa Thứ và Ngày

    // In NGÀY (DD)
    if (now.day() < 10) lcd.print("0");
    lcd.print(now.day());
    lcd.print("/");

    // In THÁNG (MM)
    if (now.month() < 10) lcd.print("0");
    lcd.print(now.month());
    lcd.print("/");

    // In NĂM (YYYY)
    lcd.print(now.year());
    
    // Xóa ký tự thừa ở cuối dòng để đảm bảo sạch sẽ
    lcd.print("   "); 

    // Cập nhật giá trị ngày/tháng/năm/thứ đã hiển thị
    prevDisplayedDay = now.day();
    prevDisplayedMonth = now.month();
    prevDisplayedYear = now.year();
    prevDisplayedDayOfWeek = dayIndex;
  }

  // --- Dòng 1: Hiển thị "Time HH:MM:SS" ---
  // Chỉ cập nhật dòng 1 nếu có bất kỳ phần nào của giờ/phút/giây thay đổi
  if (now.hour() != prevDisplayedHours || now.minute() != prevDisplayedMinutes || now.second() != prevDisplayedSeconds) {
    lcd.setCursor(0, 1); // Đặt con trỏ ở đầu dòng 1
    lcd.print("Time "); // In chữ "Time "

    // In Giờ (HH)
    if (now.hour() < 10) lcd.print("0");
    lcd.print(now.hour());

    lcd.print(":"); 

    // In Phút (MM)
    if (now.minute() < 10) lcd.print("0");
    lcd.print(now.minute());

    lcd.print(":"); 

    // In Giây (SS)
    if (now.second() < 10) lcd.print("0");
    lcd.print(now.second());
    lcd.print("  "); // Khoảng trắng để xóa ký tự thừa ở cuối dòng
    
    // Cập nhật giá trị giờ/phút/giây đã hiển thị
    prevDisplayedHours = now.hour();
    prevDisplayedMinutes = now.minute();
    prevDisplayedSeconds = now.second();
  }
}

void resetClockDisplayFlags() {
  prevDisplayedHours = -1;
  prevDisplayedMinutes = -1;
  prevDisplayedSeconds = -1;
  prevDisplayedDay = -1;
  prevDisplayedMonth = -1;
  prevDisplayedYear = -1;
  prevDisplayedDayOfWeek = -1;
}

/**
 * @brief Quản lý menu chính của hệ thống.
 * Cho phép người dùng cuộn qua các tùy chọn và chọn một chức năng.
 */
void handleMainMenu() {
  lcd.setCursor(0, 0);
  lcd.print("MENU CHINH       "); 
  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(mainMenuOptions[selectOptionIndex]); // In ra chuỗi tùy chọn
  // Đảm bảo xóa phần còn lại của dòng nếu tùy chọn mới ngắn hơn tùy chọn cũ
  for(int i=strlen(mainMenuOptions[selectOptionIndex]); i<14; i++) lcd.print(" ");

  // Xử lý nút UP: di chuyển lựa chọn lên trên, vòng lại khi đến đầu
  if (isButtonPressed(BTN_UP)) {
    selectOptionIndex = (selectOptionIndex - 1 + numMainMenuOptions) % numMainMenuOptions;
    Serial.print("Main Menu: Chon truoc: "); Serial.println(mainMenuOptions[selectOptionIndex]);
    lcd.clear(); 
  }
  // Xử lý nút DOWN: di chuyển lựa chọn xuống dưới, vòng lại khi đến cuối
  if (isButtonPressed(BTN_DOWN)) {
    selectOptionIndex = (selectOptionIndex + 1) % numMainMenuOptions;
    Serial.print("Main Menu: Chon sau: "); Serial.println(mainMenuOptions[selectOptionIndex]);
    lcd.clear(); 
  }

  // Xử lý nút OK: chọn chức năng
  if (isButtonPressed(BTN_OK)) {
    Serial.print("Main Menu: OK. Lua chon: ");
    Serial.println(mainMenuOptions[selectOptionIndex]);

    // Chuyển trạng thái dựa trên lựa chọn
    switch (selectOptionIndex) {
      case 0: // "1. Hen Gio" (hoặc "1. Bao Thuc" nếu menuOptions thay đổi)
        menuState = TIMER_ALARM_SELECT; // Chuyển sang menu chọn Timer/Alarm
        selectOptionIndex = 0; // Đặt lại lựa chọn cho menu con (TIMER_ALARM_SELECT)
        lcd.clear();
        // Reset các cờ liên quan đến Timer/Alarm nếu có
        // (Ví dụ nếu TIMER_ALARM_SELECT có màn hình chào mừng hoặc các cờ hiển thị riêng)
        break;
      case 1: // "2. Bam Gio"
        menuState = STOPWATCH_MENU; // Chuyển sang menu bấm giờ
        selectOptionIndex = 0; // Đặt lại lựa chọn cho menu con (STOPWATCH_MENU)
        lcd.clear();
        break;
      case 2: // "3. Chinh Thoi Gian"
        menuState = SET_TIME_DATE_MODE; // Chuyển sang menu con chọn ngày/giờ
        selectOptionIndex = 0; // Đặt lại lựa chọn cho menu con (SET_TIME_DATE_MODE)
        lcd.clear();
        break;
      case 3: // "4. Dat Lai RTC"
        menuState = RESET_RTC_CONFIRM; // Chuyển sang màn hình xác nhận đặt lại RTC
        lcd.clear();
        confirmMessageDisplayed = false;
        break;
    }
  }
}

/**
 * @brief Quản lý menu chọn giữa chỉnh Ngày/Tháng/Năm hoặc Giờ/Phút/Giây.
 */
void handleSetTimeDateMode() {
  lcd.setCursor(0, 0);
  lcd.print("Chinh:        "); // Tiêu đề, đảm bảo xóa ký tự thừa
  lcd.setCursor(0, 1);
  // Hiển thị lựa chọn hiện tại
  if (selectOptionIndex == 0) lcd.print("> Ngay/Thang/Nam ");
  else lcd.print("> Gio/Phut/Giay  ");

  // Xử lý nút UP/DOWN để chuyển giữa hai lựa chọn
  if (isButtonPressed(BTN_UP) || isButtonPressed(BTN_DOWN)) {
    selectOptionIndex = 1 - selectOptionIndex; // Đảo ngược lựa chọn (0 <-> 1)
    Serial.print("Chinh ngay/gio: Lua chon: ");
    Serial.println(selectOptionIndex == 0 ? "Ngay/Thang/Nam" : "Gio/Phut/Giay");
  }

  // Xử lý nút OK để xác nhận lựa chọn và chuyển sang chỉnh sửa chi tiết
  if (isButtonPressed(BTN_OK)) {
    setMode = (selectOptionIndex == 0) ? SET_DATE : SET_TIME; // Đặt chế độ chỉnh sửa
    subStep = (setMode == SET_DATE) ? STEP_DAY : STEP_HOUR;   // Đặt bước chỉnh sửa ban đầu

    // Lấy thời gian hiện tại từ RTC để làm giá trị ban đầu khi chỉnh sửa
    DateTime now = rtc.now();
    setDay = now.day(); setMonth = now.month(); setYear = now.year();
    setHour = now.hour(); setMinute = now.minute(); setSecond = now.second();
    
    Serial.print("Chuyen trang thai: SET_TIME_DATE_MODE -> EDIT_TIME. Che do: ");
    Serial.println(setMode == SET_DATE ? "SET_DATE" : "SET_TIME");
    Serial.print("Gia tri ban dau: ");
    Serial.print(setDay); Serial.print("/"); Serial.print(setMonth); Serial.print("/"); Serial.print(setYear); Serial.print(" ");
    Serial.print(setHour); Serial.print(":"); Serial.print(setMinute); Serial.print(":"); Serial.println(setSecond);

    menuState = EDIT_TIME; // Chuyển sang trạng thái chỉnh sửa
    lcd.clear(); // Xóa màn hình
    blinkOn = true; // Bật hiệu ứng nhấp nháy
    lastBlinkTime = millis(); // Reset bộ đếm thời gian nhấp nháy
  }
}

/**
 * @brief Quản lý quá trình chỉnh sửa chi tiết ngày, tháng, năm, giờ, phút, giây.
 * Hiển thị giá trị đang chỉnh và nhấp nháy phần đó.
 */
void handleEditTime() {
  // Cập nhật trạng thái nhấp nháy định kỳ cho các con số
  if (millis() - lastBlinkTime > blinkInterval) {
    blinkOn = !blinkOn; // Đảo ngược trạng thái nhấp nháy
    lastBlinkTime = millis(); // Reset thời gian
  }

  // --- Kiểm soát việc vẽ lại toàn bộ màn hình ---
  // Chỉ xóa và in tiêu đề khi MỚI vào chế độ chỉnh sửa hoặc khi chuyển giữa chỉnh Ngày/Giờ
  if (!editTimeScreenDisplayed) {
    lcd.clear(); // Xóa màn hình CHỈ KHI LẦN ĐẦU VÀO hoặc chuyển chế độ
    if (setMode == SET_DATE) {
      lcd.setCursor(0, 0);
      lcd.print("Chinh ngay:"); // Tiêu đề cố định trên dòng 0
    } else { // setMode == SET_TIME
      lcd.setCursor(0, 0);
      lcd.print("Chinh gio:"); // Tiêu đề cố định trên dòng 0
    }
    editTimeScreenDisplayed = true; // Đánh dấu là màn hình đã được vẽ
  }

  // --- In và cập nhật giá trị thời gian/ngày tháng ---
  if (setMode == SET_DATE) {
    // Dòng 1: DD/MM/YYYY
    // Phần Ngày (DD)
    lcd.setCursor(0, 1); // Bắt đầu từ cột 0, dòng 1
    if (subStep == STEP_DAY && blinkOn) {
      lcd.print("  "); // In khoảng trắng để làm ẩn số
    } else {
      if (setDay < 10) lcd.print("0");
      lcd.print(setDay);
    }
    lcd.print("/"); // Dấu phân cách

    // Phần Tháng (MM) - Vị trí tương đối sau DD/
    if (subStep == STEP_MONTH && blinkOn) {
      lcd.print("  ");
    } else {
      if (setMonth < 10) lcd.print("0");
      lcd.print(setMonth);
    }
    lcd.print("/"); // Dấu phân cách

    // Phần Năm (YYYY) - Vị trí tương đối sau MM/
    if (subStep == STEP_YEAR && blinkOn) {
      lcd.print("    "); // 4 khoảng trắng cho năm
    } else {
      lcd.print(setYear);
    }
    // Đảm bảo xóa sạch phần còn lại của dòng nếu chuỗi ngắn hơn (ví dụ 10/10/2025 so với 10/10/2)
    // Cần đủ khoảng trắng để ghi đè lên các ký tự cũ trên LCD 16x2
    lcd.print("    "); // Khoảng trắng để điền đầy phần còn lại của dòng
  } else { // setMode == SET_TIME - Nếu đang chỉnh giờ/phút/giây
    // Dòng 1: HH:MM:SS
    // Phần Giờ (HH)
    lcd.setCursor(0, 1); // Bắt đầu từ cột 0, dòng 1
    if (subStep == STEP_HOUR && blinkOn) {
      lcd.print("  ");
    } else {
      if (setHour < 10) lcd.print("0");
      lcd.print(setHour);
    }
    lcd.print(":"); // Dấu phân cách

    // Phần Phút (MM) - Vị trí tương đối sau HH:
    if (subStep == STEP_MINUTE && blinkOn) {
      lcd.print("  ");
    } else {
      if (setMinute < 10) lcd.print("0");
      lcd.print(setMinute);
    }
    lcd.print(":"); // Dấu phân cách

    // Phần Giây (SS) - Vị trí tương đối sau MM:
    if (subStep == STEP_SECOND && blinkOn) {
      lcd.print("  ");
    } else {
      if (setSecond < 10) lcd.print("0");
      lcd.print(setSecond);
    }
    // Đảm bảo xóa sạch phần còn lại của dòng
    lcd.print("      "); // Khoảng trắng để điền đầy phần còn lại của dòng
  }

  // --- Xử lý nút UP/DOWN để điều chỉnh giá trị ---
  if (isButtonPressed(BTN_UP)) {
    Serial.println("Nut UP duoc nhan trong che do chinh.");
    adjustTime(+1); // Tăng giá trị
    blinkOn = true; // Đảm bảo phần tử hiển thị ngay sau khi chỉnh
    lastBlinkTime = millis(); // Reset thời gian nhấp nháy
  }
  if (isButtonPressed(BTN_DOWN)) {
    Serial.println("Nut DOWN duoc nhan trong che do chinh.");
    adjustTime(-1); // Giảm giá trị
    blinkOn = true; // Đảm bảo phần tử hiển thị ngay sau khi chỉnh
    lastBlinkTime = millis(); // Reset thời gian nhấp nháy
  }

  // --- Xử lý nút OK để chuyển bước chỉnh sửa hoặc lưu ---
  if (isButtonPressed(BTN_OK)) {
    Serial.println("Nut OK duoc nhan trong che do chinh.");
    nextStep(); // Chuyển sang bước tiếp theo
    blinkOn = true; // Đảm bảo phần tử mới hiển thị ngay sau khi chuyển bước
    lastBlinkTime = millis(); // Reset thời gian nhấp nháy
    // Khi chuyển bước hoặc thoát khỏi chế độ chỉnh sửa, cần reset cờ để lần sau vào lại sẽ vẽ lại
    editTimeScreenDisplayed = false;
  }
}

/**
 * @brief Điều chỉnh giá trị của phần thời gian/ngày tháng đang được chọn.
 * @param dir Hướng điều chỉnh (+1 để tăng, -1 để giảm).
 */
void adjustTime(int dir) {
  if (setMode == SET_DATE) { // Đang chỉnh Ngày/Tháng/Năm
    if (subStep == STEP_DAY) {
      setDay += dir;
      int maxDays = daysInMonth(setMonth, setYear);
      if (setDay > maxDays) setDay = 1; // Vòng lại khi vượt quá maxDays
      if (setDay < 1) setDay = maxDays; // Vòng lại khi nhỏ hơn 1
      Serial.print("Chinh ngay: "); Serial.println(setDay);
    }
    if (subStep == STEP_MONTH) {
      setMonth += dir;
      if (setMonth > 12) setMonth = 1; // Vòng lại khi vượt quá 12
      if (setMonth < 1) setMonth = 12; // Vòng lại khi nhỏ hơn 1
      // Đảm bảo ngày không vượt quá số ngày của tháng mới (ví dụ: chuyển từ 31/3 sang tháng 2 -> ngày thành 28/29)
      setDay = constrain(setDay, 1, daysInMonth(setMonth, setYear));
      Serial.print("Chinh thang: "); Serial.print(setMonth); Serial.print(", Ngay cap nhat: "); Serial.println(setDay);
    }
    if (subStep == STEP_YEAR) {
      setYear = constrain(setYear + dir, 2000, 2099); // Giới hạn năm từ 2000 đến 2099
      // Đảm bảo ngày không vượt quá số ngày của tháng 2 trong năm nhuận/không nhuận mới
      setDay = constrain(setDay, 1, daysInMonth(setMonth, setYear));
      Serial.print("Chinh nam: "); Serial.print(setYear); Serial.print(", Ngay cap nhat: "); Serial.println(setDay);
    }
  } else { // setMode == SET_TIME - Đang chỉnh Giờ/Phút/Giây
    if (subStep == STEP_HOUR) {
      setHour = (setHour + dir + 24) % 24; // Vòng từ 0 đến 23
      Serial.print("Chinh gio: "); Serial.println(setHour);
    }
    if (subStep == STEP_MINUTE) {
      setMinute = (setMinute + dir + 60) % 60; // Vòng từ 0 đến 59
      Serial.print("Chinh phut: "); Serial.println(setMinute);
    }
    if (subStep == STEP_SECOND) {
      setSecond = (setSecond + dir + 60) % 60; // Vòng từ 0 đến 59
      Serial.print("Chinh giay: "); Serial.println(setSecond);
    }
  }
}

/**
 * @brief Chuyển sang bước chỉnh sửa tiếp theo hoặc lưu thời gian và quay về NORMAL.
 */
void nextStep() {
  if (setMode == SET_DATE) { // Đang chỉnh Ngày/Tháng/Năm
    if (subStep == STEP_DAY) {
      subStep = STEP_MONTH; // Chuyển từ Ngày sang Tháng
      Serial.println("Chuyen buoc: STEP_DAY -> STEP_MONTH");
    } else if (subStep == STEP_MONTH) {
      subStep = STEP_YEAR; // Chuyển từ Tháng sang Năm
      Serial.println("Chuyen buoc: STEP_MONTH -> STEP_YEAR");
    } else { // Đã chỉnh xong năm (STEP_YEAR), hoàn tất chỉnh Date
      // Lưu thời gian mới vào RTC
      rtc.adjust(DateTime(setYear, setMonth, setDay, setHour, setMinute, setSecond));
      Serial.print("Luu RTC (Date): ");
      Serial.print(setDay); Serial.print("/"); Serial.print(setMonth); Serial.print("/"); Serial.print(setYear); Serial.print(" ");
      Serial.print(setHour); Serial.print(":"); Serial.print(setMinute); Serial.print(":"); Serial.println(setSecond);
      menuState = NORMAL; // Quay về trạng thái hiển thị giờ bình thường
      resetClockDisplayFlags();
      lcd.clear(); // Xóa màn hình
      Serial.println("Chuyen trang thai: EDIT_TIME -> NORMAL");
      blinkOn = false; // Tắt nhấp nháy
    }
  } else { // setMode == SET_TIME - Đang chỉnh Giờ/Phút/Giây
    if (subStep == STEP_HOUR) {
      subStep = STEP_MINUTE; // Chuyển từ Giờ sang Phút
      Serial.println("Chuyen buoc: STEP_HOUR -> STEP_MINUTE");
    } else if (subStep == STEP_MINUTE) {
      subStep = STEP_SECOND; // Chuyển từ Phút sang Giây
      Serial.println("Chuyen buoc: STEP_MINUTE -> STEP_SECOND");
    } else { // Đã chỉnh xong giây (STEP_SECOND), hoàn tất chỉnh Time
      // Lưu thời gian mới vào RTC
      rtc.adjust(DateTime(setYear, setMonth, setDay, setHour, setMinute, setSecond));
      Serial.print("Luu RTC (Time): ");
      Serial.print(setDay); Serial.print("/"); Serial.print(setMonth); Serial.print("/"); Serial.print(setYear); Serial.print(" ");
      Serial.print(setHour); Serial.print(":"); Serial.print(setMinute); Serial.print(":"); Serial.println(setSecond);
      menuState = NORMAL; // Quay về trạng thái hiển thị giờ bình thường
      resetClockDisplayFlags();
      lcd.clear(); // Xóa màn hình
      Serial.println("Chuyen trang thai: EDIT_TIME -> NORMAL");
      blinkOn = false; // Tắt nhấp nháy
    }
  }
}

/**
 * @brief Quản lý menu chính của chức năng Hẹn giờ báo thức.
 */
void handleAlarmMenu() {
  lcd.setCursor(0, 0);
  lcd.print("HEN GIO BAO THUC");
  
  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(alarmMenuOptions[selectOptionIndex]);
  for(int i=strlen(alarmMenuOptions[selectOptionIndex]); i<14; i++) lcd.print(" ");

  if (isButtonPressed(BTN_UP)) {
    selectOptionIndex = (selectOptionIndex - 1 + numAlarmMenuOptions) % numAlarmMenuOptions;
    lcd.clear();
  }
  if (isButtonPressed(BTN_DOWN)) {
    selectOptionIndex = (selectOptionIndex + 1) % numAlarmMenuOptions;
    lcd.clear();
  }

  if (isButtonPressed(BTN_OK)) {
    switch (selectOptionIndex) {
      case 0: // "1. Cai Bao Thuc"
        menuState = ALARM_SET_TIME;
        subStep = STEP_HOUR; // Bắt đầu chỉnh giờ
        blinkOn = true;
        lastBlinkTime = millis();
        lcd.clear();
        break;
      case 1: // "2. Bat/Tat Bao Thuc"
        menuState = ALARM_TOGGLE;
        lcd.clear();
        break;
    }
  }
}

/**
 * @brief Cho phép người dùng cài đặt giờ và phút cho báo thức.
 */
void handleAlarmSetTime() {
  // Cập nhật trạng thái nhấp nháy định kỳ
  if (millis() - lastBlinkTime > blinkInterval) {
    blinkOn = !blinkOn;
    lastBlinkTime = millis();
  }

  lcd.setCursor(0, 0);
  lcd.print("Set Bao Thuc:   ");

  // Hiển thị giờ
  lcd.setCursor(0, 1);
  if (subStep == STEP_HOUR && blinkOn) {
    lcd.print("  ");
  } else {
    if (alarmHour < 10) lcd.print("0");
    lcd.print(alarmHour);
  }
  lcd.print(":"); // Dấu phân cách không nhấp nháy

  // Hiển thị phút
  if (subStep == STEP_MINUTE && blinkOn) {
    lcd.print("  ");
  } else {
    if (alarmMinute < 10) lcd.print("0");
    lcd.print(alarmMinute);
  }
  lcd.print("                "); // Xóa phần còn lại của dòng

  // Xử lý nút UP/DOWN để điều chỉnh giá trị
  if (isButtonPressed(BTN_UP)) {
    if (subStep == STEP_HOUR) {
      alarmHour = (alarmHour + 1) % 24;
    } else if (subStep == STEP_MINUTE) {
      alarmMinute = (alarmMinute + 1) % 60;
    }
    blinkOn = true;
    lastBlinkTime = millis();
  }
  if (isButtonPressed(BTN_DOWN)) {
    if (subStep == STEP_HOUR) {
      alarmHour = (alarmHour - 1 + 24) % 24;
    } else if (subStep == STEP_MINUTE) {
      alarmMinute = (alarmMinute - 1 + 60) % 60;
    }
    blinkOn = true;
    lastBlinkTime = millis();
  }
  
  // Xử lý nút OK để chuyển bước hoặc hoàn thành
  if (isButtonPressed(BTN_OK)) {
    if (subStep == STEP_HOUR) {
      subStep = STEP_MINUTE; // Chuyển sang chỉnh phút
    } else { // Đã chỉnh xong phút
      Serial.print("Bao Thuc cai dat: ");
      Serial.print(alarmHour); Serial.print(":"); Serial.println(alarmMinute);
      menuState = ALARM_MENU; // Quay về menu báo thức chính
      lcd.clear();
      blinkOn = false;
    }
  }
}

/**
 * @brief Cho phép người dùng bật/tắt báo thức.
 */
void handleAlarmToggle() {
  lcd.setCursor(0, 0);
  lcd.print("Bao Thuc:       ");
  lcd.setCursor(0, 1);
  if (alarmEnabled) {
    lcd.print("> ON (OK de Tat)");
  } else {
    lcd.print("> OFF (OK de Bat)");
  }
  lcd.print(" "); // Xóa ký tự thừa

  if (isButtonPressed(BTN_OK)) {
    alarmEnabled = !alarmEnabled; // Đảo ngược trạng thái
    Serial.print("Bao Thuc da "); Serial.println(alarmEnabled ? "BAT" : "TAT");
    // Không cần lcd.clear() ở đây, chỉ cần cập nhật trạng thái hiển thị
    // để tránh nháy màn hình quá nhanh. Màn hình sẽ tự cập nhật trong vòng lặp kế tiếp.
  }

  // Nhấn MENU để quay lại
  if (isButtonPressed(BTN_MENU)) {
    menuState = ALARM_MENU;
    lcd.clear();
  }
}

/**
 * @brief Kiểm tra nếu báo thức đã đến giờ và kích hoạt.
 */
void checkAlarm() {
  DateTime now = rtc.now(); // Lấy thời gian hiện tại

  // Logic kích hoạt báo thức lần đầu
  // Chỉ kích hoạt nếu báo thức được bật, chưa kích hoạt và đến đúng giờ, phút, giây
  if (alarmEnabled && !alarmTriggered) {
    if (now.hour() == alarmHour && now.minute() == alarmMinute && now.second() == 0) {
      alarmTriggered = true;        // Đặt cờ báo thức đã kích hoạt
      alarmCurrentlySounding = true; // Đặt cờ báo thức đang kêu
      alarmSoundingStartTime = millis(); // Ghi lại thời điểm bắt đầu kêu
      
      Serial.println("BAO THUC KICH HOAT!");
      
      // QUAN TRỌNG: Chuyển trạng thái menu sang ALARM_SOUNDING
      menuState = ALARM_SOUNDING;
      
      // Reset cờ hiển thị để đảm bảo thông báo được in ra khi chuyển trạng thái
      alarmMessageDisplayed = false; 
    }
  }
}
//---------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Quản lý menu cho phép người dùng chọn giữa Hẹn giờ báo thức và Hẹn giờ đếm ngược.
 * Khi người dùng chọn "2. Hẹn giờ" từ MAIN_MENU, hệ thống sẽ chuyển đến menu này.
 */
void handleTimerAlarmSelect() {
  lcd.setCursor(0, 0);
  lcd.print("CHON HEN GIO:   ");

  // 2. Hiển thị tùy chọn đang được chọn
  lcd.setCursor(0, 1);
  lcd.print("> "); // Dấu chỉ thị lựa chọn
  lcd.print(timerAlarmSelectOptions[selectOptionIndex]); // In ra tên tùy chọn
  // Xóa bất kỳ ký tự thừa nào từ tùy chọn trước đó nếu tùy chọn hiện tại ngắn hơn
  // Điều này quan trọng để tránh "ký tự rác" trên màn hình.
  for(int i = strlen(timerAlarmSelectOptions[selectOptionIndex]); i < 14; i++) {
    lcd.print(" ");
  }

  // 3. Xử lý nút UP: Di chuyển lựa chọn lên trên
  if (isButtonPressed(BTN_UP)) {
    selectOptionIndex = (selectOptionIndex - 1 + numTimerAlarmSelectOptions) % numTimerAlarmSelectOptions;
    // lcd.clear(); // BỎ DÒNG NÀY ĐI ĐỂ TRÁNH NHẤP NHÁY
    Serial.print("Timer Alarm Select: Chon truoc: ");
    Serial.println(timerAlarmSelectOptions[selectOptionIndex]); // Debug qua Serial
  }

  // 4. Xử lý nút DOWN: Di chuyển lựa chọn xuống dưới
  // Sử dụng else if để đảm bảo chỉ một nút được xử lý tại một thời điểm
  else if (isButtonPressed(BTN_DOWN)) { 
    selectOptionIndex = (selectOptionIndex + 1) % numTimerAlarmSelectOptions;
    // lcd.clear(); // BỎ DÒNG NÀY ĐI ĐỂ TRÁNH NHẤP NHÁY
    Serial.print("Timer Alarm Select: Chon sau: ");
    Serial.println(timerAlarmSelectOptions[selectOptionIndex]); // Debug qua Serial
  }

  // 5. Xử lý nút OK: Xác nhận lựa chọn và chuyển trạng thái
  if (isButtonPressed(BTN_OK)) {
    Serial.print("Timer Alarm Select: OK. Lua chon: ");
    Serial.println(timerAlarmSelectOptions[selectOptionIndex]); // Debug qua Serial

    switch (selectOptionIndex) {
      case 0: // Tùy chọn "1. Bao Thuc"
        menuState = ALARM_MENU; // Chuyển đến menu chính của chức năng báo thức
        break; // Không cần break ở đây, vì dòng sau là lcd.clear()
      case 1: // Tùy chọn "2. Dem Nguoc"
        menuState = TIMER_MENU; // Chuyển đến menu chính của chức năng đếm ngược
        break;
    }
    selectOptionIndex = 0; // Đặt lại lựa chọn cho menu con mới
    lcd.clear(); // Xóa màn hình CHỈ KHI CHUYỂN TRẠNG THÁI MỚI
    Serial.println("Chuyen trang thai: TIMER_ALARM_SELECT -> " + String(menuState == ALARM_MENU ? "ALARM_MENU" : "TIMER_MENU"));
  }
}

/**
 * @brief Quản lý menu chính của chức năng Hẹn giờ đếm ngược.
 */
void handleTimerMenu() {
  lcd.setCursor(0, 0);
  lcd.print("HEN GIO DEM NGUOC");
  
  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(timerMenuOptions[selectOptionIndex]);
  for(int i=strlen(timerMenuOptions[selectOptionIndex]); i<14; i++) lcd.print(" ");

  if (isButtonPressed(BTN_UP)) {
    selectOptionIndex = (selectOptionIndex - 1 + numTimerMenuOptions) % numTimerMenuOptions;
    lcd.clear();
  }
  if (isButtonPressed(BTN_DOWN)) {
    selectOptionIndex = (selectOptionIndex + 1) % numTimerMenuOptions;
    lcd.clear();
  }

  if (isButtonPressed(BTN_OK)) {
    switch (selectOptionIndex) {
      case 0: // "1. Cai Dem Nguoc"
        menuState = TIMER_SET_DURATION;
        // Khởi tạo giá trị cài đặt mặc định hoặc giữ giá trị cũ
        // Ví dụ: Đặt giờ, phút, giây về 0 để cài đặt mới
        setHour = 0; setMinute = 0; setSecond = 0;
        subStep = STEP_HOUR;
        blinkOn = true;
        lastBlinkTime = millis();
        lcd.clear();
        break;
      case 1: // "2. Bat Dau/Dung"
        if (timerDuration > 0) { // Chỉ bắt đầu/dừng nếu đã cài đặt thời gian
          timerRunning = !timerRunning; // Đảo ngược trạng thái
          if (timerRunning) { // Nếu bắt đầu chạy
            timerStartTime = millis() - (timerDuration - timerRemainingTime); // Tiếp tục từ thời gian còn lại
            if (timerRemainingTime == 0) { // Nếu là lần đầu chạy hoặc reset
                timerStartTime = millis(); // Bắt đầu đếm từ đầu
            }
          } else { // Nếu dừng
            timerRemainingTime = timerDuration - (millis() - timerStartTime); // Lưu thời gian còn lại
          }
          Serial.print("Timer "); Serial.println(timerRunning ? "BAT DAU" : "DUNG");
          menuState = TIMER_RUNNING; // Chuyển sang màn hình hiển thị đếm ngược
          lcd.clear();
        } else {
          Serial.println("Timer: Chua cai dat thoi gian.");
          lcd.setCursor(0, 0); lcd.print("Chua cai dat!");
          lcd.setCursor(0, 1); lcd.print("Nhan OK de ve");
          // Giữ ở đây một lúc hoặc chờ nút OK để quay lại menu timer
          if(isButtonPressed(BTN_OK)) {
            lcd.clear(); // Xóa thông báo lỗi
          }
        }
        break;
      case 2: // "3. Dat Lai"
        timerRunning = false;
        timerDuration = 0;
        timerStartTime = 0;
        timerRemainingTime = 0;
        Serial.println("Timer: DAT LAI");
        lcd.clear();
        // Có thể quay lại TIMER_SET_DURATION hoặc TIMER_MENU
        menuState = TIMER_MENU; 
        break;
    }
  }
}

/**
 * @brief Cho phép người dùng cài đặt thời gian đếm ngược (giờ, phút, giây).
 */
void handleTimerSetDuration() {
  // Cập nhật trạng thái nhấp nháy định kỳ
  if (millis() - lastBlinkTime > blinkInterval) {
    blinkOn = !blinkOn;
    lastBlinkTime = millis();
    // Mỗi khi trạng thái blink thay đổi, cần đảm bảo màn hình được cập nhật
    // Không cần lcd.clear() toàn bộ, chỉ cần in lại phần số đang nhấp nháy
  }

  lcd.setCursor(0, 0);
  lcd.print("Set Dem Nguoc:  "); // Tiêu đề menu

  // Di chuyển con trỏ và in giờ, phút, giây
  lcd.setCursor(0, 1);

  // Hiển thị GIỜ (nhấp nháy nếu đang chỉnh)
  if (subStep == STEP_HOUR && blinkOn) {
    lcd.print("  "); // In 2 khoảng trắng để tạo hiệu ứng nhấp nháy
  } else {
    if (setHour < 10) lcd.print("0");
    lcd.print(setHour);
  }
  lcd.print(":"); // Ký tự phân cách

  // Hiển thị PHÚT (nhấp nháy nếu đang chỉnh)
  if (subStep == STEP_MINUTE && blinkOn) {
    lcd.print("  ");
  } else {
    if (setMinute < 10) lcd.print("0");
    lcd.print(setMinute);
  }
  lcd.print(":"); // Ký tự phân cách

  // Hiển thị GIÂY (nhấp nháy nếu đang chỉnh)
  if (subStep == STEP_SECOND && blinkOn) {
    lcd.print("  ");
  } else {
    if (setSecond < 10) lcd.print("0");
    lcd.print(setSecond);
  }
  lcd.print(" "); // Khoảng trắng cuối để xóa ký tự thừa nếu có

  // Xử lý nút UP/DOWN để điều chỉnh giá trị
  if (isButtonPressed(BTN_UP)) {
    if (subStep == STEP_HOUR) {
      setHour = (setHour + 1) % 24; // Giới hạn 24 giờ
    } else if (subStep == STEP_MINUTE) {
      setMinute = (setMinute + 1) % 60;
    } else if (subStep == STEP_SECOND) {
      setSecond = (setSecond + 1) % 60;
    }
    blinkOn = true; // Đảm bảo nhấp nháy khi thay đổi giá trị
    lastBlinkTime = millis();
  }
  
  // Dùng else if để tránh xử lý 2 nút cùng lúc
  else if (isButtonPressed(BTN_DOWN)) { 
    if (subStep == STEP_HOUR) {
      setHour = (setHour - 1 + 24) % 24; // Đảm bảo giá trị không âm
    } else if (subStep == STEP_MINUTE) {
      setMinute = (setMinute - 1 + 60) % 60;
    } else if (subStep == STEP_SECOND) {
      setSecond = (setSecond - 1 + 60) % 60;
    }
    blinkOn = true; // Đảm bảo nhấp nháy khi thay đổi giá trị
    lastBlinkTime = millis();
  }

  // Xử lý nút OK để chuyển bước hoặc hoàn thành cài đặt
  if (isButtonPressed(BTN_OK)) {
    if (subStep == STEP_HOUR) {
      subStep = STEP_MINUTE;
    } else if (subStep == STEP_MINUTE) {
      subStep = STEP_SECOND;
    } else { // Đã chỉnh xong giây, hoàn thành cài đặt timer
      timerDuration = (long)setHour * 3600000 + (long)setMinute * 60000 + (long)setSecond * 1000;
      timerRemainingTime = timerDuration; // Khởi tạo thời gian còn lại (cho phép tạm dừng)
      
      Serial.print("Timer da cai dat: "); 
      Serial.print(setHour); Serial.print(":"); 
      Serial.print(setMinute); Serial.print(":"); 
      Serial.println(setSecond);

      // Reset các biến liên quan đến bước cài đặt và nhấp nháy
      subStep = STEP_HOUR; // Quay về bước giờ cho lần cài đặt sau
      blinkOn = false;     // Tắt nhấp nháy
      lcd.clear();         // Xóa màn hình trước khi chuyển trạng thái

      // Sau khi cài đặt xong, chuyển về menu chính của Timer để người dùng có thể "Bắt đầu"
      menuState = TIMER_MENU; 
    }
    // Sau mỗi lần nhấn OK (trừ khi hoàn thành), cũng reset trạng thái nhấp nháy
    blinkOn = true;
    lastBlinkTime = millis();
  }
}

/**
 * @brief Hiển thị thời gian đếm ngược và xử lý khi đếm ngược kết thúc.
 */
void handleTimerRunning() {
  unsigned long currentTime = millis();
  unsigned long elapsed = 0;
  unsigned long remaining = 0;

  static bool timerRunningMessageDisplayed = false;
  static bool hetGioMessageDisplayed = false;
  static bool timerPausedMessageDisplayed = false;

  // --- GIAI ĐOẠN 1: TIMER ĐANG CHẠY BÌNH THƯỜNG ---
  if (timerRunning) {
    // Reset cờ của các trạng thái khác khi CHUYỂN VÀO trạng thái này
    // Điều này đảm bảo khi chuyển từ tạm dừng hoặc hết giờ về chạy,
    // thông báo "Dem Nguoc Con:" sẽ được in lại.
    hetGioMessageDisplayed = false;
    timerPausedMessageDisplayed = false;

    elapsed = currentTime - timerStartTime;

    // Chỉ in "Dem Nguoc Con:" một lần khi MỚI VÀO trạng thái chạy
    if (!timerRunningMessageDisplayed) {
      lcd.clear(); // Xóa toàn bộ màn hình để đảm bảo sạch
      lcd.setCursor(0, 0);
      lcd.print("Dem Nguoc Con:  "); // Đảm bảo đủ khoảng trắng để xóa ký tự cũ
      timerRunningMessageDisplayed = true;
      // Reset prevDisplayedTime để đảm bảo thời gian hiển thị đúng từ đầu
      prevDisplayedHours = -1; prevDisplayedMinutes = -1; prevDisplayedSeconds = -1;
    }

    if (elapsed >= timerDuration) { // Đếm ngược kết thúc
      remaining = 0;
      timerRunning = false;
      timerAlarmSounding = true;    // ĐẶT CỜ KHI HẾT GIỜ
      timerAlarmStartTime = millis(); // Ghi lại thời điểm bắt đầu kêu
      Serial.println("TIMER HET GIO!");

      // Reset cờ hiển thị của trạng thái chạy khi chuyển sang trạng thái báo động
      timerRunningMessageDisplayed = false;
    } else { // Timer vẫn đang chạy và chưa hết giờ
      remaining = timerDuration - elapsed;

      // Tính toán giờ, phút, giây từ thời gian còn lại (miligiây)
      long hours = remaining / 3600000;
      long minutes = (remaining % 3600000) / 60000;
      long seconds = (remaining % 60000) / 1000;

      // Chỉ cập nhật màn hình nếu thời gian thay đổi để tránh nhấp nháy
      if (hours != prevDisplayedHours || minutes != prevDisplayedMinutes || seconds != prevDisplayedSeconds) {
        lcd.setCursor(0, 1); // Đặt con trỏ để in thời gian
        if (hours < 10) lcd.print("0");
        lcd.print(hours);
        lcd.print(":");
        if (minutes < 10) lcd.print("0");
        lcd.print(minutes);
        lcd.print(":");
        if (seconds < 10) lcd.print("0");
        lcd.print(seconds);
        lcd.print("   "); // Khoảng trắng để xóa ký tự thừa (rất quan trọng!)

        // Cập nhật giá trị đã hiển thị
        prevDisplayedHours = hours;
        prevDisplayedMinutes = minutes;
        prevDisplayedSeconds = seconds;
      }
    }
  }

  // --- GIAI ĐOẠN 2: TIMER ĐÃ HẾT GIỜ VÀ ĐANG BÁO ĐỘNG ---
  else if (timerAlarmSounding) {
    // Reset cờ của các trạng thái khác khi CHUYỂN VÀO trạng thái này
    timerRunningMessageDisplayed = false;
    timerPausedMessageDisplayed = false;

    // Chỉ in thông báo "HET GIO!" một lần khi MỚI VÀO trạng thái này
    if (!hetGioMessageDisplayed) {
      lcd.clear(); // Xóa màn hình cũ
      lcd.setCursor(0, 0);
      lcd.print("HET GIO!        "); // Hiển thị thông báo hết giờ (đảm bảo xóa hết dòng)
      lcd.setCursor(0, 1);
      lcd.print("Nhan OK de tat");
      hetGioMessageDisplayed = true; // Đánh dấu là đã in

      // Quan trọng: Reset các biến của playTimerToneNonBlocking() khi MỚI vào trạng thái này
      // Để nó bắt đầu lại chuỗi "tút tút tút" từ đầu.
      currentBeepCount = 0;
      nextBeepTime = 0;
      isTonePlaying = false;
      inTimerRestPeriod = false; // Đảm bảo không ở trạng thái nghỉ
    }

    // --- GỌI HÀM PHÁT ÂM THANH KHÔNG CHẶN CHO HẸN GIỜ ---
    playTimerToneNonBlocking();

    // Kiểm tra thời gian kêu tổng cộng (tự động tắt sau ALARM_TONE_DURATION_MS)
    if (currentTime - timerAlarmStartTime >= ALARM_TONE_DURATION_MS) {
      Serial.println("Da het thoi gian keu timer. Tu dong tat bao dong.");
      noTone(BUZZER_PIN);
      timerAlarmSounding = false;
      timerDuration = 0;
      timerRemainingTime = 0;
      lcd.clear();
      menuState = NORMAL; // Quay về màn hình đồng hồ bình thường
      resetClockDisplayFlags();

      // Quan trọng: Reset TẤT CẢ các cờ và biến liên quan khi thoát khỏi báo động
      hetGioMessageDisplayed = false;
      timerRunningMessageDisplayed = false;
      timerPausedMessageDisplayed = false;
      currentBeepCount = 0;
      nextBeepTime = 0;
      isTonePlaying = false;
      inTimerRestPeriod = false;
      prevDisplayedHours = -1; prevDisplayedMinutes = -1; prevDisplayedSeconds = -1; // Reset để đảm bảo hiển thị đúng khi về NORMAL
    }
  }

  // --- GIAI ĐOẠN 3: TIMER ĐANG TẠM DỪNG HOẶC CHƯA CHẠY ---
  // Điều kiện: timerRunning là false VÀ timerAlarmSounding là false
  else {
    // Reset cờ của các trạng thái khác khi CHUYỂN VÀO trạng thái này
    timerRunningMessageDisplayed = false;
    hetGioMessageDisplayed = false;

    // Chỉ in "Dem Nguoc Con:" một lần khi MỚI VÀO trạng thái tạm dừng/chưa chạy
    if (!timerPausedMessageDisplayed) {
      lcd.clear(); // Xóa toàn bộ màn hình để đảm bảo sạch
      lcd.setCursor(0, 0);
      lcd.print("Dem Nguoc Con:  "); // Đảm bảo đủ khoảng trắng để xóa ký tự cũ
      timerPausedMessageDisplayed = true;
      // Reset prevDisplayedTime để đảm bảo hiển thị đúng từ đầu
      prevDisplayedHours = -1; prevDisplayedMinutes = -1; prevDisplayedSeconds = -1;
    }

    // Tính toán và hiển thị thời gian tạm dừng (HH:MM:SS)
    remaining = (timerRemainingTime > 0) ? timerRemainingTime : timerDuration;

    long hours = remaining / 3600000;
    long minutes = (remaining % 3600000) / 60000;
    long seconds = (remaining % 60000) / 1000;

    // Chỉ cập nhật màn hình nếu thời gian thay đổi
    if (hours != prevDisplayedHours || minutes != prevDisplayedMinutes || seconds != prevDisplayedSeconds) {
      lcd.setCursor(0, 1);
      if (hours < 10) lcd.print("0");
      lcd.print(hours);
      lcd.print(":");
      if (minutes < 10) lcd.print("0");
      lcd.print(minutes);
      lcd.print(":");
      if (seconds < 10) lcd.print("0");
      lcd.print(seconds);
      lcd.print("   ");

      prevDisplayedHours = hours;
      prevDisplayedMinutes = minutes;
      prevDisplayedSeconds = seconds;
    }
  }

  // --- XỬ LÝ NÚT OK (Được kiểm tra liên tục, không bị chặn) ---
  if (isButtonPressed(BTN_OK)) {
    if (timerAlarmSounding) { // Nếu đang báo động hết giờ
      Serial.println("Tat thong bao HET GIO");
      noTone(BUZZER_PIN);
      timerAlarmSounding = false;
      timerDuration = 0;
      timerRemainingTime = 0;
      menuState = TIMER_MENU; // Quay lại menu timer
      lcd.clear();

      // QUAN TRỌNG: RESET TẤT CẢ các cờ và biến liên quan khi thoát khỏi báo động
      hetGioMessageDisplayed = false;
      timerRunningMessageDisplayed = false;
      timerPausedMessageDisplayed = false;
      currentBeepCount = 0;
      nextBeepTime = 0;
      isTonePlaying = false;
      inTimerRestPeriod = false; // Đảm bảo cờ nghỉ cũng được reset
      prevDisplayedHours = -1; prevDisplayedMinutes = -1; prevDisplayedSeconds = -1; // Reset để đảm bảo hiển thị đúng khi quay lại menu
    } else { // Nếu không phải trạng thái báo động hết giờ, chỉ đơn thuần thoát về menu
      Serial.println("Thoat Timer Display ve Timer Menu");
      menuState = TIMER_MENU; // Quay lại menu timer
      lcd.clear();
      noTone(BUZZER_PIN);

      // QUAN TRỌNG: Reset TẤT CẢ các cờ và biến liên quan khi thoát khỏi màn hình hẹn giờ
      timerRunningMessageDisplayed = false;
      hetGioMessageDisplayed = false;
      timerPausedMessageDisplayed = false;
      prevDisplayedHours = -1; prevDisplayedMinutes = -1; prevDisplayedSeconds = -1;
    }
  }
}

/**
 * @brief Hiển thị thời gian bấm giờ trên LCD.
 * Cập nhật liên tục thời gian đang chạy hoặc thời gian dừng.
 */
void displayStopwatchTime() {
  unsigned long currentMillis = millis();
  unsigned long displayTime = 0;

  if (stopwatchRunning) {
    displayTime = currentMillis - startTime; // Thời gian hiện tại khi đang chạy
  } else {
    displayTime = pausedTime; // Thời gian khi bấm giờ đang dừng
  }

  // Chuyển đổi mili giây sang giờ, phút, giây, mili giây
  long hours = displayTime / 3600000;
  displayTime %= 3600000;
  long minutes = displayTime / 60000;
  displayTime %= 60000;
  long seconds = displayTime / 1000;
  long milliseconds = displayTime % 1000;

  // Hiển thị thời gian bấm giờ chính trên dòng 0 (HH:MM:SS.mmm)
  lcd.setCursor(0, 0);
  if (hours < 10) lcd.print("0");
  lcd.print(hours);
  lcd.print(":");
  if (minutes < 10) lcd.print("0");
  lcd.print(minutes);
  lcd.print(":");
  if (seconds < 10) lcd.print("0");
  lcd.print(seconds);
  lcd.print(".");
  // Định dạng mili giây với 3 chữ số
  if (milliseconds < 100) lcd.print("0");
  if (milliseconds < 10) lcd.print("0");
  lcd.print(milliseconds);
  lcd.print(" "); // Đảm bảo xóa ký tự thừa ở cuối dòng 0

  // Hiển thị trạng thái (RUNNING/PAUSED) hoặc Lap Time trên dòng 1
  lcd.setCursor(0, 1);
  if (lapTime > 0) { // Nếu có Lap Time, ưu tiên hiển thị Lap Time
    lcd.print("Lap: ");
    long lapSec = lapTime / 1000;
    long lapMs = lapTime % 1000;
    if (lapSec < 10) lcd.print("0");
    lcd.print(lapSec);
    lcd.print(".");
    if (lapMs < 100) lcd.print("0");
    if (lapMs < 10) lcd.print("0");
    lcd.print(lapMs);
    lcd.print("    "); // Đảm bảo xóa ký tự thừa
  } else { // Nếu không có Lap Time, hiển thị trạng thái
    if (stopwatchRunning) {
      lcd.print("RUNNING         "); // Xóa ký tự thừa
    } else {
      lcd.print("PAUSED          ");
    }
  }

  // Nút OK sẽ quay lại menu bấm giờ (Start/Stop/Reset/Lap)
  if (isButtonPressed(BTN_OK)) {
    Serial.println("Thoat Stopwatch Display ve Stopwatch Menu");
    menuState = STOPWATCH_MENU; // Quay lại menu quản lý bấm giờ
    lcd.clear();
    selectOptionIndex = 0; // Đặt lại lựa chọn menu bấm giờ về đầu
    lapTime = 0; // Reset lapTime khi thoát khỏi màn hình hiển thị để tránh hiển thị lại lap cũ
  }
}

/**
 * @brief Quản lý menu bấm giờ.
 * Cho phép người dùng chọn Start/Stop/Reset/Lap cho bấm giờ.
 */
void handleStopwatchMenu() {
  lcd.setCursor(0, 0);
  lcd.print("BAM GIO:        "); // Tiêu đề menu bấm giờ
  
  // Hiển thị tùy chọn hiện tại (Start/Stop/Reset/Lap)
  lcd.setCursor(0, 1);
  lcd.print("> ");
  lcd.print(stopwatchMenuOptions[selectOptionIndex]);
  // Xóa phần còn lại của dòng nếu tùy chọn ngắn hơn
  for(int i=strlen(stopwatchMenuOptions[selectOptionIndex]); i<14; i++) lcd.print(" ");

  // Xử lý nút UP/DOWN để di chuyển lựa chọn
  if (isButtonPressed(BTN_UP)) {
    selectOptionIndex = (selectOptionIndex - 1 + numStopwatchMenuOptions) % numStopwatchMenuOptions;
    Serial.print("Stopwatch Menu: Chon truoc: "); Serial.println(stopwatchMenuOptions[selectOptionIndex]);
    lcd.clear();
  }
  if (isButtonPressed(BTN_DOWN)) {
    selectOptionIndex = (selectOptionIndex + 1) % numStopwatchMenuOptions;
    Serial.print("Stopwatch Menu: Chon sau: "); Serial.println(stopwatchMenuOptions[selectOptionIndex]);
    lcd.clear();
  }

  // Xử lý nút OK để thực hiện hành động
  if (isButtonPressed(BTN_OK)) {
    Serial.print("Stopwatch Menu: OK. Lua chon: ");
    Serial.println(stopwatchMenuOptions[selectOptionIndex]);
    
    switch (selectOptionIndex) {
      case 0: // "Start"
        if (!stopwatchRunning) { // Chỉ Start nếu chưa chạy
          Serial.println("BAM GIO: Start");
          startTime = millis() - pausedTime; // Tính lại startTime để tiếp tục từ pausedTime
          stopwatchRunning = true;
          lcd.clear();
          menuState = STOPWATCH_DISPLAY; // Chuyển sang màn hình hiển thị bấm giờ
        } else {
          Serial.println("BAM GIO: Da dang chay.");
        }
        break;
      case 1: // "Stop"
        if (stopwatchRunning) { // Chỉ Stop nếu đang chạy
          Serial.println("BAM GIO: Stop");
          pausedTime = millis() - startTime; // Lưu lại thời gian đã chạy
          stopwatchRunning = false;
          lcd.clear();
          menuState = STOPWATCH_DISPLAY; // Chuyển sang màn hình hiển thị bấm giờ (dừng)
        } else {
          Serial.println("BAM GIO: Da dung hoac chua chay.");
        }
        break;
      case 2: // "Reset"
        Serial.println("BAM GIO: Reset");
        stopwatchRunning = false;
        startTime = 0;
        pausedTime = 0;
        lapTime = 0; // Reset cả lap time
        lcd.clear();
        menuState = STOPWATCH_DISPLAY; // Chuyển sang màn hình hiển thị bấm giờ (reset về 0)
        break;
      case 3: // "Lap"
        if (stopwatchRunning) {
          lapTime = millis() - startTime; // Ghi thời gian vòng hiện tại
          Serial.print("BAM GIO: Lap Time: "); Serial.println(lapTime);
          displayStopwatchTime();
        } else {
          Serial.println("BAM GIO: Khong the Lap khi chua chay.");
        }
        // Sau khi Lap, vẫn ở menu bấm giờ để có thể Lap tiếp hoặc Stop
        break;
    }
  }
}

/**
 * @brief Màn hình xác nhận đặt lại thời gian RTC về thời gian biên dịch.
 */
void handleResetRTCConfirm() {
  // Chỉ in thông báo xác nhận nếu chưa được in
  if (!confirmMessageDisplayed) {
    lcd.clear(); // Xóa màn hình trước đó
    lcd.setCursor(0, 0);
    lcd.print("DAT LAI RTC?    "); // Hỏi người dùng có muốn đặt lại không
    lcd.setCursor(0, 1);
    lcd.print("OK: CO / MENU: KHONG"); // Hướng dẫn sử dụng nút
    confirmMessageDisplayed = true; // Đánh dấu là đã in
  }

  // --- Xử lý nút ---

  // Nếu người dùng nhấn OK, thực hiện đặt lại RTC
  if (isButtonPressed(BTN_OK)) {
    Serial.println("Xac nhan dat lai RTC!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Đặt lại RTC về thời gian biên dịch
    
    lcd.clear(); // Xóa màn hình xác nhận
    lcd.setCursor(0, 0);
    lcd.print("Da dat lai RTC!"); // Thông báo đã đặt lại
    lcd.setCursor(0, 1);
    lcd.print("                "); // Xóa dòng 2 nếu có nội dung cũ
    delay(1500); // Giữ thông báo hiển thị trong 1.5 giây

    menuState = NORMAL; // Quay về màn hình hiển thị giờ bình thường
    resetClockDisplayFlags(); // Reset cờ để đồng hồ cập nhật ngay
    lcd.clear(); // Xóa màn hình "Da dat lai RTC!"
    confirmMessageDisplayed = false; // Reset cờ cho lần dùng sau
  }
  // Nếu người dùng nhấn MENU, hủy bỏ và quay lại menu chính
  else if (isButtonPressed(BTN_MENU)) {
    Serial.println("Huy dat lai RTC.");
    menuState = MAIN_MENU; // Quay lại menu chính
    lcd.clear(); // Xóa màn hình xác nhận
    confirmMessageDisplayed = false; // Reset cờ cho lần dùng sau
  }
}
//-----------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Quản lý trạng thái báo thức đang kêu.
 */
void handleAlarmSounding() {
  unsigned long currentTime = millis();

  // Biến cờ để đảm bảo thông báo chỉ in một lần
  static bool alarmMessageDisplayed = false; // Đảm bảo biến này được khai báo static hoặc toàn cục

  // Chỉ in thông báo "BAO THUC!" một lần khi mới vào trạng thái này
  if (!alarmMessageDisplayed) {
    lcd.clear(); // Xóa màn hình cũ để đảm bảo không còn nội dung trước đó
    lcd.setCursor(0, 0);
    lcd.print("BAO THUC!");
    lcd.setCursor(0, 1);
    lcd.print("Nhan OK de tat");
    alarmMessageDisplayed = true; // Đánh dấu là đã in
  }

  // --- Logic phát âm báo (KHÔNG CHẶN) ---
  // Vẫn trong thời gian kêu 30s, tiếp tục phát tone
  if (currentTime - alarmSoundingStartTime < ALARM_SOUND_DURATION_MS) {
    playAlarmToneNonBlocking(); 
  } else {
    // Đã kêu đủ 30 giây, tự động tắt báo thức
    Serial.println("Bao thuc da keu du 30s. Tu dong tat.");
    noTone(BUZZER_PIN);             // Tắt còi
    alarmCurrentlySounding = false; // Đặt cờ là không còn kêu nữa
    alarmTriggered = false;         // Tắt cờ báo thức (để không kêu lại cho đến ngày mai)
    lcd.clear(); 
    menuState = NORMAL;             // QUAY VỀ MÀN HÌNH ĐỒNG HỒ BÌNH THƯỜNG
    resetClockDisplayFlags();
    alarmMessageDisplayed = false;  // Reset cờ cho lần sau
    // Đảm bảo reset cả biến của playAlarmToneNonBlocking() khi thoát khỏi trạng thái này
    currentAlarmNoteIndex = 0;
    nextAlarmNoteTime = 0;
  }

  // --- Xử lý nút OK để tắt báo thức thủ công ---
  if (isButtonPressed(BTN_OK)) {
    Serial.println("BAO THUC DA TAT BANG NUT OK!");
    noTone(BUZZER_PIN);             // Đảm bảo tắt còi ngay lập tức
    alarmCurrentlySounding = false; // Tắt cờ báo động
    alarmTriggered = false;         // Tắt báo thức
    lcd.clear();
    menuState = NORMAL;             // QUAY VỀ MÀN HÌNH ĐỒNG HỒ BÌNH THƯỜNG
    resetClockDisplayFlags();
    alarmMessageDisplayed = false;  // Reset cờ cho lần sau
    // Đảm bảo reset cả biến của playAlarmToneNonBlocking() khi thoát khỏi trạng thái này
    currentAlarmNoteIndex = 0;
    nextAlarmNoteTime = 0;
  }
}

/**
 * @brief Phát giai điệu báo thức một cách không chặn.
 * Cần được gọi liên tục trong loop() hoặc handleAlarmSounding().
 */
void playAlarmToneNonBlocking() {
  // Nếu báo thức không còn kêu hoặc chưa đến thời điểm phát nốt tiếp theo, thoát
  if (!alarmCurrentlySounding || millis() < nextAlarmNoteTime) {
    return;
  }

  // Nếu đến đây, nghĩa là đã đến lúc phát nốt tiếp theo
  int noteFreq = alarmMelody[currentAlarmNoteIndex];
  int noteDurationMs = 1000 / alarmNoteDurations[currentAlarmNoteIndex];

  tone(BUZZER_PIN, noteFreq, noteDurationMs); // Phát nốt với duration
  nextAlarmNoteTime = millis() + noteDurationMs + (noteDurationMs / 10); // Nghỉ 10% thời gian nốt
  currentAlarmNoteIndex++; // Chuyển sang nốt tiếp theo

  // Nếu đã phát hết tất cả các nốt trong giai điệu, quay lại nốt đầu tiên để lặp lại
  if (currentAlarmNoteIndex >= NUM_ALARM_NOTES) {
    currentAlarmNoteIndex = 0; // Quay lại nốt đầu tiên
    nextAlarmNoteTime = millis() + 1000; // Nghỉ 1 giây trước khi bắt đầu lại giai điệu
  }
}

/**
 * @brief Phát âm báo "tút tút tút" cho hẹn giờ một cách không chặn.
 * Gồm 3 tiếng 'tút', sau đó có khoảng nghỉ.
 * Cần được gọi liên tục trong hàm xử lý trạng thái hẹn giờ hết.
 */
void playTimerToneNonBlocking() {
  unsigned long currentTime = millis();

  // --- GIAI ĐOẠN NGHỈ GIỮA CÁC CHUỖI TÚT TÚT TÚT ---
  if (inTimerRestPeriod) {
    if (currentTime >= nextBeepTime) {
      // Đã hết thời gian nghỉ, bắt đầu lại chuỗi "tút tút tút"
      inTimerRestPeriod = false;
      currentBeepCount = 0;       // Reset để bắt đầu lại từ tiếng 'tút' đầu tiên
      // nextBeepTime sẽ được tính lại khi tiếng 'tút' đầu tiên kêu
      isTonePlaying = false;      // Đảm bảo còi không kêu
      noTone(BUZZER_PIN);
    } else {
      // Vẫn đang trong thời gian nghỉ, không làm gì cả
      return; 
    }
  }

  // --- GIAI ĐOẠN PHÁT 3 TIẾNG TÚT TÚT TÚT ---
  // Điều kiện: Chưa phát đủ 3 tiếng VÀ đã đến thời điểm của tiếng 'tút' tiếp theo
  if (currentBeepCount < 3 && currentTime >= nextBeepTime) {
    if (!isTonePlaying) { // Nếu chưa có tiếng nào đang kêu (bắt đầu một tiếng mới)
      tone(BUZZER_PIN, 800);      // Tần số 800 Hz
      nextBeepTime = currentTime + 150; // Sẽ tắt sau 150ms
      isTonePlaying = true;
    } else { // Nếu tiếng 'tút' đang kêu (150ms đã trôi qua), thì tắt và nghỉ
      noTone(BUZZER_PIN);
      nextBeepTime = currentTime + 100; // Nghỉ 100ms giữa các tiếng 'tút'
      isTonePlaying = false;
      currentBeepCount++; // Tăng số tiếng 'tút' đã phát
    }
  } 
  // --- KẾT THÚC CHUỖI 3 TIẾNG TÚT VÀ BẮT ĐẦU GIAI ĐOẠN NGHỈ ---
  else if (currentBeepCount >= 3) {
    noTone(BUZZER_PIN); // Đảm bảo còi tắt sau tiếng 'tút' cuối cùng
    nextBeepTime = currentTime + TIMER_SEQUENCE_REST_INTERVAL_MS; // Bắt đầu khoảng nghỉ 2 giây
    inTimerRestPeriod = true;     // Đặt cờ đang trong thời gian nghỉ
    // Không reset currentBeepCount ở đây. Nó sẽ được reset khi hết thời gian nghỉ
    // hoặc khi báo động hẹn giờ bị tắt hoàn toàn.
  }
}
