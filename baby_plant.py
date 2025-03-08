#!/usr/bin/env python3
import serial
import time
from datetime import datetime
import logging
import sys
import os

# 로깅 설정
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('time_sender.log'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

# 시리얼 포트 설정 (USB0이 아니면 변경 필요)
SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200

def setup_serial():
    """시리얼 연결 설정"""
    while True:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            logger.info(f"시리얼 포트 {SERIAL_PORT} 연결 성공")
            time.sleep(2)  # 아두이노 리셋 시간 대기
            return ser
        except serial.SerialException as e:
            logger.error(f"시리얼 포트 연결 실패: {e}")
            logger.info("시리얼 포트 목록 확인:")
            os.system("ls -l /dev/ttyUSB*")
            logger.info("10초 후 재시도...")
            time.sleep(10)

def main():
    """메인 프로그램"""
    logger.info("제슨 나노 시간 전송 프로그램 시작")
    
    try:
        ser = setup_serial()
        request_count = 0
        response_count = 0
        last_time_send = 0
        
        while True:
            current_time = time.time()
            
            # 아두이노로부터 메시지 읽기
            if ser.in_waiting > 0:
                try:
                    message = ser.readline().decode('utf-8', errors='replace').strip()
                    if message:
                        logger.info(f"아두이노로부터 수신: {message}")
                        
                        # 시간 요청에 응답
                        if message == "REQUEST_TIME":
                            request_count += 1
                            # 현재 시간 가져오기
                            current_time_str = datetime.now().strftime("%H:%M:%S")
                            time_message = f"TIME:{current_time_str}\n"
                            ser.write(time_message.encode())
                            
                            response_count += 1
                            last_time_send = current_time
                            logger.info(f"시간 정보 전송: {current_time_str} (요청 {request_count}개, 응답 {response_count}개)")
                except Exception as e:
                    logger.error(f"메시지 읽기 오류: {e}")
            
            # 30초 이상 시간을 보내지 않았으면 자동으로 전송
            if current_time - last_time_send >= 30:
                current_time_str = datetime.now().strftime("%H:%M:%S")
                time_message = f"TIME:{current_time_str}\n"
                ser.write(time_message.encode())
                last_time_send = current_time
                logger.info(f"자동 시간 정보 전송: {current_time_str}")
            
            # 잠시 대기
            time.sleep(0.1)
            
    except KeyboardInterrupt:
        logger.info("프로그램 종료 (Ctrl+C)")
    except Exception as e:
        logger.error(f"예상치 못한 오류: {e}")
        raise
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            logger.info("시리얼 포트 닫힘")

# 무한 재시도 루프 추가
if __name__ == "__main__":
    while True:
        try:
            main()
        except Exception as e:
            logger.error(f"심각한 오류 발생, 5초 후 재시작: {e}")
            time.sleep(5)
