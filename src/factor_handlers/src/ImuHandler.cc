/*
 * Copyright Notes
 * Authors: Matteo Palieri      (matteo.palieri@jpl.nasa.gov)
 */

// Includes
#include <factor_handlers/ImuHandler.h>

namespace pu = parameter_utils;

// Constructor & Destructor
// --------------------------------------------------------------------------------

ImuHandler::ImuHandler() 
  : buffer_size_limit_(1000),
    ts_threshold_(0.1) {
    ROS_INFO("ImuHandler Class Constructor");
}

ImuHandler::~ImuHandler() {
    ROS_INFO("ImuHandler Class Destructor");
}

// Initialization
// --------------------------------------------------------------------------------

bool ImuHandler::Initialize(const ros::NodeHandle& n){
    ROS_INFO("ImuHandler - Initialize");    
    name_ = ros::names::append(n.getNamespace(), "ImuHandler");
    if (!LoadParameters(n)) {
        ROS_ERROR("%s: Failed to load parameters.", name_.c_str());
        return false;
    }
    if (!RegisterCallbacks(n)) {
        ROS_ERROR("%s: Failed to register callbacks.", name_.c_str());
        return false;
    }    
    return true;
}

bool ImuHandler::LoadParameters(const ros::NodeHandle& n) {    
    ROS_INFO("ImuHandler - LoadParameters");
    if (!pu::Get("buffer_size_limit", buffer_size_limit_))
        return false;
    if (!pu::Get("ts_threshold", ts_threshold_))
        return false;
    if (!pu::Get("base_frame_id", base_frame_id_))
        return false;
    if (!pu::Get("imu_frame_id", imu_frame_id_)) 
        return false;
    LoadCalibrationFromTfTree();
    return true;
}

bool ImuHandler::RegisterCallbacks(const ros::NodeHandle& n) {    
    ROS_INFO("%s: Registering online callbacks in ImuHandler", name_.c_str());  
    ros::NodeHandle nl(n);
    imu_sub_ = nl.subscribe("imu_topic", 1000, &ImuHandler::ImuCallback, this);     
    return true;
}

// Callback
// --------------------------------------------------------------------------------

void ImuHandler::ImuCallback(const ImuMessage::ConstPtr& msg) {    
    ROS_INFO("ImuHandler - ImuCallback"); 
    if (CheckBufferSize() > buffer_size_limit_){
        imu_buffer_.erase(imu_buffer_.begin());
    }   
    if (!InsertMsgInBuffer(msg)){
        ROS_WARN("ImuHandler - ImuCallback - Unable to store message in buffer");
    }
}

// LAMP Interface
// --------------------------------------------------------------------------------

FactorData* ImuHandler::GetData(){
    ROS_INFO("ImuHandler - GetData");
    ImuData* factors_output = new ImuData(factors_);   
    factors_output->b_has_data = false; 

    if (CheckBufferSize()==0) {
        ROS_WARN("Buffers are empty, returning no data");
        return factors_output;
    }

    ImuQuaternion imu_quaternion;
    ros::Time query_stamp_ros;
    query_stamp_ros.fromSec(query_stamp_);
    
    if (GetQuaternionAtTime(query_stamp_ros, imu_quaternion)==true){        
        ROS_INFO("Successfully extracted data from buffer");
        Eigen::Vector3d imu_ypr = QuaternionToYpr(imu_quaternion);
        ImuFactor new_factor(CreateAttitudeFactor(imu_ypr));

        factors_output->b_has_data = true; 
        factors_output->type = "imu";        
        factors_output->factors.push_back(new_factor);

        ResetFactorData();
    }

    else {
        factors_output->b_has_data = false; 
    }

    return factors_output; 
}

// Buffers
// --------------------------------------------------------------------------------

bool ImuHandler::InsertMsgInBuffer(const ImuMessage::ConstPtr& msg) {  
    ROS_INFO("ImuHandler - InsertMsgInBuffer");  
    auto initial_size = imu_buffer_.size();    
    auto current_time = msg->header.stamp.toSec();    
    ImuQuaternion current_quaternion;
    tf::quaternionMsgToEigen(msg->orientation, current_quaternion);
    // Convert quaternion from imu frame to base frame, then store it 
    current_quaternion = I_T_B_q_*current_quaternion*I_T_B_q_.inverse(); 
    imu_buffer_.insert({current_time, current_quaternion});
    auto final_size = imu_buffer_.size();    
    if (final_size == (initial_size+1)) {
        return true;
    }
    else {
        return false; 
    } 
}

int ImuHandler::CheckBufferSize() const {    
    ROS_INFO("ImuCallback - ChechBufferSize");    
    return imu_buffer_.size();
}

bool ImuHandler::ClearBuffer() {
    ROS_INFO("ImuHandler - ClearBuffer");    
    imu_buffer_.clear();    
    if (CheckBufferSize()==0) {
        ROS_INFO("Successfully cleared buffer");
        return true;
    }
    else {
        ROS_WARN("Could not clear buffer");
        return false; 
    }
}

// Quaternions
// --------------------------------------------------------------------------------

