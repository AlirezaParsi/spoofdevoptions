#include <jni.h>
#include <string>
#include <android-base/properties.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class HideDevOptions : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        // فقط برای پکیج مورد نظر ما فعال شود
        if (args->nice_name && env->GetStringUTFChars(args->nice_name, nullptr) {
            std::string package_name = env->GetStringUTFChars(args->nice_name, nullptr);
            if (package_name == "com.tosan.dara.sepah") {
                target_package = true;
            }
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (!target_package) return;

        // Hook توابع مربوط به تنظیمات توسعه‌دهنده
        hookSettings();
    }

private:
    bool target_package = false;
    Api *api;
    JNIEnv *env;

    void hookSettings() {
        // Hook کردن SystemProperties.get
        auto system_properties_class = env->FindClass("android/os/SystemProperties");
        if (system_properties_class) {
            JNINativeMethod methods[] = {
                {"get", "(Ljava/lang/String;)Ljava/lang/String;", (void*) hookedGet},
                {"get", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void*) hookedGetWithDefault},
                {"getBoolean", "(Ljava/lang/String;Z)Z", (void*) hookedGetBoolean}
            };
            api->hookJniNativeMethods(env, "android/os/SystemProperties", methods, 3);
        }

        // Hook کردن Settings.Global.getInt
        auto settings_global_class = env->FindClass("android/provider/Settings$Global");
        if (settings_global_class) {
            JNINativeMethod methods[] = {
                {"getInt", "(Landroid/content/ContentResolver;Ljava/lang/String;I)I", (void*) hookedGetInt},
                {"getInt", "(Landroid/content/ContentResolver;Ljava/lang/String;)I", (void*) hookedGetIntNoDefault}
            };
            api->hookJniNativeMethods(env, "android/provider/Settings$Global", methods, 2);
        }
    }

    // توابع hook شده
    static jstring hookedGet(JNIEnv *env, jobject, jstring key) {
        const char* key_str = env->GetStringUTFChars(key, nullptr);
        if (strcmp(key_str, "sys.usb.config") == 0 || 
            strcmp(key_str, "sys.usb.state") == 0 ||
            strcmp(key_str, "init.svc.adbd") == 0) {
            return env->NewStringUTF("mtp");
        }
        return env->NewStringUTF(android::base::GetProperty(key_str, "").c_str());
    }

    static jstring hookedGetWithDefault(JNIEnv *env, jobject, jstring key, jstring def) {
        const char* key_str = env->GetStringUTFChars(key, nullptr);
        if (strcmp(key_str, "sys.usb.config") == 0 || 
            strcmp(key_str, "sys.usb.state") == 0 ||
            strcmp(key_str, "init.svc.adbd") == 0) {
            return env->NewStringUTF("mtp");
        }
        return def;
    }

    static jboolean hookedGetBoolean(JNIEnv *env, jobject, jstring key, jboolean def) {
        const char* key_str = env->GetStringUTFChars(key, nullptr);
        if (strcmp(key_str, "persist.sys.usb.config") == 0) {
            return false;
        }
        return def;
    }

    static jint hookedGetInt(JNIEnv *env, jobject, jobject resolver, jstring key, jint def) {
        const char* key_str = env->GetStringUTFChars(key, nullptr);
        if (strcmp(key_str, "adb_enabled") == 0 || 
            strcmp(key_str, "development_settings_enabled") == 0 ||
            strcmp(key_str, "adb_wifi_enabled") == 0) {
            return 0;
        }
        return def;
    }

    static jint hookedGetIntNoDefault(JNIEnv *env, jobject, jobject resolver, jstring key) {
        return hookedGetInt(env, nullptr, resolver, key, 0);
    }
};

REGISTER_ZYGISK_MODULE(HideDevOptions)
