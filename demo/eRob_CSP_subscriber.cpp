#include <stdio.h>
#include <string.h>
#include "ethercat.h"
#include <iostream>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>

#include <sys/time.h>
#include <pthread.h>
#include <math.h>

#include <chrono>
#include <ctime>

#include <iostream>
#include <cstdint>

#include <sched.h>

#include <algorithm>  // Add this include
using std::min;
using std::max;

char IOmap[4096];
int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;
int dorun = 0;
bool start_ecatthread_thread;
int ctime_thread;

int64 toff, gl_delta;

OSAL_THREAD_FUNC ecatcheck(void *ptr);
OSAL_THREAD_FUNC ecatthread(void *ptr);

OSAL_THREAD_HANDLE thread1;
OSAL_THREAD_HANDLE thread2;

void ec_sync(int64 reftime, int64 cycletime, int64 *offsettime);
void add_timespec(struct timespec *ts, int64 addtime);

#define stack64k (64 * 1024)
#define NSEC_PER_SEC 1000000000   // 1s
#define EC_TIMEOUTMON 5000        // 5 us

// Conversion units from the servomotor
float Cnt_to_deg = 0.000686645;
int8_t SLAVE_ID;

#define POSITION_BUFFER_SIZE 3
#define POSITION_STABILITY_THRESHOLD 50  // Decrease position stability threshold, improve response speed

// Global variables
int32_t position_buffer[POSITION_BUFFER_SIZE] = {0};
int buffer_index = 0;
bool position_stable = false;

#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

// Declare target_position globally or pass it as a parameter to erob_test()
volatile int target_position = 0;
pthread_mutex_t target_position_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t target_position_cond = PTHREAD_COND_INITIALIZER;
bool target_position_updated = false;

// Server function
void* start_server(void* arg) {
    int port = *(int*)arg;
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return nullptr;
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "Failed to set socket options" << std::endl;
        return nullptr;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Binding failed" << std::endl;
        return nullptr;
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listening failed" << std::endl;
        return nullptr;
    }

    std::cout << "Waiting for connection..." << std::endl;
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        std::cerr << "Failed to accept connection" << std::endl;
        return nullptr;
    }

    // Receive data
    static time_t last_print_time = 0;
    while (true) {
        int valread = read(new_socket, buffer, 1024);
        if (valread > 0) {
            std::cout << "Received data: " << buffer << std::endl;
            try {
                int new_target = std::stoi(buffer);
                pthread_mutex_lock(&target_position_mutex);
                target_position = new_target;
                target_position_updated = true;
                pthread_cond_signal(&target_position_cond);
                pthread_mutex_unlock(&target_position_mutex);
                std::cout << "Updated target position to: " << target_position << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Data conversion error: " << e.what() << std::endl;
            }
            memset(buffer, 0, sizeof(buffer));
        }

        time_t current_time = time(NULL);
        if (current_time - last_print_time >= 5) {  // Print every 5 seconds
            pthread_mutex_lock(&target_position_mutex);
            printf("Server thread: current target_position = %d\n", target_position);
            pthread_mutex_unlock(&target_position_mutex);
            last_print_time = current_time;
        }
    }

    close(new_socket);
    close(server_fd);
    return nullptr;
}
//##################################################################################################
// Function: Set thread CPU affinity
void set_thread_affinity(pthread_t thread, int cpu_core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);

    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        printf("Failed to set CPU %d affinity for thread\n", cpu_core);
    } else {
        printf("Thread successfully bound to CPU %d\n", cpu_core);
    }
}

//##################################################################################################

// 将函数定义移到全局作用域（在erob_test函数之前）
#define SMOOTHING_WINDOW_SIZE 5
int32_t position_smoothing_buffer[SMOOTHING_WINDOW_SIZE] = {0};
int smoothing_index = 0;

int32_t smooth_position(int32_t new_position) {
    position_smoothing_buffer[smoothing_index] = new_position;
    smoothing_index = (smoothing_index + 1) % SMOOTHING_WINDOW_SIZE;
    
    int64_t sum = 0;
    for (int i = 0; i < SMOOTHING_WINDOW_SIZE; i++) {
        sum += position_smoothing_buffer[i];
    }
    return (int32_t)(sum / SMOOTHING_WINDOW_SIZE);
}

int erob_test();

uint16_t data_R;

