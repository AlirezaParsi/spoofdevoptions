#include <jni.h>
#include <string>
#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "SpoofModule"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static jstring (*orig_SystemProperties_get)(JNIEnv*, jclass, jstring, jstring);
static jint (*orig_Settings_getInt)(JNIEnv*, jclass, jobject, jstring, jint);

static jstring spoof_SystemProperties_get(JNIEnv* env, jclass clazz, jstring key, jstring def) {
    const char* keyStr = env->GetStringUTFChars(key, nullptr);
    std::string keyCpp(keyStr);
    env->ReleaseStringUTFChars(key, keyStr);

    if (keyCpp == "sys.usb.state" ||
        keyCpp == "sys.usb.config" ||
        keyCpp == "persist.sys.usb.reboot.func" ||
        keyCpp == "init.svc.adbd" ||
        keyCpp == "sys.usb.ffs.ready") {
        return env->NewStringUTF("0"); // یا "mtp"
    }
    return orig_SystemProperties_get(env, clazz, key, def);
}

static jint spoof_Settings_getInt(JNIEnv* env, jclass clazz, jobject cr, jstring name, jint def) {
    const char* keyStr = env->GetStringUTFChars(name, nullptr);
    std::string keyCpp(keyStr);
    env->ReleaseStringUTFChars(name, keyStr);

    if (keyCpp == "development_settings_enabled" ||
        keyCpp == "adb_enabled" ||
        keyCpp == "adb_wifi_enabled") {
        return 0; // همیشه خاموش
    }
    return orig_Settings_getInt(env, clazz, cr, name, def);
}

class SpoofModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* pkg = args->nice_name ? env->GetStringUTFChars(args->nice_name, nullptr) : "";
        if (pkg && std::string(pkg) == "com.tosan.dara.sepah") {
            LOGD("Target app detected: %s", pkg);

            JNINativeMethod methods1[] = {
                {"get", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void*) spoof_SystemProperties_get},
            };
            api->hookJniNativeMethods(env, "android/os/SystemProperties", methods1, 1);
            *(void**)&orig_SystemProperties_get = methods1[0].fnPtr;

            JNINativeMethod methods2[] = {
                {"getInt", "(Landroid/content/ContentResolver;Ljava/lang/String;I)I", (void*) spoof_Settings_getInt},
            };
            api->hookJniNativeMethods(env, "android/provider/Settings$Global", methods2, 1);
            *(void**)&orig_Settings_getInt = methods2[0].fnPtr;

            api->hookJniNativeMethods(env, "android/provider/Settings$Secure", methods2, 1);
        }
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(SpoofModule)
