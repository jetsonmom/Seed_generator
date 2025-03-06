#!/usr/bin/env python3
#  발아기 사진 보내는 루틴
import time
import os
import smtplib
import subprocess
import sys
import select
import termios
import tty
from datetime import datetime
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.image import MIMEImage
import logging

# 로깅 설정
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('daily_camera.log'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

# 이메일 설정
EMAIL_CONFIG = {
    'smtp_server': 'smtp.gmail.com',
    'smtp_port': 587,
    'username':,  # 앱 비밀번호
    'receiver': '@hanmail.net'  # 받는 이메일 주소
}

# 카메라 설정
SCHEDULED_HOUR = 9  # 아침 9시
RESOLUTION = 3  # 3: 1280x720, 4: 1920x1080

class KeyPoller:
    """키보드 입력을 비차단 방식으로 감지하는 클래스"""
    def __init__(self):
        self.old_settings = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
    
    def poll(self):
        """키 입력이 있으면 해당 키를, 없으면 None을 반환"""
        if select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
            return sys.stdin.read(1)
        return None
    
    def cleanup(self):
        """원래 터미널 설정으로 복원"""
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)

class DailyMorningPhotoSystem:
    def __init__(self):
        logger.info("일일 아침 사진 시스템 초기화 완료")
        logger.info("프로그램 종료: q 키를 누르세요")
        self.today_sent = False  # 오늘 이미 전송했는지 여부
        self.key_poller = KeyPoller()
    
    def capture_photo(self):
        """GStreamer를 사용한 사진 촬영"""
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"smartfarm_{timestamp}.jpg"
            
            # nvgstcapture-1.0 명령어로 고품질 사진 촬영
            cmd = f"nvgstcapture-1.0 --mode=1 --capture-auto --image-res={RESOLUTION} --file-name={filename}"
            
            logger.info(f"명령어 실행: {cmd}")
            result = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            
            if result.returncode != 0:
                logger.error(f"사진 촬영 실패: {result.stderr.decode()}")
                return None
            
            # nvgstcapture-1.0은 파일 이름에 자동으로 .jpg를 추가할 수 있음
            actual_filename = filename
            if not os.path.exists(actual_filename) and os.path.exists(f"{filename}.jpg"):
                actual_filename = f"{filename}.jpg"
            
            logger.info(f"사진 촬영 및 저장 완료: {actual_filename}")
            return actual_filename
        except Exception as e:
            logger.error(f"사진 촬영 중 오류: {e}")
            return None
    
    def send_email(self, photo_path):
        """이메일 전송"""
        try:
            # 현재 시간
            current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            
            # 이메일 구성
            msg = MIMEMultipart()
            msg['Subject'] = f'스마트팜 일일 보고 - {current_time}'
            msg['From'] = EMAIL_CONFIG['username']
            msg['To'] = EMAIL_CONFIG['receiver']
            
            # 텍스트 내용
            text = MIMEText(f"스마트팜의 오늘 아침 상태입니다.\n\n촬영 시간: {current_time}")
            msg.attach(text)
            
            # 이미지 첨부
            with open(photo_path, 'rb') as f:
                img_data = f.read()
                
            image = MIMEImage(img_data, name=os.path.basename(photo_path))
            msg.attach(image)
            
            # 이메일 전송
            with smtplib.SMTP(EMAIL_CONFIG['smtp_server'], EMAIL_CONFIG['smtp_port']) as server:
                server.starttls()
                server.login(EMAIL_CONFIG['username'], EMAIL_CONFIG['password'])
                server.send_message(msg)
                
            logger.info(f"이메일 전송 완료: {EMAIL_CONFIG['receiver']}")
            return True
        except Exception as e:
            logger.error(f"이메일 전송 실패: {e}")
            return False
    
    def cleanup_photo(self, photo_path):
        """임시 사진 파일 삭제"""
        try:
            if os.path.exists(photo_path):
                os.remove(photo_path)
                logger.info(f"임시 파일 삭제 완료: {photo_path}")
        except Exception as e:
            logger.error(f"파일 삭제 중 오류: {e}")
    
    def run(self):
        """메인 실행 루프"""
        logger.info("일일 아침 사진 시스템 시작 (예약 시간: 매일 오전 9시)")
        logger.info("종료하려면 'q' 키를 누르세요")
        
        try:
            while True:
                # 'q' 키 입력 확인
                key = self.key_poller.poll()
                if key == 'q':
                    logger.info("q 키 입력 감지: 프로그램 종료")
                    break
                
                now = datetime.now()
                
                # 현재 날짜의 오전 9시인지 확인
                is_target_time = (now.hour == SCHEDULED_HOUR and now.minute == 0)
                
                # 날짜가 바뀌면 today_sent 플래그 초기화
                if now.hour == 0 and now.minute == 0:
                    self.today_sent = False
                    logger.info("새로운 날 시작: 상태 초기화")
                
                # 오전 9시이고 오늘 아직 전송하지 않았으면 사진 촬영 및 전송
                if is_target_time and not self.today_sent:
                    logger.info("예약된 시간에 도달: 사진 촬영 시작")
                    
                    # 사진 촬영
                    photo_path = self.capture_photo()
                    
                    if photo_path:
                        # 이메일 전송
                        if self.send_email(photo_path):
                            # 전송 성공시 파일 삭제
                            self.cleanup_photo(photo_path)
                            # 오늘 전송 완료 표시
                            self.today_sent = True
                            logger.info("오늘의 사진 전송 완료")
                
                # 1초 대기
                time.sleep(1)
                
        except KeyboardInterrupt:
            logger.info("프로그램 종료 (Ctrl+C)")
        finally:
            # 터미널 설정 복원
            self.key_poller.cleanup()
            logger.info("프로그램이 정상적으로 종료되었습니다.")

if __name__ == "__main__":
    try:
        system = DailyMorningPhotoSystem()
        system.run()
    except Exception as e:
        logger.error(f"심각한 오류 발생: {e}")
        sys.exit(1)
