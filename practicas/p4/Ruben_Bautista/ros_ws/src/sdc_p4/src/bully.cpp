#include "sdc_p4/bully.hpp"

#include <unistd.h>
#include <algorithm>
#include <string>
#include <memory>

namespace sdc_p4
{

using namespace std::chrono_literals;

BullyNode::BullyNode(const rclcpp::NodeOptions & options)
: Node("bully_node_" + std::to_string(getpid()), options),
  pid_(getpid()),
  current_state_(NodeState::FOLLOWER),
  election_in_progress_(false),
  highest_candidate_pid_(-1),
  i_started_election_(false),
  is_initializing_(true)
{
  declare_parameter("role", "follower");
  start_role_ = get_parameter("role").as_string();
  
  role_str_ = "follower";

  rclcpp::QoS qos_settings(rclcpp::QoSInitialization::from_rmw
    (rmw_qos_profile_default));
  qos_settings.keep_last(10);
  qos_settings.reliable();
  qos_settings.durability_volatile();

  heartbeat_pub_ = create_publisher<std_msgs::msg::Float64>(
    "/sdc/p4/heartbeat", qos_settings);
  
  election_start_pub_ = create_publisher<std_msgs::msg::Int32>(
    "/sdc/p4/election", qos_settings);
  
  candidates_pub_ = create_publisher<std_msgs::msg::Int32>(
    "/sdc/p4/candidates_election", qos_settings);
  
  new_leader_pub_ = create_publisher<std_msgs::msg::Int32>(
    "/sdc/p4/new_leader", qos_settings);

  heartbeat_sub_ = create_subscription<std_msgs::msg::Float64>(
    "/sdc/p4/heartbeat", qos_settings,
    std::bind(&BullyNode::receive_heartbeat, this, std::placeholders::_1));
  
  election_start_sub_ = create_subscription<std_msgs::msg::Int32>(
    "/sdc/p4/election", qos_settings,
    std::bind(&BullyNode::election_process, this, std::placeholders::_1));
  
  candidates_sub_ = create_subscription<std_msgs::msg::Int32>(
    "/sdc/p4/candidates_election", qos_settings,
    std::bind(&BullyNode::receive_candidate, this, std::placeholders::_1));
  
  new_leader_sub_ = create_subscription<std_msgs::msg::Int32>(
    "/sdc/p4/new_leader", qos_settings,
    std::bind(&BullyNode::announce_new_leader, this, std::placeholders::_1));

  if (start_role_ == "leader") {
    // Set the state to LEADER, then start election
    change_state(NodeState::LEADER);
    start_election();    
  } else {
    // Start as FOLLOWER with initialization delay
    change_state(NodeState::FOLLOWER);
    last_heartbeat_time_ = now();

    init_timer_ = create_wall_timer(
      std::chrono::milliseconds(FOLLOWER_START_DELAY),
      [this]() {
        is_initializing_ = false;
        if (init_timer_) {
          init_timer_->cancel();
        }
        
        // Check if heartbeat was received during initialization
        rclcpp::Time current_time = now();
        double time_since_last_heartbeat = (current_time - last_heartbeat_time_).seconds();
        
        if (time_since_last_heartbeat * 1000 > HEARTBEAT_TIMEOUT) {
          RCLCPP_WARN(get_logger(), "[%s] No heartbeat received in the last %.0f seconds!", 
                      role_str_.c_str(), time_since_last_heartbeat);
          start_election();
        } else {
          // Check heartbeat timeout
          timeout_timer_ = create_wall_timer(
            std::chrono::milliseconds(HEARTBEAT_TIMEOUT),
            std::bind(&BullyNode::timeout_callback, this));
        }
      });
  }
}

/*change_state(): change the current state of the node
and update the role string*/
void BullyNode::change_state(NodeState new_state)
{
  current_state_ = new_state;
  role_str_ = state_2_string(new_state);
}

/*state_2_string(): convert NodeState enum to string*/
std::string BullyNode::state_2_string(NodeState state)
{
  switch (state) {
    case NodeState::FOLLOWER: return "follower";
    case NodeState::LEADER: return "leader";
    case NodeState::ELECTION: return "election";
    default: return "unknown";
  }
}

/*get_epoch(): get the current time
in seconds since epoch*/
double BullyNode::get_epoch()
{
  auto now = this->now();
  return now.seconds() + now.nanoseconds() * 1e-9;
}

/*heartbeat_callback(): send heartbeat message if is leader*/
void BullyNode::heartbeat_callback()
{
  if (current_state_ != NodeState::LEADER) {
    return;
  }

  auto msg = std_msgs::msg::Float64();
  msg.data = get_epoch();
  heartbeat_pub_->publish(msg);
  
  RCLCPP_INFO(get_logger(), "[%s] Sending heartbeat", role_str_.c_str());
}

/*timeout_callback(): check for heartbeat timeout is exceeded
and start election if necessary*/
void BullyNode::timeout_callback()
{
  if (is_initializing_) {
    return;
  }
  
  if (current_state_ != NodeState::FOLLOWER) {
    return;
  }

  rclcpp::Time current_time = now();
  double time_since_last_heartbeat = (current_time - last_heartbeat_time_).seconds();
  
  if (time_since_last_heartbeat * 1000 > HEARTBEAT_TIMEOUT) {
    RCLCPP_WARN(get_logger(), 
    "[%s] No heartbeat received in the last %.0f seconds!", 
      role_str_.c_str(), time_since_last_heartbeat);
    
    start_election();
  }
}

/*receive_heartbeat(): process received heartbeat messages*/
void BullyNode::receive_heartbeat(const std_msgs::msg::Float64::SharedPtr msg)
{

  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  } else if (current_state_ == NodeState::ELECTION) {
    return;
  } else if (current_state_ == NodeState::LEADER) {
    return;
  }

