
#build example

cmake -DCMAKE_TOOLCHAIN_FILE=../arm_linux_setup.cmake -DCMAKE_INSTALL_PREFIX=./build ..

cmake -DCMAKE_TOOLCHAIN_FILE=../arm_linux_setup.cmake -DCMAKE_INSTALL_PREFIX=/home/wenyi/workspace/link_model/build ..


cmake -DCMAKE_TOOLCHAIN_FILE=../arm_linux_setup.cmake ..
