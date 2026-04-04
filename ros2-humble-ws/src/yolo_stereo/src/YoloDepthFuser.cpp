#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <optional>


#include <image_transport/image_transport.hpp> // Using image_transport allows us to publish and subscribe to compressed image streams in ROS2
#include <opencv2/opencv.hpp> // We include everything about OpenCV as we don't care much about compilation time at the moment.
// #include <opencv2/core.hpp>
// #include <opencv2/imgproc.hpp>
#include <cv_bridge/cv_bridge.h>
#include "rclcpp/rclcpp.hpp"
#include "vision_msgs/msg/detection3_d_array.hpp"
#include "vision_msgs/msg/detection2_d_array.hpp"
#include "stereo_msgs/msg/disparity_image.hpp"

#define BUFFER_LEN 5 // the actual buffer is BUFFER_LEN-1 because of how the buffer works lol
#define MAX_TIME_DIFF .01 // in seconds
#define CROP_RATIO .1 // CROP_RATIO*2 = the percent of the bucket per dimension that is included in the crop
// the crop is median'd to find an approximation for the closest point to the camera and from there the center
#define BUCKET_RADIUS .5 // in m

using namespace std::chrono_literals;

class YoloDepthFuser : public rclcpp::Node
{
  public:
    YoloDepthFuser()
    : Node("yolo_depth_fuser"), count_(0)
    {
      using std::placeholders::_1;

      publisher_ = this->create_publisher<vision_msgs::msg::Detection3DArray>("fused_vision_measurements", 10);

      yolo_detection_subscription_ = this->create_subscription<vision_msgs::msg::Detection2DArray>(
      "PLACEHOLDER1", 10, std::bind(&YoloDepthFuser::detection_callback, this, _1)); // PLACEHOLDER TOPICS

      stereo_disparityimg_subscription_ = this->create_subscription<stereo_msgs::msg::DisparityImage>(
      "PLACEHOLDER2", 10, std::bind(&YoloDepthFuser::disparityimg_callback, this, _1)); // PLACEHOLDER TOPICS
    }
    
    class Disparity {
      public:
        cv::Mat disparity_image;
        rclcpp::Time timestamp; // fix: was rclpp::Time
        float f;
        float T;
    };
    std::array<std::optional<Disparity>, BUFFER_LEN> disparities_buffer;

    class Detection {
      public:
        int bbox_centerx; // relative to top left corner; in pixels
        int bbox_centery;
        int bbox_sizex;
        int bbox_sizey;
        float confidence;
        int color; // 0 is red, 1 is yellow, 2 is blue
    };

    class Detections_Set {
      public:
        std::vector<Detection> detections;
        rclcpp::Time timestamp; 
    };

    std::array<std::optional<Detections_Set>, BUFFER_LEN> detections_buffer;
    
  private:
    template <typename T, std::size_t N> void add_to_buf(std::array<std::optional<T>, N>& a, T n)
    {
      a[N-1] = n;
      std::rotate(a.begin(), a.begin() + 1, a.end());
      a[N-1] = std::nullopt;
    }
    
    void try_match_times()
    {
      float min_timediff = 999; // in seconds
      Detections_Set target_det_set;
      Disparity target_dis;
      for (const auto& det_set : detections_buffer) { 
        if (!det_set.has_value()) {continue;}
        for (const auto& dis : disparities_buffer) { 
          if (!dis.has_value()) {continue;}
          if (min_timediff > abs((det_set->timestamp - dis->timestamp).seconds())) {
            min_timediff = abs((det_set->timestamp - dis->timestamp).seconds());
            target_det_set = *det_set;
            target_dis = *dis;
          }
        }
      }
      if ((target_det_set.timestamp - target_dis.timestamp).seconds() < MAX_TIME_DIFF) 
      {
        find_and_publish_3d_dets(target_det_set, target_dis); 
      }
    }

    // Source - https://stackoverflow.com/q/30078756
    // Posted by CV_User, modified by community. See post 'Timeline' for change history
    // Retrieved 2026-04-02, License - CC BY-SA 4.0
    double medianMat(cv::Mat Input){    
      Input = Input.reshape(0,1); // spread Input Mat to single row
      std::vector<double> vecFromMat;
      Input.copyTo(vecFromMat); // Copy Input Mat to vector vecFromMat
      std::nth_element(vecFromMat.begin(), vecFromMat.begin() + vecFromMat.size() / 2, vecFromMat.end());
      return vecFromMat[vecFromMat.size() / 2];
    }