// Add these definitions at the top with other global variables
#define INTERPOLATION_PERIOD 500    // 500微秒
#define MAX_VELOCITY 80000          // 降低最大速度限制
#define MAX_ACCELERATION 200000     // 降低最大加速度限制
#define VELOCITY_THRESHOLD 1000     // 速度误差阈值
#define MINIMUM_MOVE_DISTANCE 100   // 最小运动距离

// Add these global variables
struct VelocityProfile {
    double current_velocity;
    double target_velocity;
    double acceleration;
    double deceleration;
    double max_jerk;        // 加加速度限制
    bool need_slow_down;    // 是否需要减速标志
};

struct MotionProfile {
    int32_t start_position;
    int32_t target_position;
    int32_t current_position;
    double current_velocity;
    double current_acceleration;
    bool in_motion;
    double max_velocity;        // Current maximum velocity
    double max_acceleration;    // Current maximum acceleration 
    double deceleration_point; // Deceleration point position
    VelocityProfile vel_profile;
    double previous_velocity;
    double velocity_error;
    uint32_t error_count;
};
MotionProfile motion_profile = {0, 0, 0, 0.0, 0.0, false, 0.0, 0.0, 0.0};

// Add motor status monitoring
struct MotorStatus {
    bool is_operational;      // Whether in OP state
    bool is_fault;           // Whether there is a fault
    bool target_reached;     // Whether target position is reached
    uint16_t status_word;    // Status word
    int32_t actual_position; // Actual position
    int32_t actual_velocity; // Actual velocity 
    int32_t position_error;  // Position error
};

MotorStatus motor_status = {false, false, false, 0, 0, 0, 0};

// Update motor status function
void update_motor_status(int slave_id) {
    if (ec_slave[slave_id].state != EC_STATE_OPERATIONAL) {
        motor_status.is_operational = false;
        return;
    }

    motor_status.is_operational = true;
    motor_status.status_word = *(uint16_t*)(ec_slave[slave_id].inputs + 8);
    motor_status.actual_position = *(int32_t*)(ec_slave[slave_id].inputs + 0);
    
    // Parse status word
    motor_status.is_fault = (motor_status.status_word & 0x0008) != 0;
    motor_status.target_reached = (motor_status.status_word & 0x0400) != 0;
    
    // Calculate position error
    motor_status.position_error = motor_status.actual_position - motion_profile.current_position;
}

int32_t generate_position_setpoint(MotionProfile* profile, double dt) {
    if (!profile || !motor_status.is_operational) {
        return motor_status.actual_position;
    }

    // 初始化运动参数
    if (!profile->in_motion) {
        profile->start_position = motor_status.actual_position;
        profile->current_position = profile->start_position;
        profile->vel_profile.current_velocity = 0;
        profile->previous_velocity = 0;
        profile->velocity_error = 0;
        profile->error_count = 0;
        profile->in_motion = true;
        
        // 计算运动距离
        double distance = abs(profile->target_position - profile->start_position);
        
        // 根据距离动态调整运动参数
        if (distance < MINIMUM_MOVE_DISTANCE) {
            return profile->current_position; // 距离太小，不移动
        }
        
        // 根据距离动态调整速度和加速度
        if (distance < 1000) {
            profile->max_velocity = MAX_VELOCITY * 0.1;
            profile->max_acceleration = MAX_ACCELERATION * 0.15;
            profile->vel_profile.max_jerk = profile->max_acceleration * 0.2;
        } else if (distance < 5000) {
            profile->max_velocity = MAX_VELOCITY * 0.2;
            profile->max_acceleration = MAX_ACCELERATION * 0.25;
            profile->vel_profile.max_jerk = profile->max_acceleration * 0.3;
        } else {
            profile->max_velocity = MAX_VELOCITY * 0.5;
            profile->max_acceleration = MAX_ACCELERATION * 0.4;
            profile->vel_profile.max_jerk = profile->max_acceleration * 0.4;
        }
    }

    // 计算运动参数
    double total_distance = profile->target_position - profile->start_position;
    int direction = (total_distance >= 0) ? 1 : -1;
    total_distance = abs(total_distance);

    // 计算当前到目标的距离
    double remaining_distance = abs(profile->target_position - profile->current_position);
    
    // 速度规划
    double target_velocity = profile->max_velocity;
    
    // 根据剩余距离调整速度
    if (remaining_distance < (profile->max_velocity * profile->max_velocity) / (2 * profile->max_acceleration)) {
        // 需要开始减速
        target_velocity = sqrt(2 * profile->max_acceleration * remaining_distance);
    }

    // 限制加加速度（平滑加速度变化）
    double velocity_change = target_velocity - profile->vel_profile.current_velocity;
    velocity_change = std::min(std::max(velocity_change, -profile->vel_profile.max_jerk * dt), 
                              profile->vel_profile.max_jerk * dt);
    
    // 更新当前速度
    profile->vel_profile.current_velocity += velocity_change;
    
    // 计算速度误差
    double velocity_error = abs(profile->vel_profile.current_velocity - profile->previous_velocity);
    if (velocity_error > VELOCITY_THRESHOLD) {
        profile->error_count++;
        // 如果连续出现速度误差，降低速度
        if (profile->error_count > 5) {
            profile->max_velocity *= 0.9;
            profile->error_count = 0;
        }
    } else {
        profile->error_count = 0;
    }
    
    // 更新位置
    profile->current_position += profile->vel_profile.current_velocity * dt * direction;
    
    // 存储当前速度用于下次比较
    profile->previous_velocity = profile->vel_profile.current_velocity;

    // 检查是否到达目标
    if (abs(profile->target_position - profile->current_position) < POSITION_STABILITY_THRESHOLD) {
        profile->current_position = profile->target_position;
        profile->vel_profile.current_velocity = 0;
        profile->in_motion = false;
    }

    return (int32_t)profile->current_position;
}

