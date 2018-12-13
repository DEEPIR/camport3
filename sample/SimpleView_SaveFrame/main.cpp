#include "../common/common.hpp"
#include "../common/json.hpp"
#include <string>
#include <fstream>

static int fps_counter = 0;
static clock_t fps_tm = 0;

void save_frame(const cv::Mat& depth, const cv::Mat& leftIR, const cv::Mat& rightIR, const cv::Mat& color);
#ifdef _WIN32
static int get_fps() {
   const int kMaxCounter = 250;
   fps_counter++;
   if (fps_counter < kMaxCounter) {
     return -1;
   }
   int elapse = (clock() - fps_tm);
   int v = (int)(((float)fps_counter) / elapse * CLOCKS_PER_SEC);
   fps_tm = clock();

   fps_counter = 0;
   return v;
 }
#else
static int get_fps() {
  const int kMaxCounter = 200;
  struct timeval start;
  fps_counter++;
  if (fps_counter < kMaxCounter) {
    return -1;
  }

  gettimeofday(&start, NULL);
  int elapse = start.tv_sec * 1000 + start.tv_usec / 1000 - fps_tm;
  int v = (int)(((float)fps_counter) / elapse * 1000);
  gettimeofday(&start, NULL);
  fps_tm = start.tv_sec * 1000 + start.tv_usec / 1000;

  fps_counter = 0;
  return v;
}
#endif

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
        // Note: 
        //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to debug with breakpoint!
    }
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}
using nlohmann::json;
json load_config(const std::string& cfg_path){
	std::fstream fs(cfg_path);
	json cfg;
	fs >> cfg;
	return cfg;
}
int main(int argc, char* argv[])
{
    json cfg = load_config("./config.json");
    std::string depth_fmt = cfg.at("depth_fmt");
    std::string rgb_fmt = cfg.at("rgb_fmt");

    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int32_t color, ir, depth;
    color = ir = depth = 1;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if(strcmp(argv[i], "-color=off") == 0) {
            color = 0;
        } else if(strcmp(argv[i], "-depth=off") == 0) {
            depth = 0;
        } else if(strcmp(argv[i], "-ir=off") == 0) {
            ir = 0;
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_FetchFrame [-h] [-id <ID>] [-ip <IP>]");
            return 0;
        }
    }

    if (!color && !depth && !ir) {
        LOGD("At least one component need to be on");
        return -1;
    }
    LOGD("ID:%s IP:%s", ID.c_str(), IP.c_str());
    LOGD("Init lib");
    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK( selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) );
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
    ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );

    int32_t allComps;
    ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );
    if(allComps & TY_COMPONENT_RGB_CAM /* && color*/) {
        LOGD("Has RGB camera, open RGB cam");
        ASSERT_OK( TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM) );

        std::vector<TY_ENUM_ENTRY> image_mode_list;
        ASSERT_OK(get_feature_enum_list(hDevice, TY_COMPONENT_RGB_CAM, TY_ENUM_IMAGE_MODE, image_mode_list));
        for (int idx = 0; idx < image_mode_list.size(); idx++){
            TY_ENUM_ENTRY &entry = image_mode_list[idx];
            //try to select a vga resolution
	    auto w = TYImageWidth(entry.value);
	    auto h = TYImageHeight(entry.value);
	    LOGD("support RGB width:%d height:%d description:%s", w, h, entry.description);
	    if(std::string(entry.description) == rgb_fmt){
                LOGD("Select RGB Image Mode: %s", entry.description);
                int err = TYSetEnum(hDevice, TY_COMPONENT_RGB_CAM, TY_ENUM_IMAGE_MODE, entry.value);
		LOGD("set RGB mode error:%d", err);
                ASSERT(err == TY_STATUS_OK || err == TY_STATUS_NOT_PERMITTED);
                break;
            }
        }

    }else{
        LOGD("not open RGB cam color=%d", color);
    }
    if(allComps & TY_COMPONENT_RGB_CAM_RIGHT) {
        LOGD("has right RGB");
    }else{
        LOGD("has not right RGB");
    }
    if(allComps & TY_COMPONENT_RGB_CAM_LEFT) {
        LOGD("has left RGB");
    }else{
        LOGD("has not left RGB");
    }

    if (allComps & TY_COMPONENT_IR_CAM_LEFT && ir) {
        LOGD("Has IR left camera, open IR left cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_LEFT));
    }

    if (allComps & TY_COMPONENT_IR_CAM_RIGHT && ir) {
        LOGD("Has IR right camera, open IR right cam");
		    ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_RIGHT));
    }

    LOGD("Configure components, open depth cam");
    if (allComps & TY_COMPONENT_DEPTH_CAM && depth) {
        LOGD("Has depth camera, open depth right cam");
        std::vector<TY_ENUM_ENTRY> image_mode_list;
        ASSERT_OK(get_feature_enum_list(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode_list));
        for (int idx = 0; idx < image_mode_list.size(); idx++){
            TY_ENUM_ENTRY &entry = image_mode_list[idx];
	    LOGD("depth support Image mode:%s", entry.description);
            //try to select a vga resolution
            //if (TYImageWidth(entry.value) == 640 || TYImageHeight(entry.value) == 640){
	    if(std::string(entry.description) == depth_fmt){
                LOGD("Select Depth Image Mode: %s", entry.description);
                int err = TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, entry.value);
		LOGD("set error:%d", err);
                ASSERT(err == TY_STATUS_OK || err == TY_STATUS_NOT_PERMITTED);
                break;
            }
        }
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));
    }


    LOGD("Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);
    ASSERT( frameSize >= 640 * 480 * 2 );

    LOGD("     - Allocate & enqueue buffers");
    char* frameBuffer[2];
    frameBuffer[0] = new char[frameSize];
    frameBuffer[1] = new char[frameSize];
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

    LOGD("Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    bool hasTrigger;
    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &hasTrigger));
    if (hasTrigger) {
        LOGD("Disable trigger mode");
        TY_TRIGGER_PARAM trigger;
        trigger.mode = TY_TRIGGER_MODE_OFF;
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));
    }

    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    DepthViewer depthViewer("Depth");
    int index = 0;
    while(!exit_main) {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d", ++index);

            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }

            cv::Mat depth, irl, irr, color;
            parseFrame(frame, &depth, &irl, &irr, &color);
            if(!depth.empty()){
                LOGD("show depth width:%d height:%d channels:%d", depth.cols, depth.rows, depth.channels());
                depthViewer.show(depth);
            }
