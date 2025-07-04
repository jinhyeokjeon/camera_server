#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include "threads.h"
#include "global.h"
#include "utils.h"
#include "protocols.h"

int camsetpage() {
  while (true) {
    // 클라이언트 측으로부터 "stop" 수신 시 종료
    uint8_t type;
    if (recv(client_fd, &type, 1, MSG_DONTWAIT) == 1) {
      if (type == COMMAND) {
        uint32_t dataLen;
        char buf[64] = { 0, };
        readNBytes(client_fd, &dataLen, 4);
        readNBytes(client_fd, buf, dataLen);
        std::cout << "[Server] message from client: " << buf << std::endl;
        if (strcmp(buf, "stop") == 0) {
          return 0;
        }
        else {
          puts("Undefined situation");
          return -1;
        }
      }
      else {
        puts("Undefined situation");
        return -1;
      }
    }

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) break;

    // 얼굴 탐지 쓰레드를 위해 최신 프레임 공유
    {
      std::lock_guard<std::mutex> lock(frameMutex);
      frame.copyTo(sharedFrame);
    }

    // 얼굴 사각형과 감지 유무 받아옴
    dlib::rectangle faceRect;
    bool localHasFace;
    {
      std::lock_guard<std::mutex> lock(faceMutex);
      localHasFace = hasFace;
      faceRect = biggestFaceRect;
    }

    if (localHasFace) {
      // 얼굴 사각형 그리기
      cv::rectangle(frame,
        cv::Point(faceRect.left(), faceRect.top()),
        cv::Point(faceRect.right(), faceRect.bottom()),
        cv::Scalar(0, 255, 0), 2);
    }

    // 클라이언트에 프레임 전송하기
    std::vector<uchar> buf;
    cv::imencode(".jpg", frame, buf);
    uint32_t size = buf.size();
    uint8_t protocol = VIDEO;

    if (writeNBytes(client_fd, &protocol, 1) == -1) return -1;
    if (writeNBytes(client_fd, &size, 4) == -1) return -1;
    if (writeNBytes(client_fd, buf.data(), size) == -1) return -1;
  }

  return 0;
}