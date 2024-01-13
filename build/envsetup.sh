#/bin/sh

export TREMO_SDK_PATH=$(pwd)
export TOOLCHAIN_DIR=tools/toolchain/gcc-arm-none-eabi-10.3-2021.10

which arm-none-eabi-gcc >/dev/null 2>&1
if [ $? -ne 0 ]; then
    if [ ! -e $TREMO_SDK_PATH/$TOOLCHAIN_DIR/bin/arm-none-eabi-gcc.exe ]; then
    	cd ./tools/toolchain/
    	unzip gcc-arm-none-eabi-10.3-2021.10-win32.zip
    	cd -
    fi
    
    which arm-none-eabi-gcc >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        export PATH="$PATH:$TREMO_SDK_PATH/$TOOLCHAIN_DIR/bin/"
    fi
fi