//            if(!irl.empty()){ cv::imshow("LeftIR", irl); }
//            if(!irr.empty()){ cv::imshow("RightIR", irr); }
            if(!color.empty()){ cv::imshow("Color", color); }

            int key = cv::waitKey(1);
            switch(key & 0xff) {
            case 0xff:
                break;
            case 'q':
                exit_main = true;
                break;
            case 's':
                save_frame(depth, irl, irr, color);
                break;
            default:
                LOGD("Unmapped key %d", key);
            }

            LOGD("Re-enqueue buffer(%p, %d)"
                , frame.userBuffer, frame.bufferSize);
            ASSERT_OK( TYEnqueueBuffer(hDevice, frame.userBuffer, frame.bufferSize) );
        }
    }
    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice) );
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];

    LOGD("Main done!");
    return 0;
}
void save_frame(const cv::Mat& depth, const cv::Mat& leftIR, const cv::Mat& rightIR, const cv::Mat& color){
	static int c = 0;
	c++;
	std::string depth_name = std::string("depth_") + std::to_string(c) + ".jpg";
	std::string left_name = std::string("left_") + std::to_string(c) + ".jpg";
	std::string right_name = std::string("right_") + std::to_string(c) + ".jpg";
	std::string color_name = std::string("color_") + std::to_string(c) + ".jpg";
	if (!depth.empty()){ 
		cv::imwrite(depth_name, depth);
		LOGI("depth width:%d, height:%d", depth.cols, depth.rows);
	}
	if (!color.empty()) {
		cv::imwrite(color_name, color);
		LOGI("color width:%d, height:%d", color.cols, color.rows);
	}
	LOGI("save files done counter:%d", c);
}