int erob_test() {
    int rdl;
    SLAVE_ID = 1;
    int i, j, oloop, iloop, chk;
    // 1. Call ec_config_init() to move from INIT to PRE-OP state.
    printf("__________STEP 1___________________\n");
    // Init EtherCAT master
    if (ec_init("enp6s0") <= 0) {
        printf("Error: Could not initialize EtherCAT master!\n");
        printf("No socket connection on Ethernet port. Execute as root.\n");
        printf("___________________________________________\n");
        return -1;
    }
    printf("EtherCAT master initialized successfully.\n");
    printf("___________________________________________\n");

    // Search for EtherCAT slaves on the network
    if (ec_config_init(FALSE) <= 0) {
        printf("Error: Cannot find EtherCAT slaves!\n");
        printf("___________________________________________\n");
        ec_close();
        return -1;
    }
    printf("%d slaves found and configured.\n", ec_slavecount);
    printf("___________________________________________\n");

    //2.- Change to pre operational state to config the PDO registers
    printf("__________STEP 2___________________\n");

    //Check if the slave is ready to map
    ec_readstate();

    for(int i = 1; i<=ec_slavecount ; i++)
    {
        if(ec_slave[i].state != EC_STATE_PRE_OP)
        {
            printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                   i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
            printf("\nRequest init state for slave %d\n", i);
            ec_slave[i].state = EC_STATE_INIT;
            printf("___________________________________________\n");
        }
        else //request operational mode
        {
            ec_slave[0].state = EC_STATE_PRE_OP;
            /* request EC_STATE_PRE_OP state for all slaves */
            ec_writestate(0);
            /* wait for all slaves to reach state */
            if ((ec_statecheck(0, EC_STATE_PRE_OP,  3*EC_TIMEOUTSTATE)) == EC_STATE_PRE_OP)
            {
                printf("State changed to EC_STATE_PRE_OP: %d \n", EC_STATE_PRE_OP );
                printf("___________________________________________\n");
            }else { printf("State EC_STATE_PRE_OP can not changed step 2 to\n"); return -1;}

        }

    }

//##################################################################################################
    //3.- Map RXPOD
    printf("__________STEP 3___________________\n");

    //clear the register 0x1c12, subindex
    int retval = 0;
    uint16 clear_val=  0x0000;
    retval = 0;
    uint16 map_1c12;
    for(int i = 1; i<=ec_slavecount ; i++) {

        retval = ec_SDOwrite(i, 0x1c12, 0x00, FALSE, sizeof(clear_val), &clear_val, EC_TIMEOUTSAFE);
        map_1c12 =  0x1600 ;
        retval += ec_SDOwrite(i, 0x1c12, 0x01, FALSE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);

        map_1c12 =  0x1611 ;  //Profile velocity
        retval += ec_SDOwrite(i, 0x1c12, 0x01+1, FALSE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);

        map_1c12 =  0x1613 ;  //Profile acceleration
        retval += ec_SDOwrite(i, 0x1c12, 0x01+2, FALSE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);

        map_1c12 =  0x1614 ; // Profile deceleration
        retval += ec_SDOwrite(i, 0x1c12, 0x01+3, FALSE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);

        map_1c12 =  0x0001 ;
        retval += ec_SDOwrite(i, 0x1c12, 0x00, FALSE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);
    }
    printf("slave %d set, retval = %d\n", SLAVE_ID, retval);
    if ( retval <1)
    {
        printf("RXPOD Mapping can no set.\n");
        printf("___________________________________________\n");
        return -1;

    }

    printf("RXPOD Mapping set correct1ly.\n");
    printf("___________________________________________\n");

    //........................................................................................
    //Map TXPOD
    retval = 0;
    uint16 map_1c13;
    for(int i = 1; i<=ec_slavecount ; i++) {
        //clear the register 0x1c13, subindex
        clear_val=  0x0000;

        retval += ec_SDOwrite(i, 0x1c13, 0x00, FALSE, sizeof(clear_val), &clear_val, EC_TIMEOUTSAFE);

        map_1c13 = 0x1A00;
        retval += ec_SDOwrite(i, 0x1c13, 0x01, FALSE, sizeof(map_1c13), &map_1c13, EC_TIMEOUTSAFE);

        map_1c13 = 0x0001;
        retval += ec_SDOwrite(i, 0x1c13, 0x00, FALSE, sizeof(map_1c13), &map_1c13, EC_TIMEOUTSAFE);
    }
    printf(" slave %d set, retval = %d\n", SLAVE_ID, retval);

   if ( retval <1  )
   {
       printf("TXPOD Mapping can no set.\n");
       printf("___________________________________________\n");
       return -1;
   }

   printf("TXPOD Mapping set correct1ly.\n");
   printf("___________________________________________\n");

   //.............................................................................................

   //##################################################################################################

    //4.- Set ecx_context.manualstatechange = 1. Map PDOs for all slaves by calling ec_config_map().
   printf("__________STEP 4___________________\n");

   ecx_context.manualstatechange = 1; //Automatic state changed disable
   osal_usleep(1e6);

    uint8 WA = 0;
    uint8 my_RA = 0;
    uint32 TIME_RA;

    // Print the information of the slaves found
    for (int i = 1; i <= ec_slavecount; i++) {
       // (void)ecx_FPWR(ecx_context.port, i, ECT_REG_DCSYNCACT, sizeof(WA), &WA, 5 * EC_TIMEOUTRET);
        printf("Name: %s\n", ec_slave[i].name);
        printf("Slave %d: Type %d, Address 0x%02x, State Machine actual %d, required %d\n", i, ec_slave[i].eep_id, ec_slave[i].configadr, ec_slave[i].state, EC_STATE_INIT);
        printf("___________________________________________\n");
        ecx_dcsync0(&ecx_context, i, TRUE, 2000000, 0);  ///!!!!!!!  DC
    }

    // Set the IOMap
    ec_config_map(&IOmap);

    printf("__________STEP 5___________________\n");

     
    ec_slave[0].state = EC_STATE_SAFE_OP;
    ec_writestate(0);
    if ((ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 5)) == EC_STATE_SAFE_OP) {
        printf("State changed to EC_STATE_SAFE_OP: %d\n", EC_STATE_SAFE_OP);
        printf("___________________________________________\n");
    } else {
        printf("State could not be changed to EC_STATE_SAFE_OP\n");
        return -1;
    }
    // Call ec_configdc() in EC_STATE_SAFE_OP state
    ec_configdc();   // 9%
    expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
    printf("Calculated workcounter %d\n", expectedWKC);
    printf("___________________________________________\n");

    ec_readstate();
    for (int cnt = 1; cnt <= ec_slavecount; cnt++) {
        printf("Slave %d\nName: %s\nOutput size: %3d bits\nInput size: %3d bits\nState: %2d\ndelay: %d.%d\n", cnt, ec_slave[cnt].name, ec_slave[cnt].Obits, ec_slave[cnt].Ibits, ec_slave[cnt].state, (int)ec_slave[cnt].pdelay, ec_slave[cnt].hasdc);
        printf("Out: %p, %4d\nIn: %p, %4d\n", ec_slave[cnt].outputs, ec_slave[cnt].Obytes, ec_slave[cnt].inputs, ec_slave[cnt].Ibytes);
    }
    printf("___________________________________________\n");

    // 6. Start the regular PDO transfer at the proper interval
    printf("__________STEP 6___________________\n");

    start_ecatthread_thread = TRUE;
    osal_thread_create_rt(&thread1, stack64k * 2, (void *)&ecatthread, (void *)&ctime_thread);

    //set_thread_affinity(*thread1, 1);
    osal_thread_create(&thread2, stack64k * 2, (void *)&ecatcheck, NULL);

   //set_thread_affinity(*thread2,1);
    printf("___________________________________________\n");
    my_RA = 0;
    for (int cnt = 1; cnt <= ec_slavecount; cnt++) {
        ec_SDOread(cnt, 0x1c32, 0x01, FALSE, &rdl, &data_R, EC_TIMEOUTSAFE);
        printf("Slave %d DC synchronized 0x1c32: %d\n", cnt, data_R);
    }


    // 8. Transition to OP state
    printf("__________STEP 8___________________\n");

    ec_send_processdata();
    wkc = ec_receive_processdata(EC_TIMEOUTRET);


    ec_slave[0].state = EC_STATE_OPERATIONAL;
    ec_writestate(0);


    if ((ec_statecheck(0, EC_STATE_OPERATIONAL, 5 * EC_TIMEOUTSTATE)) == EC_STATE_OPERATIONAL) {
        printf("State changed to EC_STATE_OPERATIONAL: %d\n", EC_STATE_OPERATIONAL);
        printf("___________________________________________\n");
    } else {
        printf("State could not be changed to EC_STATE_OPERATIONAL\n");
        for (int cnt = 1; cnt <= ec_slavecount; cnt++) {
            printf("ALstatuscode: %d\n", ecx_context.slavelist[cnt].ALstatuscode);
        }
    }

    ec_readstate();
    for (int i = 1; i <= ec_slavecount; i++) {
        printf("Slave %d: Type %d, Address 0x%02x, State Machine actual %d, required %d\n", i, ec_slave[i].eep_id, ec_slave[i].configadr, ec_slave[i].state, EC_STATE_OPERATIONAL);
        printf("Name: %s\n", ec_slave[i].name);
        printf("___________________________________________\n");
    }

    // 9. Configure servomotor and mode operation
    printf("__________STEP 9___________________\n");

    if (ec_slave[0].state == EC_STATE_OPERATIONAL) {
        printf("######################################################################################\n");
        printf("################# All slaves entered into OP state! ##################################\n");
        // Profile velocity

        uint16_t  Control_Word;
        Control_Word = 128;
        

        uint32_t  Profile_velocity;
        Profile_velocity = 50000;

        uint32_t  Profile_acceleration;
        Profile_acceleration = 150000;

        uint32_t  Profile_deceleration;
        Profile_deceleration = 150000;
        uint8    operation_mode = 8;


        for (int i = 1; i <= ec_slavecount; i++) {
            ec_SDOwrite(i, 0x6040, 0x00, FALSE, sizeof(Control_Word), &Control_Word, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6060, 0x00, FALSE, sizeof(operation_mode), &operation_mode, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6081, 0x00, FALSE, sizeof(Profile_velocity), &Profile_velocity, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6083, 0x00, FALSE, sizeof(Profile_acceleration), &Profile_acceleration, EC_TIMEOUTSAFE);
            ec_SDOwrite(i, 0x6084, 0x00, FALSE, sizeof(Profile_deceleration), &Profile_deceleration, EC_TIMEOUTSAFE);

        }
        // Record start time

        auto start = std::chrono::high_resolution_clock::now();
        //osal_usleep(1000000);
        int step = 0;
        uint16_t pdo_status_word;
        uint16_t tenth_bit = 0;
        uint16_t previous_tenth_bit = 1;  // Add this line to track the previous state
        int32_t previous_position;
        int turn_flag = 0;
        int32_t position_actual_value;
        int32_t next_position=0;

        while(1)
        {
            // Add status check
            static int error_count = 0;
            static time_t last_error_time = 0;
            
            if (wkc < expectedWKC) {
                error_count++;
                time_t current_time = time(NULL);
                
                if (error_count > 10 && (current_time - last_error_time) > 5) {
                    printf("Warning: Multiple WKC errors detected, attempting recovery...\n");
                    
                    // Attempt to recover communication
                    ec_recover_slave(SLAVE_ID, EC_TIMEOUTMON);
                    
                    error_count = 0;
                    last_error_time = current_time;
                }
            } else {
                error_count = 0;
            }

            // Check slave status
            if (ec_slave[SLAVE_ID].state != EC_STATE_OPERATIONAL) {
                printf("Warning: Slave not in OPERATIONAL state, attempting to restore...\n");
                ec_slave[SLAVE_ID].state = EC_STATE_OPERATIONAL;
                ec_writestate(SLAVE_ID);
                
                // Wait for status recovery
                int retry = 0;
                while (retry < 3 && ec_slave[SLAVE_ID].state != EC_STATE_OPERATIONAL) {
                    osal_usleep(1000); // 100ms
                    ec_statecheck(SLAVE_ID, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
                    retry++;
                }
            }

            ec_send_processdata();
            wkc = ec_receive_processdata(EC_TIMEOUTRET);


    if (wkc >= expectedWKC) { 
        printf("Processdata cycle %4d, WKC %d , O:", i, wkc);

        // Display input data
        printf(" I:");


            // Read actual position value (4 bytes)
            position_actual_value = *(int32_t *)(ec_slave[0].inputs + 0); // Assuming position actual value is at the start of input data
            printf(" Position Actual Value: %d", position_actual_value);


/*

            uint16_t sdo_status_word;
            int size = sizeof(sdo_status_word);
            ec_SDOread(1, 0x6041, 0x00, FALSE, &size, &sdo_status_word, EC_TIMEOUTSAFE);
            printf("Status Word (SDO): 0x%04X\n", sdo_status_word);

*/

            // Get via PDO
            pdo_status_word = *(uint16_t *)(ec_slave[0].inputs + 8);
            //printf("Status Word (PDO): 0x%04X\n", pdo_status_word);

            tenth_bit = (pdo_status_word >> 10) & 1; // Extract the tenth bit
            printf(" tenth_bit: %d\n", tenth_bit); // Print the tenth bit value

        needlf = TRUE;
    }
            
                uint16 output_data[5] = {0};
            
/*
                                // 新增PDO数据
                output_data[5] = 30000 & 0xFFFF;                    // Profile velocity低16位
                output_data[6] = (30000 >> 16) & 0xFFFF;           // Profile velocity高16位
                output_data[7] = 10000 & 0xFFFF;                    // Profile acceleration低16位
                output_data[8] = (10000 >> 16) & 0xFFFF;           // Profile acceleration高16位
                output_data[9] = 10000 & 0xFFFF;                    // Profile deceleration低16位
                output_data[10] = (10000 >> 16) & 0xFFFF;  
*/

            if (step > 800 && target_position != 0) {
                update_motor_status(SLAVE_ID);
                
                pthread_mutex_lock(&target_position_mutex);
                while (!target_position_updated) {
                    pthread_cond_wait(&target_position_cond, &target_position_mutex);
                }
                
                motion_profile.target_position = target_position;
                target_position_updated = false;
                pthread_mutex_unlock(&target_position_mutex);

                if (motor_status.is_operational) {
                    // 生成下一位置设定点
                    next_position = generate_position_setpoint(&motion_profile, INTERPOLATION_PERIOD / 1000000.0);
                    next_position = smooth_position(next_position);  // 应用平滑
                    
                    // 更新输出数据
                    output_data[0] = next_position & 0xFFFF;
                    output_data[1] = (next_position >> 16) & 0xFFFF;
                    output_data[2] = 0x00;
                    output_data[3] = 0x00;
                    output_data[4] = 0x1F;  // Control word for normal operation
                }
            }
            else if (step>800 && target_position == 0)
            {
                output_data[0] = (position_actual_value) & 0xFFFF;          //
                output_data[1] = (position_actual_value >> 16) & 0xFFFF; //
                output_data[2] = 0x00;          //
                output_data[3] = 0x00; //
                output_data[4] = 0x0F;     

            }

            if (step > 600 && step <=800)
            {

                output_data[0] = position_actual_value & 0xFFFF;          //
                output_data[1] = (position_actual_value >> 16) & 0xFFFF; //
                output_data[2] = 0x00;          //
                output_data[3] = 0x00; //
                output_data[4] = 0x0F;          //
                //output_data[5] = 0x0F; //
                //step = 1;
            }
            if (step <= 600 && step > 400)
            {


                output_data[0] = position_actual_value & 0xFFFF;          //
                output_data[1] = (position_actual_value >> 16) & 0xFFFF; //
                output_data[2] = 0x00;          //
                output_data[3] = 0x00; //
                output_data[4] = 0x07;      
               //memcpy(ec_slave[0].outputs, output_data, sizeof(output_data)); //
                //step = 3;

            }
            if (step <= 400 && step >200 )
            {


                output_data[0] = position_actual_value & 0xFFFF;          //
                output_data[1] = (position_actual_value >> 16) & 0xFFFF; //
                output_data[2] = 0x00;          //
                output_data[3] = 0x00; //
                output_data[4] = 0x06;      
               // memcpy(ec_slave[0].outputs, output_data, sizeof(output_data)); //
                //step = 2;
            }
            if (step <= 200)
            {

                output_data[0] = position_actual_value & 0xFFFF;          //
                output_data[1] = (position_actual_value >> 16) & 0xFFFF; //
                output_data[2] = 0x00;          //
                output_data[3] = 0x00; //
                output_data[4] = 0x80;      
                //memcpy(ec_slave[0].outputs, output_data, sizeof(output_data)); //
            }

            for (int i = 1; i <= ec_slavecount; i++)
            {
            memcpy(ec_slave[i].outputs, output_data, sizeof(output_data)); //
            }
            if(step <900) {
                step += 1;/* code */
            }

            osal_usleep(100);
            // Record end time

            auto end = std::chrono::high_resolution_clock::now();

            // Calculate the duration in different units
            //auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            auto duration_s = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
            auto duration_m = std::chrono::duration_cast<std::chrono::minutes>(end - start).count();
            auto duration_h = std::chrono::duration_cast<std::chrono::hours>(end - start).count();
            // Output the runtime
            // Output the runtime in the correct format, keeping the columns intact
            std::cout << "Program runtime: " << duration_h << "h:" << duration_m % 60 << "m:"
                      << duration_s % 60 << "s:" << duration_ms % 1000 << "ms"<< duration_us%1000<<"us"<<std::flush;
            //ec_send_processdata();
            //wkc = ec_receive_processdata(EC_TIMEOUTRET);


        }

        osal_usleep(2*1e6);

        ec_close();

         printf("\nRequest init state for all slaves\n");
         ec_slave[0].state = EC_STATE_INIT;
         /* request INIT state for all slaves */
         ec_writestate(0);

        printf("EtherCAT master closed.\n");
    } else {
        printf("Not all slaves reached operational state.\n");

        ec_readstate();
        for (int i = 1; i <= ec_slavecount; i++) {
            if (ec_slave[i].state != EC_STATE_OPERATIONAL) {
                printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n", i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                printf("Requesting INIT state for slave %d\n", i);
                ec_slave[i].state = EC_STATE_INIT;
                printf("___________________________________________\n");
            }
        }
    }

    if (i % 1000 == 0) {  // print once every 1000 cycles
        pthread_mutex_lock(&target_position_mutex);
        printf("eRob thread: current target_position = %d\n", target_position);
        pthread_mutex_unlock(&target_position_mutex);
    }

    return 0;
}

