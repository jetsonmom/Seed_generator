#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SimpleDHT.h>

// LCD 설정
LiquidCrystal_I2C lcd(0x3F, 16, 2);  // LCD 주소가 0x27인 경우 수정 필요

// 릴레이 핀 설정
#define relay_led1 2    // LED 릴레이
#define relay_pump 3    // 펌프 릴레이
#define relay_fan 5     // 팬 릴레이

// DHT11 온도 센서 설정
int pinDHT11 = 6;       // 온도 센서 핀
SimpleDHT11 dht11(pinDHT11);

// 시간 변수
int currentHour = 12;   // 기본값 설정 (정오)
int currentMinute = 0;
int currentSecond = 0;
bool timeReceived = false;  // 시간을 받았는지 여부

// 온도 및 습도 변수
byte temperature = 0;
byte humidity = 0;

// 온도 및 습도 임계값 설정
const byte TEMP_THRESHOLD = 35;  // 35도 이상이면 LED 끄고 팬 켜기
const byte HUMIDITY_THRESHOLD = 90;  // 발아 단계: 90% 이상이면 팬 켜기

// 상태 변수
bool isOperatingHours = true;   // 초기값은 운영 시간으로 설정
bool isOverheated = false;      // 과열 여부
bool isHighHumidity = false;    // 고습도 여부

// 팬 동작 이유 추적
String fanReason = "";

// 디버깅용 변수
unsigned long lastSerialRequestTime = 0;
unsigned long serialRequestCount = 0;

// LCD 업데이트 시간 제어용 변수
unsigned long lastLCDUpdateTime = 0;
const unsigned long LCD_UPDATE_INTERVAL = 1000;  // 1초마다 LCD 업데이트

void setup() {
  // 릴레이 핀 초기화 - 가장 먼저 실행
  pinMode(relay_led1, OUTPUT);
  pinMode(relay_pump, OUTPUT);
  pinMode(relay_fan, OUTPUT);
  
  // 모든 릴레이 초기 상태 OFF
  digitalWrite(relay_led1, LOW);
  digitalWrite(relay_pump, LOW);
  digitalWrite(relay_fan, LOW);
  
  // 시리얼 통신 초기화
  Serial.begin(115200);
  
  // LCD 초기화
  Wire.begin();  // Arduino에서 I2C 초기화 (A4=SDA, A5=SCL)
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("Smart Farm System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  delay(2000);
  lcd.clear();

  // 제슨 나노에 시간 정보 요청
  Serial.println("REQUEST_TIME");
  lastSerialRequestTime = millis();
  serialRequestCount++;
  
  Serial.println("스마트팜 시스템이 시작되었습니다.");
}

void loop() {
  // 1. 시리얼에서 시간 데이터 읽기
  readTimeFromJetson();
  
  // 2. 온습도 센서 읽기
  readDHT11Sensor();
  
  // 3. 운영 시간 체크 (06:00~20:00)
  if (timeReceived) {
    isOperatingHours = (currentHour >= 6 && currentHour < 20);
  }
  
  // 4. 환경 조건 체크
  isOverheated = (temperature >= TEMP_THRESHOLD);
  isHighHumidity = (humidity >= HUMIDITY_THRESHOLD);
  
  // 5. 장치 제어 로직
  controlDevices();
  
  // 6. LCD 화면 주기적 업데이트
  unsigned long currentMillis = millis();
  if (currentMillis - lastLCDUpdateTime >= LCD_UPDATE_INTERVAL) {
    updateLCD();
    lastLCDUpdateTime = currentMillis;
  }
  
  // 잠시 대기
  delay(100);  // 100ms 간격으로 실행 (CPU 부하 감소)
}

void readTimeFromJetson() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    


    // "TIME:HH:MM:SS" 형식 확인
    if (command.startsWith("TIME:")) {
      String timeStr = command.substring(5);
      
      if (timeStr.length() >= 8) {
        currentHour = timeStr.substring(0, 2).toInt();
        currentMinute = timeStr.substring(3, 5).toInt();
        currentSecond = timeStr.substring(6, 8).toInt();
        timeReceived = true;

        Serial.print("시간 설정 완료: ");
        Serial.print(currentHour);
        Serial.print(":");
        Serial.print(currentMinute);
        Serial.print(":");
        Serial.println(currentSecond);
      }
    }
  }
}

