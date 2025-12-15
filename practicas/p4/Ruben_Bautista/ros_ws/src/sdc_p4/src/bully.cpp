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
  
  // **AÑADIR: Inicializar role_str_ desde el principio**
  role_str_ = "follower";  // Estado inicial por defecto
  
  // Primero inicializar TODOS los publishers y subscribers
  rclcpp::QoS qos_settings(rclcpp::QoSInitialization::from_rmw(rmw_qos_profile_default));
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
    std::bind(&BullyNode::handle_heartbeat, this, std::placeholders::_1));
  
  election_start_sub_ = create_subscription<std_msgs::msg::Int32>(
    "/sdc/p4/election", qos_settings,
    std::bind(&BullyNode::handle_election_start, this, std::placeholders::_1));
  
  candidates_sub_ = create_subscription<std_msgs::msg::Int32>(
    "/sdc/p4/candidates_election", qos_settings,
    std::bind(&BullyNode::handle_candidate, this, std::placeholders::_1));
  
  new_leader_sub_ = create_subscription<std_msgs::msg::Int32>(
    "/sdc/p4/new_leader", qos_settings,
    std::bind(&BullyNode::handle_new_leader, this, std::placeholders::_1));

  if (start_role_ == "leader") {
    // **CORRECCIÓN: Cambiar estado a follower primero (como todos)**
    change_state(NodeState::FOLLOWER);
    RCLCPP_INFO(get_logger(), "[%s] Node started with PID: %d", role_str_.c_str(), pid_);
    
    // Esperar 1 segundo y luego iniciar elecciones
    init_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(LEADER_START_DELAY),
      [this]() {
        is_initializing_ = false;
        // Cancelar el timer después de usarlo
        if (init_timer_) {
          init_timer_->cancel();
        }
        start_election();
      });
    
  } else {
    // **CORRECCIÓN: Asegurar que se llama a change_state()**
    change_state(NodeState::FOLLOWER);
    RCLCPP_INFO(get_logger(), "[%s] Node started with PID: %d", role_str_.c_str(), pid_);
    
    // Configurar timer de timeout para followers DESPUÉS de 7 segundos
    last_heartbeat_time_ = now();
    
    // Crear un timer para activar el follower después de 7 segundos
    init_timer_ = create_wall_timer(
      std::chrono::milliseconds(FOLLOWER_START_DELAY),
      [this]() {
        is_initializing_ = false;
        // Cancelar el timer después de usarlo
        if (init_timer_) {
          init_timer_->cancel();
        }
        
        // Verificar si ya recibimos heartbeat mientras dormíamos
        rclcpp::Time current_time = now();
        double time_since_last_heartbeat = (current_time - last_heartbeat_time_).seconds();
        
        if (time_since_last_heartbeat * 1000 > HEARTBEAT_TIMEOUT) {
          RCLCPP_WARN(get_logger(), "[%s] No heartbeat received in the last %.0f seconds!", 
                      role_str_.c_str(), time_since_last_heartbeat);
          start_election();
        } else {
          // Configurar timeout timer normal
          timeout_timer_ = create_wall_timer(
            std::chrono::milliseconds(HEARTBEAT_TIMEOUT),
            std::bind(&BullyNode::timeout_callback, this));
        }
      });
  }
}

void BullyNode::change_state(NodeState new_state)
{
  current_state_ = new_state;
  role_str_ = state_to_string(new_state);
}

std::string BullyNode::state_to_string(NodeState state)
{
  switch (state) {
    case NodeState::FOLLOWER: return "follower";
    case NodeState::LEADER: return "leader";
    case NodeState::ELECTION: return "election";
    default: return "unknown";
  }
}

double BullyNode::get_current_epoch()
{
  auto now = this->now();
  return now.seconds() + now.nanoseconds() * 1e-9;
}

