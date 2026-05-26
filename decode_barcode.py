import cv2
from pyzbar.pyzbar import decode
import sys
import os

if len(sys.argv) < 2:
    print("NOT_FOUND")
    sys.exit(1)

image_path = sys.argv[1]

# Проверяем что файл существует
if not os.path.exists(image_path):
    print("NOT_FOUND")
    sys.exit(1)

# Читаем изображение
img = cv2.imread(image_path)

if img is None:
    print("NOT_FOUND")
    sys.exit(1)

# Пробуем разные методы улучшения изображения
barcodes = []

# 1. Оригинал
barcodes = decode(img)

# 2. Серый
if not barcodes:
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    barcodes = decode(gray)

# 3. Размытие
if not barcodes:
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)
    barcodes = decode(blurred)

# 4. Порог
if not barcodes:
    _, thresh = cv2.threshold(gray, 100, 255, cv2.THRESH_BINARY)
    barcodes = decode(thresh)

if barcodes:
    result = barcodes[0].data.decode('utf-8')
    print(result)
else:
    print("NOT_FOUND")
