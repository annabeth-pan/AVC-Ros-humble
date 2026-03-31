#include <__config>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <queue>
#include <utility>

#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>
#include "rclcpp/rclcpp.hpp"
#include "vision_msgs/msg/detection3d_array.hpp"
#include "vision_msgs/msg/detection2d_array.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "stereo_msgs/msg/disparity_image.hpp"

#define BUFFER_LEN

using namespace std::chrono_literals;

class YoloDepthFuser : public rclcpp::Node
{
  public:
    YoloDepthFuser()
    : Node("yolo_depth_fuser"), count_(0)
    {
      publisher_ = this->create_publisher<vision_msgs::msg::Detection3DArray>("fused_vision_measurements", 10);
      // timer_ = this->create_wall_timer(
      // 100ms, std::bind(&YoloDepthFuser::timer_callback, this));

      yolo_detection_subscription_ = this->create_subscription<vision_msgs::msg::Detection2DArray>(
      "PLACEHOLDER1", 10, std::bind(&YoloDepthFuser::detection_callback, this, _1)); // PLACEHOLDER TOPICS

      // stereo_pointcloud_subscription_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
      // "PLACEHOLDER2", 10, std::bind(&YoloDepthFuser::pointcloud_callback, this, _1)); // PLACEHOLDER TOPICS

      stereo_disparityimg_subscription_ = this->create_subscription<stereo_msgs::msg::DisparityImage>(
      "PLACEHOLDER2", 10, std::bind(&YoloDepthFuser::disparityimg_callback, this, _1)); // PLACEHOLDER TOPICS
    }
    
    class Disparity {
      public:
        cv::Mat disparity_image;
        rclpp::Time timestamp;
        float f;
        float T;
    };
    std::array<Disparity, BUFFER_LEN> disparities_buffer;

    class Detection {
      public:
        int bbox_centerx;
        int bbox_centery;
        int bbox_sizex;
        int bbox_sizey;
        rclcpp::Time timestamp;
    };
    //Detection[3] Detections;
    std::array<Detection, BUFFER_LEN> detections_buffer;

    // some form to store detections; must keep bbox
    
  private:
    template <typename T, std::size_t N> void add_to_buf(std::array<T, N>& a, T n)
    {
      a[N-1] = n;
      std::rotate(a.begin(), a.begin() + 1, a.end());
      a[N-1] = NULL;
    }
    
    void matchmake_headers()
    {
      float min_timediff = 999;
      Detection target_det;
      Disparity target_dis;
      for (Detection det : detections_buffer) {
        if (det != NULL) { // not 100% sure this is actually how null checking works lol?
          for (dis : disparities_buffer) {
            if (min_timediff > abs((det.timestamp - dis.timestamp).seconds())) {
              min_timediff = abs((det.timestamp - dis.timestamp).seconds());
              target_det = det;
              target_dis = dis;
            }
          }
        }
      }
      return std::make_pair(target_det, target_dis);
    }

    void find_bb_depth()
    {
      Detection det = detections_buffer.front();
      for(int i=det.bbox_centerx-det.bbox_sizex/2; i<det.bbox_centerx+det.bbox_sizex/2; i++){
        for(int j=det.bbox_centery-det.bbox_sizey/2; j<det.bbox_centery+det.bbox_sizey/2; j++){
          
        }
      }
      // calculate median DISPARITY of section of bounding box 
          // specifically the median of a little bit in the middle (hopefully around the part closes to the camera)
      // find corresponding real depth
          // (subtract the radius of the bucket to get the center)
      // do all the other stuff you wanna put in a 3d detectoin array
      // publish <- there was an example of how to do that but i accidentally deleted it lmao find it
    }
    void detection_callback(const vision_msgs::msg::Detection2DArray::SharedPtr msg) 
    {
      // save the detection msg as a detections object
      Detection detected; 
      detected.bbox_centerx = msg->detections[0].bbox.center.x;
      detected.bbox_centery = msg->detections[0].bbox.center.y;
      detected.bbox_sizex = msg->detections[0].bbox.size_x;
      detected.bbox_sizey = msg->detections[0].bbox.size_y;
      detected.detection_time=msg.header.stamp;

      add_to_buf(detections_buffer, detected);
      // check if detections/disparity callback has a header that matches up
      if((detected.detection_time-detected.disparitytime).seconds <= .01):
        find_bb_depth(detected)
      else:
    }
    rclcpp::Subscription<vision_msgs::msg::Detection2DArray>::SharedPtr yolo_detection_subscription_;

    void disparityimg_callback(const vision_msgs::msg::Detection2DArray::SharedPtr disparity) 
    {
      Disparity disp;
      disp.disparity_image = cv_bridge::toCvShare(disparity.image) -> cv_disparityimg;
      disp.f = disparity -> f;
      disp.T = disparity -> T;
      
      add_to_buf(disparities_buffer, disp);
      // check if detections/disparity callback has a header that matches up
      // call the calc
      if(detected.detection_time == detected.disparitytime):
        find_bb_depth(detected)       
    }
    rclcpp::Subscription<stereo_msgs::msg::DisparityImage>::SharedPtr stereo_disparityimg_subscription_;

    // rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    size_t count_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<YoloDepthFuser>());
  rclcpp::shutdown();
  return 0;
}