    void find_and_publish_3d_dets(Detections_Set det_set, Disparity disp)
    {
      vision_msgs::msg::Detection3DArray final_detections_arr;
      for (Detection det : det_set.detections) { 
        vision_msgs::msg::Detection3D detection3D;
        // calculate median disparity of section of bounding box 
          // specifically the median of a little bit in the middle (hopefully around the part closes to the camera)
        cv::Mat cropped = disp.disparity_image(
                               cv::Range(det.bbox_centery - det.bbox_sizey*CROP_RATIO, det.bbox_centery + det.bbox_sizey*CROP_RATIO),
                               cv::Range(det.bbox_centerx - det.bbox_sizex*CROP_RATIO, det.bbox_centerx + det.bbox_sizex*CROP_RATIO));
        // find corresponding real depth (subtract the radius of the bucket to get the center)
        float relx = disp.f*disp.T/medianMat(cropped) - BUCKET_RADIUS; // called relx for consistency with buckalization
        
        // calculated through FANCY MATH with our specific camera VL-FPD3-8CAM-RPI22
        // https://docs.opencv.org/2.4/modules/calib3d/doc/camera_calibration_and_3d_reconstruction.html
        float rely = -relx*((1.0f/1300)*det.bbox_centerx - 48.0f/65);
        
        detection3D.results.resize(1);
        detection3D.results[0].pose.pose.position.x = relx;
        detection3D.results[0].pose.pose.position.y = rely;
        
        // do all the other stuff you wanna put in a 3d detection array
        detection3D.results[0].hypothesis.score = det.confidence;
        detection3D.results[0].hypothesis.class_id = det.color;

        final_detections_arr.detections.push_back(detection3D);
      }

      auto stamp = det_set.timestamp;
      final_detections_arr.header.stamp = stamp;

      publisher_->publish(final_detections_arr);
    }
    rclcpp::Publisher<vision_msgs::msg::Detection3DArray>::SharedPtr publisher_;
    void detection_callback(const vision_msgs::msg::Detection2DArray::SharedPtr msg) 
    {
      // save the detections into a vector to add to the detections buffer
      Detections_Set new_det_set;
      for (int i = 0; i < (int)msg->detections.size(); i ++) {
        Detection det;
        det.bbox_centerx = msg->detections[i].bbox.center.position.x;
        det.bbox_centery = msg->detections[i].bbox.center.position.y;
        det.bbox_sizex = msg->detections[i].bbox.size_x;
        det.bbox_sizey = msg->detections[i].bbox.size_y;
        det.confidence = msg->detections[i].results[0].hypothesis.score;
        det.color = std::stoi(msg->detections[i].results[0].hypothesis.class_id);
        new_det_set.detections.push_back(det);
        // you might notice that we ignore orientation. this is because we use yolov8 and it doesn't give it; we dont car
      }
      new_det_set.timestamp = msg->header.stamp;
      add_to_buf(detections_buffer, new_det_set);
      // check if detections/disparity callback has a header that matches up
      try_match_times();
    }
    rclcpp::Subscription<vision_msgs::msg::Detection2DArray>::SharedPtr yolo_detection_subscription_;

    void disparityimg_callback(const stereo_msgs::msg::DisparityImage::SharedPtr disparity)
    {
      Disparity disp;
      disp.disparity_image = cv_bridge::toCvShare(std::make_shared<sensor_msgs::msg::Image>(disparity->image))->image;
      disp.f = disparity->f;
      disp.T = disparity->t;
      disp.timestamp = disparity->header.stamp;
      
      add_to_buf(disparities_buffer, disp);
      // check if detections/disparity callback has a header that matches up
      try_match_times(); // fix: was missing
    }
    rclcpp::Subscription<stereo_msgs::msg::DisparityImage>::SharedPtr stereo_disparityimg_subscription_;
    
    size_t count_; // fix: was commented out but still referenced in constructor initializer
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<YoloDepthFuser>());
  rclcpp::shutdown();
  return 0;
}