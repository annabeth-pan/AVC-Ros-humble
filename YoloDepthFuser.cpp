#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>
#include <iostream>

#include "rclcpp/rclcpp.hpp"
#include "vision_msgs/msg/detection3d_array.hpp"
#include "vision_msgs/msg/detection2d_array.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "stereo_msgs/msg/disparity_image.hpp"

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
    cv::Mat cv_disparityimg[3];
    float f;
    float T;
    
    class Detections{
      public:
        int bbox_centerx;
        int bbox_centery;
        int bbox_sizex;
        int bbox_sizey;
        rclcpp::Time detection_time;
        rclcpp::Time disparity_time;
        cv::Mat disparities;
    };
    Detections[3] alldetections; // should we use a vector??? i feel like maybe just in case the processing for one of them takes way longer or smth idk

    // some form to store detections; must keep bbox
    
  private:
    void find_bb_depth()
    {
      Detection det = alldetections[0]
      for(int i=det.bbox_centerx-det.bbox_sizex/2; i<det.bbox_centerx+det.bbox_sizex/2; i++){
        for(int j=det.bbox_centery-det.bbox_sizey/2; j<det.bbox_centery+det.bbox_sizey/2; j++){
          
        }
      }
      // calculate median DISPARITY of section of bounding box 
      //    -> not literal median by points, but median by bundling points tgth (makes sense thru visualizing)
      // find corresponding real depth
      // do all the other stuff you wanna put in a 3d detectoin array
      // publish <- there was an example of how to do that but i accidentally deleted it lmao find it
    }
    void detection_callback(const vision_msgs::msg::Detection2DArray::SharedPtr msg) 
    {
      Detections detected; 
      detected.bbox_centerx=msg.bbox.center.x;
      detected.bbox_centery=msg.bbox.center.y;
      detected.bbox_sizex=msg.bbox.size_x;
      detected.bbox_sizey=msg.bbox.size_y;
      detected.detection_time=msg.header.stamp;
      // save the detection msg as. something.
      // check if detectinos/disparity callback has a header that matches up
      if((detected.detection_time-detected.disparitytime).seconds <= .001):
        find_bb_depth(detected)
      else:
        alldetections.push_back(detected); 

    }
    rclcpp::Subscription<vision_msgs::msg::Detection2DArray>::SharedPtr yolo_detection_subscription_;

    void disparityimg_callback(const vision_msgs::msg::Detection2DArray::SharedPtr disparity) 
    {
      cv::Mat cv_disparityimg = cv_bridge::toCvShare(disparity.image) -> cv_disparityimg
      f = 0;
      T = 0;
      // check if detectinos/disparity callback has a header that matches up
      // call the calc
      if(detected.detection_time==detected.disparitytime):
        find_bb_depth(detected)       
    }
    rclcpp::Subscription<stereo_msgs::msg::DisparityImage>::SharedPtr yolo_detection_subscription_;

    rclcpp::TimerBase::SharedPtr timer_;
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