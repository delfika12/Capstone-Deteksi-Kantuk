import torch
import time
import platform
import requests
from pathlib import Path
from models.common import DetectMultiBackend
from utils.dataloaders import LoadStreams
from utils.general import (
    check_img_size, non_max_suppression, scale_boxes, cv2
)
from utils.torch_utils import select_device, smart_inference_mode
from ultralytics.utils.plotting import Annotator, colors

ESP32_IP = 'http://192.168.137.254'  # Ganti IP ESP32 sesuai IP yang tampil di Serial Monitor

@smart_inference_mode()
def run():
    if platform.system() == "Windows":
        import pathlib
        pathlib.PosixPath = pathlib.WindowsPath

    weights = 'saved_model/best.pt'  # Path model YOLO custom Anda
    source = 'http://192.168.137.254:81/stream'  # MJPEG stream ESP32-CAM
    conf_thres = 0.25
    iou_thres = 0.45
    imgsz = (640, 640)

    device = select_device('')
    model = DetectMultiBackend(weights, device=device, data='data/coco128.yaml', fp16=False)
    stride, names = model.stride, model.names
    imgsz = check_img_size(imgsz, s=stride)

    dataset = LoadStreams(source, img_size=imgsz, stride=stride, auto=model.pt)
    model.warmup(imgsz=(1, 3, *imgsz))

    eye_closed_start = None
    last_state = None

    for path, im, im0s, _, _ in dataset:
        im = torch.from_numpy(im).to(device).float() / 255.0
        if im.ndimension() == 3:
            im = im.unsqueeze(0)

        pred = model(im)
        pred = non_max_suppression(pred, conf_thres, iou_thres)

        for i, det in enumerate(pred):
            im0 = im0s[i].copy()
            annotator = Annotator(im0, line_width=2, example=str(names))

            current_time = time.time()
            eyeclose_detected = False

            if det is not None and len(det):
                det[:, :4] = scale_boxes(im.shape[2:], det[:, :4], im0.shape).round()
                for *xyxy, conf, cls in reversed(det):
                    class_name = names[int(cls)]
                    label = f'{class_name} {conf:.2f}'
                    annotator.box_label(xyxy, label, color=colors(int(cls), True))

                    if class_name.lower() == 'eyeclose':
                        eyeclose_detected = True

            if eyeclose_detected:
                if eye_closed_start is None:
                    eye_closed_start = current_time
                eye_closed_duration = current_time - eye_closed_start
                
            else:
                eye_closed_duration = 0
                eye_closed_start = None
                

            duration_text = f"Eye closed: {eye_closed_duration:.2f} sec"
            cv2.putText(im0, duration_text, (10, im0.shape[0] - 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

            # Kirim status jika drowsy > 2 detik
            if eye_closed_duration > 2:
                cv2.putText(im0, "DROWSINESS DETECTED!", (10, im0.shape[0] - 50),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 3)
                requests.get(f"{ESP32_IP}/status?status=drowsy", timeout=2)
                print(f"[INFO] Status dikirim: drowsy")

            else:
                requests.get(f"{ESP32_IP}/status?status=neutral", timeout=2)
                print(f"[INFO] Status dikirim: neutral")


            cv2.imshow(str(path[i]), annotator.result())
            if cv2.waitKey(1) == ord('q'):
                break

if __name__ == "__main__":
    run()
