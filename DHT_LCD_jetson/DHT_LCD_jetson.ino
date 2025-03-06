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
int currentHour = 0;
int currentMinute = 0;
int currentSecond = 0;

// 온도 및 습도 변수
byte temperature = 0;
byte humidity = 0;

// 온도 및 습도 임계값 설정
const byte TEMP_THRESHOLD = 38;  // 35도 이상이면 LED 끄고 팬 켜기
const byte HUMIDITY_THRESHOLD = 95;  // 80% 이상이면 팬 켜기

// 상태 변수
bool isOperatingHours = false;  // 운영 시간 여부 (06:00~20:00)
bool isOverheated = false;      // 과열 여부
bool isHighHumidity = false;    // 고습도 여부

// 팬 동작 이유 추적
String fanReason = "";

// 시간 요청 관련 변수
unsigned long lastRequestTime = 0;
const unsigned long REQUEST_INTERVAL = 10000000;  // 10초마다 요청

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(115200);
  
  // 릴레이 핀 초기화
  pinMode(relay_led1, OUTPUT);
  pinMode(relay_pump, OUTPUT);
  pinMode(relay_fan, OUTPUT);
  
  // 모든 릴레이 초기 상태 OFF
  digitalWrite(relay_led1, LOW);
  digitalWrite(relay_pump, LOW);
  digitalWrite(relay_fan, LOW);
  
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
  
  // 시작시 시간 요청
  Serial.println("REQUEST_TIME");
  lastRequestTime = millis();
  
  Serial.println("스마트팜 시스템이 시작되었습니다.");
}

void loop() {
  // 1. 시리얼에서 시간 데이터 읽기
  readTimeFromJetson();
  
  // 2. 정기적으로 시간 요청
  unsigned long currentMillis = millis();
  if (currentMillis - lastRequestTime >= REQUEST_INTERVAL) {
    Serial.println("REQUEST_TIME");
    lastRequestTime = currentMillis;
  }
  
  // 3. 온습도 센서 읽기
  readDHT11Sensor();
  
  // 4. 운영 시간 체크 (06:00~20:00)
  isOperatingHours = (currentHour >= 6 && currentHour < 21);
  
  // 5. 환경 조건 체크
  isOverheated = (temperature >= TEMP_THRESHOLD);
  isHighHumidity = (humidity >= HUMIDITY_THRESHOLD);
  
  // 6. 장치 제어 로직
  controlDevices();
  
  // 7. LCD 화면 업데이트
  updateLCD();
  
  // 잠시 대기
  delay(1000);
}

void readTimeFromJetson() {
  // 제슨 나노로부터 시간 데이터 읽기
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    
    if (command.startsWith("TIME:")) {
      String timeStr = command.substring(5);
      
      // 시:분:초 파싱
      if (timeStr.length() >= 8) {
        currentHour = timeStr.substring(0, 2).toInt();
        currentMinute = timeStr.substring(3, 5).toInt();
        currentSecond = timeStr.substring(6, 8).toInt();
        
        // 디버그 출력
        Serial.print("시간 수신: ");
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
  int err = SimpleDHTErrSuccess;
  
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("DHT11 읽기 실패, 오류="); 
    Serial.print(SimpleDHTErrCode(err));
    Serial.print(", "); 
    Serial.println(SimpleDHTErrDuration(err));
    return;
  }
  
  // 디버그 출력
  Serial.print("온도: ");
  Serial.print((int)temperature);
  Serial.print("°C, 습도: ");
  Serial.print((int)humidity);
  Serial.println("%");
  
  // 경고 메시지
  if (isOverheated) {
    Serial.println("경고: 온도가 너무 높습니다!");
  }
  
  if (isHighHumidity) {
    Serial.println("경고: 습도가 너무 높습니다!");
  }
}

void controlDevices() {
  // LED 제어: 운영 시간 내 & 과열 아닐 때 켜기
  if (isOperatingHours && !isOverheated) {
    digitalWrite(relay_led1, HIGH);  // LED 켜기
    Serial.println("LED: ON");
  } else {
    digitalWrite(relay_led1, LOW);   // LED 끄기
    Serial.println("LED: OFF");
  }
  
  // 팬 제어: 과열 또는 고습도 시 켜기
  if (isOverheated || isHighHumidity) {
    digitalWrite(relay_fan, HIGH);   // 팬 켜기
    
    // 팬 동작 이유 설정
    if (isOverheated && isHighHumidity) {
      fanReason = "Temp&Humid";
    } else if (isOverheated) {
      fanReason = "Temperature";
    } else {
      fanReason = "Humidity";
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
  // LCD 첫 번째 줄: 시간과 온도/습도
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature);
  lcd.write(0xDF);
  lcd.print("C ");
  
  lcd.print("H:");
  lcd.print(humidity);
  lcd.print("% ");
  
  // LCD 두 번째 줄: 시간 및 상태 정보
  lcd.setCursor(0, 1);
  
  // 시간 표시
  if (currentHour < 10) lcd.print("0");
  lcd.print(currentHour);
  lcd.print(":");
  if (currentMinute < 10) lcd.print("0");
  lcd.print(currentMinute);
  lcd.print(" ");
  
  // 상태 표시
  lcd.setCursor(6, 1);
  if (!isOperatingHours) {
    lcd.print("Night");
  } else if (isOverheated) {
    lcd.print("Hot! ");
  } else if (isHighHumidity) {
    lcd.print("Humid");
  } else {
    lcd.print("Good ");
  }
}