void BullyNode::heartbeat_callback()
{
  if (current_state_ != NodeState::LEADER) {
    return;
  }

  auto msg = std_msgs::msg::Float64();
  msg.data = get_current_epoch();
  heartbeat_pub_->publish(msg);
  
  RCLCPP_INFO(get_logger(), "[%s] Sending heartbeat", role_str_.c_str());
}

void BullyNode::timeout_callback()
{
  // Si estamos en inicialización, no hacer nada
  if (is_initializing_) {
    return;
  }
  
  if (current_state_ != NodeState::FOLLOWER) {
    return;
  }

  rclcpp::Time current_time = now();
  double time_since_last_heartbeat = (current_time - last_heartbeat_time_).seconds();
  
  if (time_since_last_heartbeat * 1000 > HEARTBEAT_TIMEOUT) {
    RCLCPP_WARN(get_logger(), "[%s] No heartbeat received in the last %.0f seconds!", 
                role_str_.c_str(), time_since_last_heartbeat);
    
    start_election();
  }
}

void BullyNode::handle_heartbeat(const std_msgs::msg::Float64::SharedPtr msg)
{
  // Si estamos inicializando como follower, ignorar heartbeats hasta despertar
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

void BullyNode::handle_election_start(const std_msgs::msg::Int32::SharedPtr msg)
{
  // Si estamos inicializando como follower, ignorar mensajes de elección
  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  }
  
  // SI ya hay una elección en curso, ignorar (evitar múltiples elecciones)
  if (election_in_progress_) {
    return;
  }

  election_start_time_ = this->now();
  election_in_progress_ = true;
  change_state(NodeState::ELECTION);
  
  RCLCPP_INFO(get_logger(), "[%s] Received message for starting the election", 
              role_str_.c_str());
  
  // Limpiar candidatos anteriores
  candidates_.clear();
  highest_candidate_pid_ = -1;
  
  // Parar timers durante elección
  if (timeout_timer_) {
    timeout_timer_->cancel();
  }
  
  if (heartbeat_timer_) {
    heartbeat_timer_->cancel();
  }
  
  // Enviar nuestro PID como candidato
  auto candidate_msg = std_msgs::msg::Int32();
  candidate_msg.data = pid_;
  candidates_pub_->publish(candidate_msg);
  
  RCLCPP_INFO(get_logger(), "[%s] Sending my PID for leader election: %d", 
              role_str_.c_str(), pid_);
  
  // Añadirnos a nosotros mismos como candidatos
  candidates_.insert(pid_);
  if (pid_ > highest_candidate_pid_) {
    highest_candidate_pid_ = pid_;
  }
  
  // Iniciar timer de elección (5 segundos)
  election_timer_ = create_wall_timer(
    std::chrono::milliseconds(ELECTION_DURATION),
    std::bind(&BullyNode::election_timeout_callback, this),
    nullptr,
    true);
}

void BullyNode::start_election()
{
  // Si estamos inicializando como follower, no iniciar elecciones
  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  }
  
  if (election_in_progress_) {
    return;
  }

  i_started_election_ = true;
  
  RCLCPP_INFO(get_logger(), "[%s] Send message to start election", role_str_.c_str());
  
  // Enviar mensaje vacío para iniciar elección
  auto election_msg = std_msgs::msg::Int32();
  election_msg.data = 0;
  election_start_pub_->publish(election_msg);
  
  // Manejar localmente también
  auto dummy_msg = std::make_shared<std_msgs::msg::Int32>();
  handle_election_start(dummy_msg);
}

void BullyNode::handle_candidate(const std_msgs::msg::Int32::SharedPtr msg)
{
  // Si estamos inicializando como follower, ignorar candidatos
  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  }
  
  if (current_state_ != NodeState::ELECTION) {
    return;
  }

  int candidate_pid = msg->data;
  
  // No registrar nuestro propio PID de nuevo
  if (candidate_pid == pid_) {
    return;
  }
  
  RCLCPP_INFO(get_logger(), "[%s] Received candidate leader %d", 
              role_str_.c_str(), candidate_pid);
  
  candidates_.insert(candidate_pid);
  
  if (candidate_pid > highest_candidate_pid_) {
    highest_candidate_pid_ = candidate_pid;
  }
}

