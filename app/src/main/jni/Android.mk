# 设置当前模块的本地路径
LOCAL_PATH := $(call my-dir)

# 清除之前的变量 (预编译库部分)
include $(CLEAR_VARS)
LOCAL_MODULE := dobby  # 去掉lib前缀
LOCAL_SRC_FILES := libraries/$(TARGET_ARCH_ABI)/libdobby.a
include $(PREBUILT_STATIC_LIBRARY)

# 清除变量 (主模块部分)
include $(CLEAR_VARS)
LOCAL_MODULE := Dumper

# 添加头文件路径 (修正正斜杠)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/BNM/include \
                $(LOCAL_PATH)/BNM/external/include \
                $(LOCAL_PATH)/BNM/external \
                $(LOCAL_PATH)/BNM/external/utf8 \
                $(LOCAL_PATH)/BNM/src/BNM_data
                

# 链接预编译库 (使用模块名dobby)
LOCAL_STATIC_LIBRARIES := dobby

# 添加源文件
LOCAL_SRC_FILES := BNM/src/Class.cpp \
    BNM/src/ClassesManagement.cpp \
    BNM/src/Coroutine.cpp \
    BNM/src/Delegates.cpp \
    BNM/src/Defaults.cpp \
    BNM/src/EventBase.cpp \
    BNM/src/Exceptions.cpp \
    BNM/src/FieldBase.cpp \
    BNM/src/Hooks.cpp \
    BNM/src/Image.cpp \
    BNM/src/Internals.cpp \
    BNM/src/Loading.cpp \
    BNM/src/MethodBase.cpp \
    BNM/src/MonoStructures.cpp \
    BNM/src/PropertyBase.cpp \
    BNM/src/UnityStructures.cpp \
    BNM/src/Utils.cpp \
    Main.cpp

# 指定C++标准
LOCAL_CPPFLAGS := -std=c++20 -fexceptions
LOCAL_LDLIBS := -llog

# LOCAL_LDLIBS := -llog -L$(LOCAL_PATH) -lshadowhook
# 构建共享库
include $(BUILD_SHARED_LIBRARY)