void readDHT11Sensor() {
  // DHT11 센서에서 온습도 읽기
  static unsigned long lastSensorReadTime = 0;
  unsigned long currentMillis = millis();
  
  // 2초마다 센서 읽기 (DHT11 권장 간격)
  if (currentMillis - lastSensorReadTime >= 2000) {
    lastSensorReadTime = currentMillis;
    
    int err = SimpleDHTErrSuccess;
    if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
      Serial.print("DHT11 읽기 실패, 오류="); 
      Serial.print(SimpleDHTErrCode(err));
      Serial.print(", "); 
      Serial.println(SimpleDHTErrDuration(err));
      return;
    }
    
    // 디버그 출력

    
    // 경고 메시지
    if (isOverheated) {
      Serial.println("경고: 온도가 너무 높습니다!");
    }
    
    if (isHighHumidity) {
      Serial.println("경고: 습도가 너무 높습니다!");
    }
  }
}

void controlDevices() {
  // LED 제어: 운영 시간 내 & 과열 아닐 때 켜기
  if (isOperatingHours && !isOverheated) {
    digitalWrite(relay_led1, HIGH);  // LED 켜기
    Serial.println("LED: ON (운영 시간 내)");
  } else {
    digitalWrite(relay_led1, LOW);   // LED 끄기
    if (!isOperatingHours) {
      Serial.println("LED: OFF (운영 시간 외)");
    } else {
      Serial.println("LED: OFF (과열)");
    }
  }
  
  // 팬 제어: 과열 또는 고습도 시 켜기
  if (isOverheated || isHighHumidity) {
    digitalWrite(relay_fan, HIGH);   // 팬 켜기
    
    // 팬 동작 이유 설정
    if (isOverheated && isHighHumidity) {
      fanReason = "T&H";  // 온도 & 습도
    } else if (isOverheated) {
      fanReason = "Temp";  // 온도
    } else {
      fanReason = "Humid";  // 습도
    }
    
    Serial.print("팬: ON (이유: ");
    Serial.print(fanReason);
    Serial.println(")");
  } else {
    digitalWrite(relay_fan, LOW);    // 팬 끄기
    fanReason = "";
    Serial.println("팬: OFF");
  }
  
  // 여기에 펌프 제어 로직 추가 가능
  // 예: 토양 습도 센서 기반 또는 정기적 관수
}

void updateLCD() {
  // 첫 번째 줄: 온도 및 습도
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature);
  lcd.write(0xDF);
  lcd.print("C  ");
  
  lcd.print("H:");
  lcd.print(humidity);
  lcd.print("%   ");
  
  // 두 번째 줄: 현재 시간과 상태
  lcd.setCursor(0, 1);
  
  // 시간 표시 - 무조건 표시하도록 변경
  lcd.print("Time ");
  if (timeReceived) {
    // 시간이 수신된 경우
    if (currentHour < 10) lcd.print("0");
    lcd.print(currentHour);
    lcd.print(":");
    if (currentMinute < 10) lcd.print("0");
    lcd.print(currentMinute);
  } else {
    // 시간이 수신되지 않은 경우
    lcd.print("--:--");
  }

  // 상태 표시
  lcd.setCursor(11, 1);
  if (fanReason != "") {
    lcd.print("F:");
    lcd.print(fanReason);
  } else if (isOperatingHours) {
    lcd.print("LED:ON ");
  } else {
    lcd.print("LED:OFF");
  }
}