bool ImuHandler::GetQuaternionAtTime(const ros::Time& stamp, ImuQuaternion& imu_quaternion) const {
    // TODO: Implement GetValueAtTime in base class as it is a common functionality needed by all handlers
    ROS_INFO("ImuHandler - GetQuaternionAtTime"); 
    if (imu_buffer_.size() == 0){
        return false;
    }
    auto itrTime = imu_buffer_.lower_bound(stamp.toSec());
    auto time2 = itrTime->first;
    double time_diff;
    if (itrTime == imu_buffer_.begin()) {
        imu_quaternion = itrTime->second;
        time_diff = itrTime->first - stamp.toSec();
        ROS_WARN("Timestamp before the start of the imu buffer");
        ROS_INFO_STREAM("time diff is: " << time_diff);
    } 
    else if (itrTime == imu_buffer_.end()) {
        ROS_WARN("Timestamp past the end of the imu buffer");
        itrTime--;
        imu_quaternion = itrTime->second;
        time_diff = stamp.toSec() - itrTime->first;
        ROS_INFO_STREAM("input time is " << stamp.toSec()
                                        << "s, and latest time is "
                                        << itrTime->first << " s"
                                        << " diff is " << time_diff);
    } 
    else {
        double time1 = std::prev(itrTime, 1)->first;
        if (time2 - stamp.toSec() < stamp.toSec() - time1) {
            imu_quaternion = itrTime->second;
            time_diff = time2 - stamp.toSec();
        } 
        else {
            imu_quaternion = std::prev(itrTime, 1)->second;
            time_diff = stamp.toSec() - time1;
        }
    }
    if (time_diff > ts_threshold_) { 
        ROS_WARN("Time difference between request and latest ImuQuaternion is too large, returning no ImuQuaternion");
        ROS_INFO_STREAM("Time difference is "
                        << time_diff << "s, threshold is: " << ts_threshold_);
        return false;
    }     
  return true; 
}

Eigen::Vector3d ImuHandler::QuaternionToYpr(const ImuQuaternion& imu_quaternion) const {
    ROS_INFO("ImuHandler - QuaternionToYpr");
    auto ypr = imu_quaternion.toRotationMatrix().eulerAngles(2, 1, 0);
    // Returned ypr is in radians
    return ypr;
}

// Factors
// --------------------------------------------------------------------------------

Pose3AttitudeFactor ImuHandler::CreateAttitudeFactor(const Eigen::Vector3d& imu_ypr) const {
    ROS_INFO("ImuHandler - CreateAttitudeFactor");
    Unit3 ref(0, 0, -1); 
    SharedNoiseModel model = noiseModel::Isotropic::Sigma(2, 0.25);    
    // Yaw can be set to 0
    Rot3 R_imu = Rot3::Ypr(0, double(imu_ypr[1]), double(imu_ypr[2])); 
    Unit3 meas = Unit3(Rot3(R_imu.transpose()).operator*(ref));
    Pose3AttitudeFactor factor(query_key_, meas, model, ref);
    return factor;
}

void ImuHandler::ResetFactorData() {
    ROS_INFO("ImuHandler - ResetFactorData");
    factors_.b_has_data = false;
    factors_.type = "imu";
    factors_.factors.clear();
}

bool ImuHandler::SetTimeForImuAttitude(const ros::Time& stamp) {        
    ROS_INFO("ImuHandler - SetTimeForImuAttitude");    
    query_stamp_ = stamp.toSec();    
    if (query_stamp_ == stamp.toSec()) {
        return true;
    }
    else {
        ROS_WARN("Could not store received stamp into private class member");
        return false;
    }
}

bool ImuHandler::SetKeyForImuAttitude(const Symbol& key) {        
    ROS_INFO("ImuHandler - SetKeyForImuAttitude");    
    query_key_ = key;    
    if (query_key_ == key) {
        return true;
    }
    else {
        ROS_WARN("Could not store received symbol into private class member");
        return false;
    }
}

// Transformations
// --------------------------------------------------------------------------------

bool ImuHandler::LoadCalibrationFromTfTree(){
    ROS_WARN_DELAYED_THROTTLE(2.0, 
                            "Waiting for \'%s\' and \'%s\' to appear in tf_tree...",
                            imu_frame_id_,
                            base_frame_id_);
    tf::StampedTransform imu_T_base_transform;
    try {   
        imu_T_base_listener_.waitForTransform(
            imu_frame_id_,
            base_frame_id_,
            ros::Time(0),
            ros::Duration(2.0));
      
        imu_T_base_listener_.lookupTransform(
            imu_frame_id_,
            base_frame_id_,
            ros::Time(0),
            imu_T_base_transform);

        geometry_msgs::TransformStamped imu_T_base_tmp_msg;
        tf::transformStampedTFToMsg(imu_T_base_transform, imu_T_base_tmp_msg);        
        tf::transformMsgToEigen(imu_T_base_tmp_msg.transform, I_T_B_);
        B_T_I_ = I_T_B_.inverse();
        ROS_INFO_STREAM("Loaded pose_sensor to imu calibration B_T_L:");
        std::cout << I_T_B_.translation() << std::endl;
        std::cout << I_T_B_.rotation() << std::endl;        
        I_T_B_q_ = Eigen::Quaterniond(I_T_B_.rotation());
        ROS_INFO("q: x: %.3f, y: %.3f, z: %.3f, w: %.3f", I_T_B_q_.x(), I_T_B_q_.y(), I_T_B_q_.z(), I_T_B_q_.w());
        return true; 
    }     
    catch (tf::TransformException ex) {
        ROS_ERROR("%s", ex.what());
        I_T_B_ = Eigen::Affine3d::Identity();
        B_T_I_ = Eigen::Affine3d::Identity();
        return false;
    }
}