  last_heartbeat_time_ = now();
  
  RCLCPP_INFO(get_logger(), "[%s] Received heartbeat", role_str_.c_str());
}

/*election_process(): process received election start messages*/
void BullyNode::election_process(const std_msgs::msg::Int32::SharedPtr msg)
{
  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  }

  if (election_in_progress_) {
    return;
  }

  election_start_time_ = this->now();
  election_in_progress_ = true;
  change_state(NodeState::ELECTION);
  
  RCLCPP_INFO(get_logger(), "[%s] Received message for starting the election", 
              role_str_.c_str());
  
  // Clear previous candidates
  candidates_.clear();
  highest_candidate_pid_ = -1;
  
  // Stop timers during election
  if (timeout_timer_) {
    timeout_timer_->cancel();
  }
  
  if (heartbeat_timer_) {
    heartbeat_timer_->cancel();
  }
  
  // Send our PID as candidate
  auto candidate_msg = std_msgs::msg::Int32();
  candidate_msg.data = pid_;
  candidates_pub_->publish(candidate_msg);
  
  RCLCPP_INFO(get_logger(), "[%s] Sending my PID for leader election: %d", 
              role_str_.c_str(), pid_);
  
  // Add ourselves as candidates
  candidates_.insert(pid_);
  if (pid_ > highest_candidate_pid_) {
    highest_candidate_pid_ = pid_;
  }
  
  // Start election timeout timer
  election_timer_ = create_wall_timer(
    std::chrono::milliseconds(ELECTION_DURATION),
    std::bind(&BullyNode::election_callback, this),
    nullptr,
    true);
}

/*start_election(): initiate the election process*/
void BullyNode::start_election()
{
  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  }
  
  if (election_in_progress_) {
    return;
  }

  i_started_election_ = true;
  
  RCLCPP_INFO(get_logger(), "[%s] Send message to start election",
  role_str_.c_str());
  
  // Send empty message to start election
  auto election_msg = std_msgs::msg::Int32();
  election_msg.data = 0;
  election_start_pub_->publish(election_msg);
  
  // Handle locally as well
  auto dummy_msg = std::make_shared<std_msgs::msg::Int32>();
  election_process(dummy_msg);
}

//receive_candidate(): process received candidate messages
void BullyNode::receive_candidate(const std_msgs::msg::Int32::SharedPtr msg)
{
  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  }
  
  if (current_state_ != NodeState::ELECTION) {
    return;
  }

  int candidate_pid = msg->data;
  
  // If the candidate is ourselves, ignore the message
  if (candidate_pid == pid_) {
    if (candidates_.find(candidate_pid) == candidates_.end()) {
      candidates_.insert(candidate_pid);
      if (candidate_pid > highest_candidate_pid_) {
        highest_candidate_pid_ = candidate_pid;
      }
    }
    return;
  }
  
  RCLCPP_INFO(get_logger(), "[%s] Received candidate leader %d", 
              role_str_.c_str(), candidate_pid);
  
  candidates_.insert(candidate_pid);
  
  // Update highest candidate PID
  if (candidate_pid > highest_candidate_pid_) {
    highest_candidate_pid_ = candidate_pid;
  }
}