/* PI calculation to get Linux time synced to DC time */
void ec_sync(int64 reftime, int64 cycletime, int64 *offsettime) {
    static int64 integral = 0;
    int64 delta;
    delta = (reftime) % cycletime;
    if (delta > (cycletime / 2)) {
        delta = delta - cycletime;
    }
    if (delta > 0) {
        integral++;
    }
    if (delta < 0) {
        integral--;
    }
    *offsettime = -(delta / 100) - (integral / 20);
    gl_delta = delta;
}

/* Add ns to timespec */
void add_timespec(struct timespec *ts, int64 addtime) {
    int64 sec, nsec;

    nsec = addtime % NSEC_PER_SEC;
    sec = (addtime - nsec) / NSEC_PER_SEC;
    ts->tv_sec += sec;
    ts->tv_nsec += nsec;
    if (ts->tv_nsec >= NSEC_PER_SEC) {
        nsec = ts->tv_nsec % NSEC_PER_SEC;
        ts->tv_sec += (ts->tv_nsec - nsec) / NSEC_PER_SEC;
        ts->tv_nsec = nsec;
    }
}

OSAL_THREAD_FUNC ecatcheck(void *ptr) {
    int slave;
    (void)ptr;

    while (1) {
        if (inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate)) {
            // Add timeout mechanism
            struct timespec timeout;
            clock_gettime(CLOCK_MONOTONIC, &timeout);
            timeout.tv_sec += 1; // 1 second timeout

            if (needlf) {
                needlf = FALSE;
                printf("\n");
            }
            
            ec_group[currentgroup].docheckstate = FALSE;
            
            // Use timeout version of state reading
            int result = ec_readstate();
            if (result <= 0) {
                printf("Warning: ec_readstate failed, continuing...\n");
                continue;
            }

            for (slave = 1; slave <= ec_slavecount; slave++) {
                if ((ec_slave[slave].group == currentgroup) && 
                    (ec_slave[slave].state != EC_STATE_OPERATIONAL)) {
                    
                    ec_group[currentgroup].docheckstate = TRUE;
                    
                    // Add timeout check
                    struct timespec current_time;
                    clock_gettime(CLOCK_MONOTONIC, &current_time);
                    if (current_time.tv_sec >= timeout.tv_sec) {
                        printf("Warning: State check timeout for slave %d\n", slave);
                        continue;
                    }

                    // State recovery handling
                    if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
                        printf("ERROR: Slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                        ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                        ec_writestate(slave);
                    } else if (ec_slave[slave].state == EC_STATE_SAFE_OP) {
                        printf("WARNING: Slave %d is in SAFE_OP, changing to OPERATIONAL.\n", slave);
                        ec_slave[slave].state = EC_STATE_OPERATIONAL;
                        ec_writestate(slave);
                    }
                }
            }
        }
        
        // Add short sleep to avoid excessive CPU usage
        osal_usleep(2000000); // 10ms
    }
}

