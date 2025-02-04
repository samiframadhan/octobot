/*
 * Blink
 * Turns on an LED on for one second,
 * then off for one second, repeatedly.
 */

#define STM_CODE 0
// STM Only = 0
// Dry STM = 1

#include <Arduino.h>

#include <stm_include.h>
// #include <dry_stm_include.h>

#include <micro_ros_platformio.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/int8.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/int64.h>
#include <std_msgs/msg/float32.h>

#include <std_srvs/srv/trigger.h>
#include <stm_interface/srv/relay_control.h>
#include <stm_interface/srv/motor_status.h>
#include <stm_interface/srv/tool_status.h>

#ifndef LED_BUILTIN
  #define LED_BUILTIN PC13
#endif

#if !defined(MICRO_ROS_TRANSPORT_ARDUINO_SERIAL)
#error This example is only avaliable for Arduino framework with serial transport.
#endif

std_msgs__msg__Int64 distL;
rcl_publisher_t distL_publisher;

std_msgs__msg__Int64 distR;
rcl_publisher_t distR_publisher;

std_msgs__msg__Float32 wireVal;
rcl_publisher_t wire_publisher;

std_msgs__msg__Int8 motorDirection;
std_msgs__msg__Int32 pose_msg;
rcl_subscription_t motor_subscriber;
int pose = 0;

std_msgs__msg__Float32 flowVal;
rcl_publisher_t flow_publisher;

// TODO: Service

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t flow_timer;
rcl_timer_t wire_timer;

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

// Error handle loop
void error_loop() {
  while(1) {
    delay(100);
  }
}

#pragma region STM_Only

void flowPulseCounter(){
  pulseCount ++;
}

void flow_callback(rcl_timer_t * timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    RCSOFTCHECK(rcl_publish(&flow_publisher, &flowVal, NULL));
    flowVal.data++;
    // detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN));

    // flowRate = ((FLOW_DELAY / (millis() - prev_flow_millis)) * pulseCount) / FLOW_CONSTANT;
    // prev_flow_millis = millis();


    // flowRate_ml = (flowRate / 60) * 1000;
    // totalVolume  += flowRate_ml;

    // debug("flowRate: ");
    // debug(flowRate_ml);
    // debug(" mL/s | ");


    // debug("flowVolume: ");
    // debug(totalVolume);
    // debugln(" mL");
    // pulseCount = 0;

    // flowVal.data = flowRate_ml;
    // RCSOFTCHECK(rcl_publish(&flow_publisher, &flowVal, NULL));
    // // flow_pub.publish(&flowVal);

    // attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseCounter, RISING);
  }
}

void wire_callback(rcl_timer_t * timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    RCSOFTCHECK(rcl_publish(&wire_publisher, &wireVal, NULL));
    wireVal.data++;
    // int16_t adcVal = D_WIRE.readADC(0);
    // float mVolt = adcVal * (ADS_GAIN / ADC_VAL_15);
    // float mAmp = mVolt / MV_TO_AMP;

    // debug("ADC: ");
    // debug(adcVal);
    // debug(" | mVolt: ");
    // debug(mVolt);
    // debug(" | mAmp: ");
    // debugln(mAmp);

    // wireVal.data = mAmp;
    // RCSOFTCHECK(rcl_publish(&wire_publisher, &wireVal, NULL));
    // wire_pub.publish(&wireVal);
  }
}

void gpioInit()
{
  /** I2C */
  Wire.setSDA(SDA1_PIN);
  Wire.setSCL(SCL1_PIN);
  Wire.begin();

  // draw wire
  D_WIRE.begin();
  D_WIRE.setGain(0);


  // 12V dc motor cytron
  pinMode(ACT_A1_PIN, OUTPUT);
  pinMode(ACT_A2_PIN, OUTPUT);

  // lin-servo (pwm-out)
  pinMode(LINEAR_SERVO_PIN, OUTPUT);

  // flow sensor
  pinMode(FLOW_SENSOR_PIN, INPUT);

  // inbuilt led
  pinMode(LED_PIN, OUTPUT);

  // switch
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(RELAY_3, OUTPUT);
  pinMode(RELAY_4, OUTPUT);




  // 12V DC motor (cytron)
  digitalWrite(ACT_A1_PIN, LOW);
  digitalWrite(ACT_A2_PIN, LOW);

  analogWrite(LINEAR_SERVO_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);

  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);
  digitalWrite(RELAY_3, LOW);
  digitalWrite(RELAY_4, LOW);

  // flow sensor pulse count
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseCounter, RISING);

}

void get_distL(){
  if (SerialL.available() > 0)
  {
    delay(4);

    if (SerialL.read() == 0xff)
    {
      dataL_buffer[0] = 0xff;
      for (int i = 1; i < 4; i++) {
        dataL_buffer[i] = SerialL.read();
      }

      csL = dataL_buffer[0] + dataL_buffer[1] + dataL_buffer[2];

      if (dataL_buffer[3] == csL) {
        distanceL = (dataL_buffer[1] << 8) + dataL_buffer[2];

        debug("distL: ");
        debug(distanceL);
        debugln(" mm");

        distL.data = distanceL;
        RCSOFTCHECK(rcl_publish(&distL_publisher, &distL, NULL));
        // distL_pub.publish(&distL);
      }
    }
  }
}