//election_callback(): handle election timeout
void BullyNode::election_callback()
{
  if (current_state_ != NodeState::ELECTION) {
    return;
  }

  // Find the highest PID among candidates
  int max_pid = -1;
  for (int pid : candidates_) {
    if (pid > max_pid) {
      max_pid = pid;
    }
  }
  
  // If no candidates, assume ourselves
  if (max_pid == -1) {
    max_pid = pid_;
  }
  
  // If we have the highest PID, announce ourselves as leader
  if (pid_ == max_pid) {
    send_pid();
  } else {
    // Backup timer in case the highest node fails
    auto backup_timer = create_wall_timer(
      3000ms,
      [this]() {
        if (current_state_ == NodeState::ELECTION) {
          // If there is still no leader, restart election
          reset_election();
          start_election();
        }
      },
      nullptr,
      true);
  }
}

//send_pid(): announce ourselves as the new leader
void BullyNode::send_pid()
{
  // Only announce if we are in election state
  if (current_state_ != NodeState::ELECTION) {
    return;
  }

  auto leader_msg = std_msgs::msg::Int32();
  leader_msg.data = pid_;
  new_leader_pub_->publish(leader_msg);
  RCLCPP_INFO(get_logger(), 
  "[%s] Sending my PID to set the new leader: %d",
   role_str_.c_str(), pid_);

  reset_election();
}

//announce_new_leader(): process received new leader announcements
void BullyNode::announce_new_leader(const std_msgs::msg::Int32::SharedPtr msg)
{
  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  }
  
  int new_leader_pid = msg->data;
  
  // If we are already follower, ignore
  if (current_state_ == NodeState::FOLLOWER) {
    return;
  }
  
  // If we are the new leader, announce ourselves
  if (new_leader_pid == pid_) {
    if (current_state_ == NodeState::ELECTION) {
      change_state(NodeState::LEADER);
      RCLCPP_INFO(get_logger(), "[%s] I'm the new leader", role_str_.c_str());
      start_heartbeat();
      reset_election();
    } else if (current_state_ == NodeState::LEADER) {
      return;
    }
    return;
  } else if (new_leader_pid > pid_) {
    RCLCPP_INFO(get_logger(), "[%s] There is a new leader %d",
    role_str_.c_str(), new_leader_pid);
    
    // If we were the leader, stop being so
    if (current_state_ == NodeState::LEADER) {
      if (heartbeat_timer_) {
        heartbeat_timer_->cancel();
      }
    }
    
    change_state(NodeState::FOLLOWER);
    reset_election();
    
    // Reset last heartbeat time and start timeout timer
    last_heartbeat_time_ = now();
    
    if (!is_initializing_) {
      if (timeout_timer_) {
        timeout_timer_->cancel();
      }
      timeout_timer_ = create_wall_timer(
        std::chrono::milliseconds(HEARTBEAT_TIMEOUT),
        std::bind(&BullyNode::timeout_callback, this));
    }
  }   
}

//reset_election(): reset election state and timers
void BullyNode::reset_election()
{
  election_in_progress_ = false;
  i_started_election_ = false;
  candidates_.clear();
  highest_candidate_pid_ = -1;
  
  if (election_timer_) {
    election_timer_->cancel();
    election_timer_.reset();
  }
}

//start_heartbeat(): start sending heartbeat messages periodically
void BullyNode::start_heartbeat()
{
  if (timeout_timer_) {
    timeout_timer_->cancel();
    timeout_timer_.reset();
  }
  
  if (heartbeat_timer_) {
    heartbeat_timer_->cancel();
  }
  
  // Start heartbeat timer every 5 seconds
  heartbeat_timer_ = create_wall_timer(
    std::chrono::milliseconds(HEARTBEAT_PERIOD),
    [this]() {
      if (this->current_state_ == NodeState::LEADER) {
        auto msg = std_msgs::msg::Float64();
        msg.data = this->get_epoch();
        this->heartbeat_pub_->publish(msg);
        
        RCLCPP_INFO(this->get_logger(), "[%s] Sending heartbeat", 
        this->role_str_.c_str());
      }
    });
  
  // Send first heartbeat immediately
  if (current_state_ == NodeState::LEADER) {
    auto msg = std_msgs::msg::Float64();
    msg.data = get_epoch();
    heartbeat_pub_->publish(msg);
    
    RCLCPP_INFO(get_logger(), "[%s] Sending heartbeat", role_str_.c_str());
  }
}

}  // namespace sdc_p4

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  
  auto node = std::make_shared<sdc_p4::BullyNode>();
  rclcpp::spin(node);
  
  rclcpp::shutdown();
  return 0;
}