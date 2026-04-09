#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <optional>


#include "rclcpp/rclcpp.hpp"

#include <image_transport/image_transport.hpp> // Using image_transport allows us to publish and subscribe to compressed image streams in ROS2
#include <opencv2/opencv.hpp> // We include everything about OpenCV as we don't care much about compilation time at the moment.
// #include <opencv2/core.hpp>
// #include <opencv2/imgproc.hpp>
#include <cv_bridge/cv_bridge.h>

//#include "message_filters/subscriber.h"
#include "message_filters/subscriber.hpp"
//#include "message_filters/synchronizer.h"
#include "message_filters/time_synchronizer.hpp"

#include "vision_msgs/msg/detection3_d_array.hpp"
#include "vision_msgs/msg/detection2_d_array.hpp"
#include "stereo_msgs/msg/disparity_image.hpp"

#define BUFFER_LEN 10
#define MAX_TIME_DIFF 0.01 // in seconds between the disparity image and detection
#define CROP_RATIO 0.08 // CROP_RATIO*2 = the percent of the bucket per dimension that is included in the crop
// the crop is median'd to find an approximation for the closest point to the camera and from there the center
#define BUCKET_RADIUS 0.5 // in m
#define BASE_LINK_OFFSET_X 0.114 // in m. x distance from cameras to base link
#define BASE_LINK_OFFSET_Y 0.128 // in m. y distance from left camera to base link (bc YOLO dets are from left camera)
#define CAMERAS_DIST 0.256 // distance between cameras in m
#define FOCAL_LEN 1300 // focal length of cameras in pixels

#define NBINS 10 //Number of bins for histogram of data

using namespace std::chrono_literals;
using std::placeholders::_1;
using std::placeholders::_2;

class YoloDepthFuser : public rclcpp::Node
{
  public:
    YoloDepthFuser()
    : Node("yolo_depth_fuser")
    {
      rclcpp::QoS yolo_qos = rclcpp::QoS(50);
      rclcpp::QoS disp_qos = rclcpp::QoS(10);

      publisher_ = this->create_publisher<vision_msgs::msg::Detection3DArray>("fused_vision_measurements", 10);

      // yolo_detection_subscription_ = this->create_subscription<vision_msgs::msg::Detection2DArray>(
      // "PLACEHOLDER1", 10, std::bind(&YoloDepthFuser::detection_callback, this, _1)); // PLACEHOLDER TOPICS
      // stereo_disparityimg_subscription_ = this->create_subscription<stereo_msgs::msg::DisparityImage>(
      // "PLACEHOLDER2", 10, std::bind(&YoloDepthFuser::disparityimg_callback, this, _1)); // PLACEHOLDER TOPICS

      yolo_detection_subscription_.subscribe(this, "/detections_output", yolo_qos.get_rmw_qos_profile());
      stereo_disparityimg_subscription_.subscribe(this, "/disparity", disp_qos.get_rmw_qos_profile());

      sync = std::make_shared<message_filters::TimeSynchronizer<vision_msgs::msg::Detection2DArray,
      stereo_msgs::msg::DisparityImage>>(yolo_detection_subscription_, stereo_disparityimg_subscription_, BUFFER_LEN);

      //sync->setAgePenalty(0.50);
      //sync->registerCallback(std::bind(&YoloDepthFuser::SyncCallback, this, _1, _2));
      sync->registerCallback(&YoloDepthFuser::SyncCallback, this);
    }
    
  private:
    message_filters::Subscriber<vision_msgs::msg::Detection2DArray> yolo_detection_subscription_;
    message_filters::Subscriber<stereo_msgs::msg::DisparityImage> stereo_disparityimg_subscription_;
    std::shared_ptr<message_filters::TimeSynchronizer<vision_msgs::msg::Detection2DArray,
      stereo_msgs::msg::DisparityImage>> sync;

