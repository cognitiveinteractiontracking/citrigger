#include <ros/ros.h>
#include <rosgraph_msgs/Clock.h>
#include "boost/date_time/posix_time/posix_time.hpp"
#include <dynamic_reconfigure/server.h>
#include <cam_trigger/cam_triggerConfig.h>
#include <unistd.h>
#include <pigpiod_if2.h>

//#include <time.h>
//#include <signal.h>

//-----------Ros Stuff----------------------------
//Publisher
ros::Publisher clock_trigger_pub;
//Clock Message
rosgraph_msgs::Clock trigger_time;
//Ros Timer for Interrupt
ros::Timer timer;


//-----------Variable Definition------------------
//Raspberry Pi ID
int pi;
//Set Timer Freuqency
int freq = 30;
//Set Impulse width in %
double width = 5;
//Impulse width in s
double width_s;
//Enabled/Disabled Cameras
bool cam[4] = {true, true, true, true};
//System Disable/Enable
bool system_stat = false;


//-----------GPIO Definition-----------------------
//GPIO 26 --> Pin 37
int output_pin_1 = 26;
//GPIO 19 --> Pin 35
int output_pin_2 = 19;
//GPIO 13 --> Pin 33
int output_pin_3 = 13;
//GPIO 6 --> Pin 31
int output_pin_4 = 6;

//Bitmask for GPIO write
uint32_t BitMask = (1 << output_pin_1) | (1 << output_pin_2) | (1 << output_pin_3) | (1 << output_pin_4);


//-----------Timer Interrupt Routine---------------
void alarmWakeup(const ros::TimerEvent&) {
	//Set Bitmask to set the GPIOs to high 
	set_bank_1(pi, BitMask);
	
	//Publish the trigger time
	ros::Time t1 = ros::Time::now();
	trigger_time.clock = t1;
	clock_trigger_pub.publish(trigger_time);
	
	//Conversation to date and time
	//boost::posix_time::ptime my_posix_time = t1.toBoost();
	//std::string iso_time_str = boost::posix_time::to_iso_extended_string(my_posix_time);
	//ROS_INFO_STREAM(iso_time_str);
	
	//Calculate publishing time in s
	double t_publish = (((double)(ros::Time::now().sec) + (double)(ros::Time::now().nsec) / 1000000000.0) - ((double)(t1.sec) + (double)(t1.nsec) / 1000000000.0 ));
	
	//Wait to generate a pulsewidth (substract the time to publish the clock message)
	ros::Duration(width_s - t_publish).sleep();
	
	//Set Bitmask to set the GPIOs to low
	clear_bank_1(pi, BitMask);
	
	//ROS_INFO("Timer abgelaufen");
}


//-----------Reconfigure Callback-------------------
void callback(cam_trigger::cam_triggerConfig &config, uint32_t level) {

	//cancel the actual alarm and gpios
	timer.stop();
	clear_bank_1(pi, UINT32_MAX);	
	
	//Display new Parameter
	ROS_INFO("Parameter: Frequenz: %f Hz  -  Pulsweite: %f %%  -  System AN: %s  - Cam1 AN: %s  - Cam2 AN: %s  - Cam3 AN: %s  - Cam4 AN: %s", 
			config.Frequenz, config.Pulsweite, 
			config.System?"True":"False", 
			config.cam1?"True":"False", 
			config.cam2?"True":"False", 
			config.cam3?"True":"False",
			config.cam3?"True":"False");
	
	//set new parameters
	freq = config.Frequenz;
	width = config.Pulsweite;
	system_stat = config.System;
	cam[0] = config.cam1;
	cam[1] = config.cam2;
	cam[2] = config.cam3;
	cam[3] = config.cam4;
	
	//Set the new Bitmask for gpio trigger
	BitMask = (cam[0] << output_pin_1) | (cam[1] << output_pin_2) | (cam[2] << output_pin_3) | (cam[3] << output_pin_4);
	
	//Start alarm new
	if(system_stat == true) {
		double timer_periode = 1.0 / freq;
		width_s = timer_periode * (width/100.0);		
		ROS_INFO("Timer Periode: %f s -  Pulswidth of signal: %f s\n", timer_periode, width_s);
		
		//Set new Timer
		timer.setPeriod(ros::Duration(timer_periode));
		timer.start();
	}
}

//-----------MAIN----------------------------------
int main(int argc, char **argv) {
	//Init ROS
	ros::init(argc, argv, "cam_trigger");
	
	//Init Node
	ros::NodeHandle Cam_Trigger;
	
	//Init Clock Publisher
	clock_trigger_pub = Cam_Trigger.advertise<rosgraph_msgs::Clock>("clock_trigger_pub", 1000);
	
	//Init dynamic reconfigure
	dynamic_reconfigure::Server<cam_trigger::cam_triggerConfig> server;
	dynamic_reconfigure::Server<cam_trigger::cam_triggerConfig>::CallbackType f;
	f = boost::bind(&callback, _1, _2);
	server.setCallback(f);
	
	//Init Timer with 30Hz
	timer = Cam_Trigger.createTimer(ros::Duration(1/30), alarmWakeup); 
	timer.stop();

	//Initialize GPIOs
	int pi = pigpio_start(NULL, "8888");
	ROS_INFO("PI Init: %d\n", pi);

	if(pi==0) {
	  ROS_INFO("PI Initialisation succesfull\n");
	} else {
	  ROS_INFO("PI Initialisation failed\nTry to start GPIOs with following command: sudo pigpiod\n\n");
	  return 1;
	}

	//Set Mode of the GPIOs
	set_mode(pi,output_pin_1, PI_OUTPUT);
	set_mode(pi,output_pin_2, PI_OUTPUT);
	set_mode(pi,output_pin_3, PI_OUTPUT);
	set_mode(pi,output_pin_4, PI_OUTPUT);
	//Set all GPIOs to Zero
	clear_bank_1(pi, UINT32_MAX);

	//Start Trigger
	ROS_INFO("Start Trigger");
	ros::spin();
	return 0;
}
