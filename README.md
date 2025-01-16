***This is an open-source demo related to the eRob product, provided solely for reference by developers. Please note that issues within the open-source project are independent of the quality of eRob products. Users are advised to exercise caution while using the demo. We are not responsible for any damage caused by improper operations. For any project errors, please raise a query in the Issues section. Collaboration and forks to resolve open-source project issues are welcome.***

## Recommendations for EtherCAT Open-Source Master Users  

1. **Use a Real-Time Kernel System**  
   Ensure your operating system has a real-time kernel to guarantee consistent and precise communication.

2. **Isolate CPU Cores**  
   Perform CPU isolation to dedicate specific cores to EtherCAT processes, reducing interruptions and improving stability.

3. **Troubleshooting OP State Issues**  
   - Failure to enter OP state may be caused by errors in the **object dictionary mapping** or improper configuration of **DC (Distributed Clock) mode**.  
   - **eRob** only supports **DC mode**, and proper configuration of DC mode is crucial for system synchronization and precision.

4. **Read Mode-Specific Instructions**  
   Before using each mode, read the relevant operational instructions to ensure correct configuration and usage.

5. **Use the Official eRob Upper Computer Software**  
   eRob provides official upper computer software. Mastering the built-in **oscilloscope tool** will allow you to quickly locate issues with the EtherCAT master.

6. **Capture and Analyze EtherCAT Data**  
   Use packet capture tools to analyze EtherCAT output and log information to identify errors.


## Installation

1. install eRob-SOEM-Linux
``` bash
git clone https://github.com/ZeroErrControl/eRob_SOEM_linux.git
cd eRob_SOEM_linux
mkdir build
cd build
cmake ..
make

```

## Usage
### Running demo:

1. CSV mode:
```bash
sudo ./build/demo/eRob_CSV
```

2. Launch the position subscriber (PP):
```bash
sudo ./build/demo/eRob_PP_subscriber
python3 src/erob_ros/src/eCoder_fake.py
```

3. Launch the position subscriber (CSP):
```bash
sudo ./build/demo/eRob_CSP_subscriber
python3 src/erob_ros/src/eCoder_fake.py
``` 

4. Launch the cyclic synchronous position mode (CSP):
```bash
sudo ./build/demo/eRob_CSP
``` 

5. Launch the profile torque mode (PT):
In this mode, we can control the torque of the servo motor and have added PDO mapping to obtain the position, speed, torque, and status word of the servo motor. 

```bash
sudo ./build/demo/eRob_PT
``` 
If you want to consult the object dictionary, you can run the following command and then run `sudo ./build/test/linux/slaveinfo <ethercat_device> -map` to view the object dictionary.


6. Launch the cyclic synchronous torque mode (CST):
In this mode, we can control the torque of the servo motor and have added PDO mapping to obtain the position, speed, torque, and status word of the servo motor. 

```bash
sudo ./build/demo/eRob_CST
``` 
If you want to consult the object dictionary, you can run the following command and then run `sudo ./build/test/linux/slaveinfo <ethercat_device> -map` to view the object dictionary.