    // Source - https://stackoverflow.com/q/30078756. Changed from doubles -> floats
    // Posted by CV_User, modified by community. See post 'Timeline' for change history
    // Retrieved 2026-04-02, License - CC BY-SA 4.0
    // float medianMat(cv::Mat Input){    
    //   Input = Input.reshape(0,1); // spread Input Mat to single row
    //   std::vector<float> vecFromMat;
    //   Input.copyTo(vecFromMat); // Copy Input Mat to vector vecFromMat
    //   std::nth_element(vecFromMat.begin(), vecFromMat.begin() + vecFromMat.size() / 2, vecFromMat.end());
    //   return vecFromMat[vecFromMat.size() / 2];
    // }
    float medianMat(cv::Mat Input, int nVals){
      // COMPUTE HISTOGRAM OF SINGLE CHANNEL MATRIX
      float range[] = { 0, nVals };
      const float* histRange = { range };
      bool uniform = true; bool accumulate = false;
      cv::Mat hist;
      calcHist(&Input, 1, 0, cv::Mat(), hist, 1, &nVals, &histRange, uniform, accumulate);
      // COMPUTE CUMULATIVE DISTRIBUTION FUNCTION (CDF)
      cv::Mat cdf;
      hist.copyTo(cdf);
      for (int i = 1; i <= nVals-1; i++){
        cdf.at<float>(i) += cdf.at<float>(i - 1);
      }
      cdf /= Input.total();
      // COMPUTE MEDIAN
      float medianVal;
      for (int i = 0; i <= nVals-1; i++){
        if (cdf.at<float>(i) >= 0.5) { medianVal = i;  break; }
      }
      return medianVal/nVals; 
    }

    void SyncCallback(const vision_msgs::msg::Detection2DArray & det_arr,
        const stereo_msgs::msg::DisparityImage & disp)
    {
      RCLCPP_INFO(get_logger(), "Sync callback with %u and %u as times", (det_arr.header.stamp.sec), (disp.header.stamp.sec));
      cv::Mat disparity_image = cv_bridge::toCvShare(std::make_shared<sensor_msgs::msg::Image>(disp.image)).image;
      vision_msgs::msg::Detection3DArray final_detections_arr;
      for (const auto& det : det_arr.detections) { 
        vision_msgs::msg::Detection3D detection3D;
  
        // calculate median disparity of little middle section of bounding box
        // cv::Mat cropped = disparity_image(cv::Range((det.bbox.center.position.y - det.bbox.size_y*CROP_RATIO), det.bbox.center.position.y + det.bbox.size_y*CROP_RATIO),
        //       cv::Range((det.bbox.center.position.x - det.bbox.size_x*CROP_RATIO), (det.bbox.center.position.x + det.bbox.size_x*CROP_RATIO)));
        // light ess outputs 480x288, yolo outputs 640x640 (black bars on top and bottom, 80 tall each)
        cv::Rect crop_rect = cv::Rect(0.75*(det.bbox.center.position.x - .5*det.bbox.size_x*CROP_RATIO), 0.6*(det.bbox.center.position.y - 80 - .5*det.bbox.size_y*CROP_RATIO),
            (0.75*det.bbox.size_x*CROP_RATIO), (0.6*det.bbox.size_y*CROP_RATIO));
        cv::Mat cropped = disparity_image(crop_rect);
        cv::imshow("apple", cropped);
        float medianermaktuallydisparity=medianMat(cropped, NBINS);
        RCLCPP_INFO(get_logger(), "Median disparity found to be %f", medianermaktuallydisparity);
        if (medianermaktuallydisparity < 0) {continue;} // check that it's valid

        // find corresponding real depth (subtract the radius of the bucket to get the center)
        float relx = FOCAL_LEN*CAMERAS_DIST/medianermaktuallydisparity - BUCKET_RADIUS + BASE_LINK_OFFSET_X; // called relx for consistency with buckalization
        
        // calculated through FANCY MATH with our specific camera VL-FPD3-8CAM-RPI22.
        // https://docs.opencv.org/2.4/modules/calib3d/doc/camera_calibration_and_3d_reconstruction.html
        // or geometry if you're boring like that; they yield the same end result
        float rely = -relx*((1.0f/1300)*det.bbox.center.position.x - 48.0f/65) + BASE_LINK_OFFSET_Y;
        
        detection3D.results.resize(1);
        detection3D.results[0].pose.pose.position.x = relx;
        detection3D.results[0].pose.pose.position.y = rely;
        
        // do all the other stuff you wanna put in a 3d detection array
        detection3D.results[0].hypothesis.score = det.results[0].hypothesis.score;
        detection3D.results[0].hypothesis.class_id = det.results[0].hypothesis.class_id;

        final_detections_arr.detections.push_back(detection3D);
      }

      final_detections_arr.header.stamp = det_arr.header.stamp;
      RCLCPP_INFO(get_logger(), "Stamped and incoming publishing");
      publisher_->publish(final_detections_arr);
    }
    rclcpp::Publisher<vision_msgs::msg::Detection3DArray>::SharedPtr publisher_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<YoloDepthFuser>());
  rclcpp::shutdown();
  return 0;
}