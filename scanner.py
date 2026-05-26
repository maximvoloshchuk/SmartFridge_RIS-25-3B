import cv2
from pyzbar.pyzbar import decode
import requests
import time

# Адрес Qt-приложения
QT_SERVER = "http://localhost:8765"

def scan_barcode():
    cap = cv2.VideoCapture(0)

    print("📷 Сканер запущен! Нажмите Q для выхода.")
    print("Наведите штрих-код на камеру...")

    last_barcode = ""
    last_time = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # Декодируем штрих-коды
        barcodes = decode(frame)

        for barcode in barcodes:
            barcode_data = barcode.data.decode('utf-8')

            # Рисуем рамку
            pts = barcode.polygon
            if len(pts) == 4:
                import numpy as np
                pts = np.array(pts, np.int32)
                cv2.polylines(frame, [pts], True, (0, 255, 0), 2)

            # Отправляем если новый (не чаще раза в 2 секунды)
            if barcode_data != last_barcode or time.time() - last_time > 2:
                print(f"✅ Найден: {barcode_data}")

                try:
                    response = requests.post(
                        f"{QT_SERVER}/barcode",
                        json={"barcode": barcode_data},
                        timeout=1
                    )
                    print(f"   Ответ Qt: {response.text}")
                except:
                    print("   ⚠️ Qt не отвечает")

                last_barcode = barcode_data
                last_time = time.time()

        cv2.imshow('📷 Сканер штрих-кодов (Q - выход)', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()
    print("Сканер остановлен.")

if __name__ == "__main__":
    scan_barcode()
