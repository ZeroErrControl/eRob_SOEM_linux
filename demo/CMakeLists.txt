cmake_minimum_required(VERSION 3.8)
project(erob_soem_demo)


project(erob_soem_demo LANGUAGES C CXX)
find_package(Threads REQUIRED)

include_directories(/home/erobman/SOEM_ethercat/install/SOEM/include/soem)
link_directories(/home/erobman/SOEM_ethercat/install/SOEM/lib)



add_executable(eRob_CSV eRob_CSV.cpp)
target_link_libraries(eRob_CSV soem)
install(TARGETS eRob_CSV DESTINATION bin)

add_executable(eRob_CSP_subscriber eRob_CSP_subscriber.cpp)
target_link_libraries(eRob_CSP_subscriber soem)
install(TARGETS eRob_CSP_subscriber DESTINATION bin)

add_executable(eRob_PP_subscriber eRob_PP_subscriber.cpp)
target_link_libraries(eRob_PP_subscriber soem)
install(TARGETS eRob_PP_subscriber DESTINATION bin)

add_executable(eRob_PT eRob_PT.cpp)
target_link_libraries(eRob_PT soem)
install(TARGETS eRob_PT DESTINATION bin)

add_executable(eRob_CST eRob_CST.cpp)
target_link_libraries(eRob_CST soem)
install(TARGETS eRob_CST DESTINATION bin)

add_executable(eRob_CSP eRob_CSP.cpp)
target_link_libraries(eRob_CSP soem)
install(TARGETS eRob_CSP DESTINATION bin)

# add a test program
#add_executable(eRob_TC eRob_TC.cpp)
#target_link_libraries(eRob_TC soem)
#install(TARGETS eRob_TC DESTINATION bin)