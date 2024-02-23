# Sync bug trigger

## Configure microROS transport
Before building the microROS transport needs to be changed from "udp" to "custom". This is done in the file `components/micro_ros_espidf_component/colcon.meta`.

Edit the file and change the value for RMW_UXRCE_TRANSPORT to "custom".