/* RT EtherCAT thread */
OSAL_THREAD_FUNC_RT ecatthread(void *ptr) {
    struct timespec ts, tleft;
    int ht;
    int64 cycletime;
    struct timeval tp;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ht = (ts.tv_nsec / 1000000) + 1; /* round to nearest ms */
    ts.tv_nsec = ht * 1000000;
    if (ts.tv_nsec >= NSEC_PER_SEC) {
        ts.tv_sec++;
        ts.tv_nsec -= NSEC_PER_SEC;
    }
    cycletime = *(int *)ptr * 1000; /* cycletime in ns */

    toff = 0;
    dorun = 0;
    ec_send_processdata();
    while (1) {
        dorun++;
        /* calculate next cycle start */
        add_timespec(&ts, cycletime + toff);
        /* wait to cycle start */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, &tleft);

        if (start_ecatthread_thread) {
            wkc = ec_receive_processdata(EC_TIMEOUTRET);

            if (ec_slave[0].hasdc) {
                /* calculate toff to get Linux time and DC synced */
                ec_sync(ec_DCtime, cycletime, &toff);
            }

            ec_send_processdata();
        }
    }
}

// 
void* erob_thread_function(void* arg) {
    erob_test(); // 
    return nullptr;
}



int main(int argc, char **argv) {
    needlf = FALSE;
    inOP = FALSE;
    start_ecatthread_thread = FALSE;
    dorun = 0;
    ctime_thread = 2000; // 1ms cycle time

    int port = 8080;
    
    // Declare all thread-related variables
    pthread_t server_thread, erob_thread;
    int server_ret, erob_ret;

    // Create server thread
    server_ret = pthread_create(&server_thread, nullptr, start_server, &port);
    if (server_ret != 0) {
        std::cerr << "Failed to create server thread: " << strerror(server_ret) << std::endl;
        return -1;
    }
    printf("Server thread created\n");

    // Give server some time to initialize
    sleep(1);

    // Create erob thread
    erob_ret = pthread_create(&erob_thread, nullptr, erob_thread_function, nullptr);
    if (erob_ret != 0) {
        std::cerr << "Failed to create eRob thread: " << strerror(erob_ret) << std::endl;
        return -1;
        
    }
    printf("eRob thread created\n");

    // Set thread affinity
    set_thread_affinity(server_thread, 3);
    set_thread_affinity(erob_thread, 2);

    // Wait for threads to complete
    pthread_join(server_thread, nullptr);
    pthread_join(erob_thread, nullptr);

    printf("End program\n");
    return EXIT_SUCCESS;
}