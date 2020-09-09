//
// Created by nachi on 8/31/20.
//

#include "jni_interface.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>


#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
      Java_umich_argraphics_JniInterface_##method_name

extern "C" {

}  // extern "C"