void BullyNode::election_timeout_callback()
{
  if (current_state_ != NodeState::ELECTION) {
    return;
  }

  // Encontrar el PID más alto entre todos los candidatos
  int max_pid = -1;
  for (int pid : candidates_) {
    if (pid > max_pid) {
      max_pid = pid;
    }
  }
  
  // Si no hay otros candidatos, somos el único
  if (max_pid == -1) {
    max_pid = pid_;
  }
  
  // REGLA SIMPLE: Si tenemos el PID más alto, anunciarnos como líder
  // No esperar, no complicar la lógica
  if (pid_ == max_pid) {
    announce_new_leader();
  } else {
    // Timer de backup por si el nodo más alto falla
    auto backup_timer = create_wall_timer(
      3000ms,
      [this]() {
        if (current_state_ == NodeState::ELECTION) {
          // Si aún no hay líder, reiniciar elección
          reset_election();
          start_election();
        }
      },
      nullptr,
      true);
  }
}

void BullyNode::announce_new_leader()
{
  // Solo anunciar si aún estamos en elección
  if (current_state_ != NodeState::ELECTION) {
    return;
  }

  auto leader_msg = std_msgs::msg::Int32();
  leader_msg.data = pid_;
  new_leader_pub_->publish(leader_msg);
    
  // Cambiar estado primero
  change_state(NodeState::LEADER);
  
  // Luego imprimir mensaje
  RCLCPP_INFO(get_logger(), "[%s] I'm the new leader", role_str_.c_str());
  
  // Finalmente iniciar heartbeats
  start_heartbeat();
  
  reset_election();
}

void BullyNode::handle_new_leader(const std_msgs::msg::Int32::SharedPtr msg)
{
  // Si estamos inicializando como follower, ignorar líderes hasta despertar
  if (is_initializing_ && current_state_ == NodeState::FOLLOWER) {
    return;
  }
  
  int new_leader_pid = msg->data;
  
  // Si somos nosotros, solo procesar si ya nos habíamos anunciado como líder
  if (new_leader_pid == pid_) {
    // Solo si estamos en elección significa que ya nos anunciamos
    if (current_state_ == NodeState::ELECTION) {
      change_state(NodeState::LEADER);
      start_heartbeat();
      reset_election();
    }
    return;
  } else if (new_leader_pid > pid_) {
    // Si éramos líder, dejar de serlo
    if (current_state_ == NodeState::LEADER) {
      if (heartbeat_timer_) {
        heartbeat_timer_->cancel();
      }
    }
    
    change_state(NodeState::FOLLOWER);
    reset_election();
    
    // Resetear heartbeat tracking
    last_heartbeat_time_ = now();
    
    // Configurar timeout timer solo si ya no estamos inicializando
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

void BullyNode::start_heartbeat()
{
  // Parar timers existentes
  if (timeout_timer_) {
    timeout_timer_->cancel();
    timeout_timer_.reset();
  }
  
  if (heartbeat_timer_) {
    heartbeat_timer_->cancel();
  }
  
  // Iniciar timer de heartbeat cada 5 segundos
  heartbeat_timer_ = create_wall_timer(
    std::chrono::milliseconds(HEARTBEAT_PERIOD),
    [this]() {
      if (this->current_state_ == NodeState::LEADER) {
        auto msg = std_msgs::msg::Float64();
        msg.data = this->get_current_epoch();
        this->heartbeat_pub_->publish(msg);
        
        RCLCPP_INFO(this->get_logger(), "[%s] Sending heartbeat", this->role_str_.c_str());
      }
    });
  
  // Enviar primer heartbeat inmediatamente
  if (current_state_ == NodeState::LEADER) {
    auto msg = std_msgs::msg::Float64();
    msg.data = get_current_epoch();
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