void get_distR(){
  if (SerialR.available() > 0)
  {
    delay(4);

    if (SerialR.read() == 0xff)
    {
      dataR_buffer[0] = 0xff;
      for (int i = 1; i < 4; i++) {
        dataR_buffer[i] = SerialR.read();
      }

      csR = dataR_buffer[0] + dataR_buffer[1] + dataR_buffer[2];

      if (dataR_buffer[3] == csR) {
        distanceR = (dataR_buffer[1] << 8) + dataR_buffer[2];

        debug("distR: ");
        debug(distanceR);
        debugln(" mm");

        distR.data = distanceR;
        RCSOFTCHECK(rcl_publish(&distR_publisher, &distR, NULL));
        // distR_publisher.publish(&distR);
      }
    }
  }
}

void pose_sub(const void * msgin){
  const std_msgs__msg__Int32 * msg = (const std_msgs__msg__Int32 *) msgin;

  pose = msg->data;
}

void controlLinServo(int posedata)
{
  if (millis() - prev_servo_millis > SERVO_DELAY)
  {
    if (posedata <= 0) {
      servoFlag = true;
    } else if (posedata >= 255) {
      servoFlag = false;
    }

    debug("servoFlag: ");
    debug(servoFlag);
    debug(" | servoPosedata:" );
    debugln(posedata);

    analogWrite(LINEAR_SERVO_PIN, posedata);

    if (servoFlag == true) {
      posedata++;
    } else {
      posedata--;
    }
    prev_servo_millis = millis();
  }
}

#pragma endregion


#pragma region Dry_STM



// void setGPIO(){
//   // I2C
//   Wire.setSDA(SDA2);
//   Wire.setSCL(SCL2);
//   Wire.begin();
  
//   // relay
//   pinMode(PUMP, OUTPUT);
//   pinMode(BOTTOM_LED, OUTPUT);
//   pinMode(TOP_LED, OUTPUT);
//   pinMode(MAGNET, OUTPUT);
//   pinMode(ARM, OUTPUT);
//   pinMode(CRAWLER, OUTPUT);

//   digitalWrite(PUMP, LOW);
//   digitalWrite(BOTTOM_LED, LOW);
//   digitalWrite(TOP_LED, LOW);
//   digitalWrite(MAGNET, LOW);
//   digitalWrite(ARM, HIGH);
//   digitalWrite(CRAWLER, HIGH);

//   // led
//   pinMode(LED, OUTPUT);
//   digitalWrite(LED, LOW);

//   // servo
//   buffTool.attach(BUFF_TOOL); // grinder
//   buffTool.write(RESET);

//   cam_base.attach(CAM_BASE);   // Camera base
//   cam_mount.attach(CAM_MOUNT); // Camera mount

//   buffTool.write(BUFFING_SPEED_RESET); // grinder
// }

// /*
//    Setup gimbal at every start of the robot.
// */
// void gimbal_setup_motion()
// {
//   cam_base.write(0);
//   cam_mount.write(0);
//   delay(1000);
//   cam_base.write(90);
//   cam_mount.write(90);
// }

// void setup_currentSensor()
// {
//   SSA100.begin();
//   SSA100.setGain(1);

// }

#pragma endregion


void setup()
{
  // initialize LED digital pin as an output.

  #pragma region STM_Only

  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);
  
  Serial.begin(57600);
  SerialL.begin(9600);
  SerialR.begin(9600);
  set_microros_serial_transports(Serial);

  allocator = rcl_get_default_allocator();

  // create init_options
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // create node
  RCCHECK(rclc_node_init_default(&node, "micro_ros_node", "", &support));

  #pragma region Publishers
  // create publisher

  RCCHECK(rclc_publisher_init_default(
    &distL_publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int64),
    "distance_left_publisher"));

  RCCHECK(rclc_publisher_init_default(
    &distR_publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int64),
    "distance_right_publisher"));

  RCCHECK(rclc_publisher_init_default(
    &flow_publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
    "flow_publisher"));

  RCCHECK(rclc_publisher_init_default(
    &wire_publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
    "wire_publisher"));

  // create subscriber
  RCCHECK(rclc_subscription_init_default(
		&motor_subscriber,
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
		"servo_pose"));

  
  // create timer,
  RCCHECK(rclc_timer_init_default(
    &wire_timer,
    &support,
    RCL_MS_TO_NS(WIRE_DELAY),
    wire_callback));

  RCCHECK(rclc_timer_init_default(
    &flow_timer,
    &support,
    RCL_MS_TO_NS(FLOW_DELAY),
    flow_callback));

  #pragma endregion

  // create executor
  RCCHECK(rclc_executor_init(&executor, &support.context, 3, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &motor_subscriber, &pose_msg, &pose_sub, ON_NEW_DATA));
  RCCHECK(rclc_executor_add_timer(&executor, &flow_timer));
  RCCHECK(rclc_executor_add_timer(&executor, &wire_timer));

  if(digitalPinToInterrupt(FLOW_SENSOR_PIN) == NOT_AN_INTERRUPT){
    debug("Pin ");
    debug(FLOW_SENSOR_PIN);
    debugln("can't take interrupt");
  }
  else{
    gpioInit();
  }

  #pragma endregion

  #pragma region Dry_STM

  

  #pragma endregion
}

void loop()
{
  RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));
  controlLinServo(pose);
}
