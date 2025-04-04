#ifndef MEETING_SDK_LINUX_SAMPLE_ZOOMSDKRENDERERDELEGATE_H
#define MEETING_SDK_LINUX_SAMPLE_ZOOMSDKRENDERERDELEGATE_H

#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <future>

#include <X11/Xlib.h>

// Temporarily comment out OpenCV for debugging
/*
#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
*/

#include "zoom_sdk_raw_data_def.h"
#include "rawdata/rawdata_renderer_interface.h"

#include "../util/SocketServer.h"
#include "../util/Log.h"

// Temporarily comment out OpenCV namespace
// using namespace cv;
using namespace std;
using namespace ZOOMSDK;

// Simple rectangle class to replace OpenCV's Rect
class Rect {
public:
    int x, y, width, height;
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {}
};

class ZoomSDKRendererDelegate : public IZoomSDKRendererDelegate {
    const string c_window = "Face_Detection";
    string m_dir = "out";
    string m_filename = "meeting-video.yuv";

    unsigned int m_frameCount = 0;
    double m_scale=3;
    double m_fx = 1/m_scale;

    vector<Rect> m_faces;
    // Temporarily comment out OpenCV classes
    // CascadeClassifier m_cascade;

    SocketServer m_socketServer;

public:
    ZoomSDKRendererDelegate();

    void writeToFile(const string& path, YUVRawDataI420* data);

    void setDir(const string& dir);
    void setFilename(const string& filename);

    void onRawDataFrameReceived(YUVRawDataI420* data) override;
    void onRawDataStatusChanged(RawDataStatus status) override {};
    void onRendererBeDestroyed() override {};
};


#endif //MEETING_SDK_LINUX_SAMPLE_ZOOMSDKRENDERERDELEGATE_H
